"""Validate motion-source provenance without loading Unreal Engine."""

from __future__ import annotations

import hashlib
import io
import ipaddress
import json
import math
import os
import posixpath
import re
import struct
import sys
import unicodedata
from collections import Counter
from datetime import date
from pathlib import Path, PurePosixPath
from typing import Any, BinaryIO, Callable
from urllib.parse import unquote, urlsplit


PROJECT_ROOT = Path(__file__).absolute().parents[1]
PROJECT_ROOT_RESOLVED = PROJECT_ROOT.resolve()
REGISTRY_PATH = PROJECT_ROOT / "SourceArt/DiscGolf/Mocap/motion_source_registry.json"
REPORT_PATH = PROJECT_ROOT / "Saved/CharacterFramework/Session5MotionSourceRegistryValidation.json"
FROZEN_REGISTRY_BYTES = 22940
FROZEN_REGISTRY_SHA256 = "13301FF11E3E6024E1F64C1154D7AEBC232F71DECC7C143F12F319A929BD979E"
SHA256_RE = re.compile(r"^[0-9A-F]{64}$")
MD5_RE = re.compile(r"^[0-9a-f]{32}$")
SOURCE_ID_RE = re.compile(r"^[a-z0-9]+(?:_[a-z0-9]+)*$")
MOTION_ID_RE = re.compile(r"^[A-Z0-9]+(?:-[A-Z0-9]+)*$")
PIPELINE_FIXTURE_PATHS = {
    "raw_asset_path": "/Game/DiscGolf/Animation/Mocap/Source/A_DG_RHBH_SyntheticSource_RAW",
    "retargeted_asset_path": "/Game/DiscGolf/Animation/Mocap/Retargeted/A_DG_RHBH_Synthetic_RTG",
    "cleaned_asset_path": "/Game/DiscGolf/Animation/Mocap/Cleaned/A_DG_RHBH_Synthetic_CLN",
    "production_asset_path": "/Game/DiscGolf/Animation/Mocap/Production/A_DG_RHBH_SyntheticPipelineTest_v001",
}
PIPELINE_FIXTURE_MONTAGE = (
    "/Game/DiscGolf/Animation/Mocap/Production/AM_DG_RHBH_SyntheticPipelineTest_v001"
)
REQUIRED_FIELDS = {
    "source_id",
    "display_name",
    "motion_id",
    "source_filename",
    "creator_or_source",
    "acquisition_method",
    "license_permission_status",
    "original_frame_rate",
    "handedness",
    "throw_type",
    "import_date",
    "pipeline_role",
    "source_kind",
    "usage_status",
    "rights_status",
    "performer",
    "license_or_release",
    "external_production_claim",
    "rights_evidence",
    "content_assets",
    "files",
    "import_metadata",
    "raw_asset_path",
    "retargeted_asset_path",
    "cleaned_asset_path",
    "production_asset_path",
    "production_status",
    "restrictions",
}

INTAKE_SCHEMA = "disc_golf_motion_source_intake"
INTAKE_SCHEMA_VERSION = 1
PREFLIGHT_RESULT_SCHEMA = "disc_golf_motion_source_preflight_result"
PREFLIGHT_RESULT_SCHEMA_VERSION = 1
ACCEPTED_INTAKE_CLASSIFICATIONS = {
    "PROJECT_OWNED_CAPTURE",
    "VALID_COMMERCIAL_LICENSE",
    "WRITTEN_PERMISSION",
}
INTAKE_FIELDS = {
    "schema",
    "schema_version",
    "source_id",
    "motion_id",
    "take_id",
    "asset_revision",
    "handedness",
    "throw_type",
    "classification",
    "source_file",
    "rights_evidence_file",
    "provenance",
    "permissions",
    "performer_release",
    "reviews",
    "capture_assertions",
    "technical",
    "targets",
    "runtime_authority",
    "production_promotion_allowed",
}
FILE_RECORD_FIELDS = {"path", "bytes", "sha256"}
PROVENANCE_FIELDS = {
    "provider",
    "source_url",
    "creator",
    "acquisition_method",
    "acquired_on",
}
PERMISSION_FIELDS = {
    "commercial_interactive_use",
    "derivative_animation",
    "solver_output_use",
}
PERFORMER_RELEASE_FIELDS = {"performer_name", "verified", "evidence_file"}
REVIEW_FIELDS = {"rights", "throw_form"}
REVIEW_RECORD_FIELDS = {"reviewer", "disposition"}
CAPTURE_ASSERTION_FIELDS = {
    "authentic_disc_golf_throw",
    "full_body_visible",
    "both_feet_visible",
    "throwing_hand_visible",
    "disc_visible",
    "single_performer",
    "continuous_take",
    "no_cuts",
    "stationary_camera",
}
TECHNICAL_FIELDS = {
    "frame_rate_fps",
    "frame_rate_mode",
    "width_pixels",
    "height_pixels",
    "codec",
    "duration_seconds",
}
TARGET_FIELDS = {
    "raw_asset_path",
    "retargeted_asset_path",
    "cleaned_asset_path",
    "production_asset_path",
}
ACCEPTED_VIDEO_CODECS = {
    "H264",
    "H265",
    "PRORES_422",
    "PRORES_422_HQ",
}
ACCEPTED_VIDEO_EXTENSIONS = {".mov", ".mp4"}
MIN_SOURCE_DURATION_SECONDS = 1.0
MAX_SOURCE_DURATION_SECONDS = 30.0
MAX_SOURCE_ID_LENGTH = 64
MAX_MOTION_ID_LENGTH = 96
MAX_TAKE_ID_LENGTH = 64
MAX_UNREAL_PACKAGE_PATH_LENGTH = 200
MAX_UNREAL_OBJECT_PATH_LENGTH = 255
MEDIA_PROBE_IMPLEMENTATION = "STDLIB_ISO_BMFF_SAME_HANDLE"
MEDIA_PROBE_VERSION = 3
RIGHTS_REVIEW_DISPOSITION = "APPROVED_FOR_STAGING"
FORM_REVIEW_DISPOSITION = "AUTHENTIC_THROW_CONFIRMED"
RAW_ASSET_PREFIX = "/Game/DiscGolf/Animation/Mocap/Source/"
RETARGETED_ASSET_PREFIX = "/Game/DiscGolf/Animation/Mocap/Retargeted/"
TAKE_ID_RE = SOURCE_ID_RE
ASSET_REVISION_RE = re.compile(r"^v(?:00[1-9]|0[1-9][0-9]|[1-9][0-9]{2})$")
PLACEHOLDER_NORMALIZED_VALUES = {
    "dummy",
    "example",
    "fake",
    "fillme",
    "na",
    "none",
    "null",
    "pending",
    "placeholder",
    "redact",
    "redacted",
    "replace",
    "sample",
    "selftest",
    "synthetic",
    "tbd",
    "test",
    "todo",
    "unassigned",
    "unknown",
    "unset",
}
RESERVED_DNS_SUFFIXES = {
    "example",
    "example.com",
    "example.net",
    "example.org",
    "invalid",
    "local",
    "localhost",
    "test",
}
FROZEN_SYNTHETIC_TARGETS = {
    PIPELINE_FIXTURE_PATHS["raw_asset_path"].casefold(),
    PIPELINE_FIXTURE_PATHS["retargeted_asset_path"].casefold(),
}
TARGET_COLLISION_SUFFIXES = (".uasset", ".umap", ".uexp", ".ubulk", ".uptnl")
BMFF_VIDEO_CODEC_MAP = {
    b"avc1": "H264",
    b"avc3": "H264",
    b"hev1": "H265",
    b"hvc1": "H265",
    b"apcn": "PRORES_422",
    b"apch": "PRORES_422_HQ",
}
ACCEPTED_BMFF_BRANDS = {
    b"avc1",
    b"iso2",
    b"iso4",
    b"iso5",
    b"iso6",
    b"isom",
    b"mp41",
    b"mp42",
    b"qt  ",
}
DOS_DEVICE_BASENAME_RE = re.compile(
    r"^(?:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?$",
    re.IGNORECASE,
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def _repo_path(raw_path: Any) -> tuple[Path | None, str | None]:
    if not isinstance(raw_path, str) or not raw_path:
        return None, "file path must be a non-empty string"
    if "\\" in raw_path or re.match(r"^[A-Za-z]:", raw_path):
        return None, f"file path must be repo-relative POSIX form: {raw_path!r}"
    raw_segments = raw_path.split("/")
    for segment in raw_segments:
        if not segment or segment == ".":
            return None, f"file path must use non-empty canonical segments: {raw_path!r}"
        if segment == "..":
            return None, f"file path escapes the repository: {raw_path!r}"
        if (
            any(unicodedata.category(character) == "Cc" for character in segment)
            or any(character in '<>:"|?*' for character in segment)
            or segment.endswith((".", " "))
            or DOS_DEVICE_BASENAME_RE.fullmatch(segment) is not None
        ):
            return None, f"file path contains a nonportable or Win32-invalid segment: {raw_path!r}"
    posix_path = PurePosixPath(raw_path)
    if posix_path.is_absolute() or ".." in posix_path.parts:
        return None, f"file path escapes the repository: {raw_path!r}"
    try:
        resolved = PROJECT_ROOT.joinpath(*posix_path.parts).resolve()
    except OSError:
        return None, f"file path could not be resolved safely: {raw_path!r}"
    try:
        resolved.relative_to(PROJECT_ROOT_RESOLVED)
    except ValueError:
        return None, f"file path resolves outside the repository: {raw_path!r}"
    return resolved, None


def _validate_registry(registry: Any) -> tuple[list[str], list[dict[str, Any]]]:
    errors: list[str] = []
    verified_files: list[dict[str, Any]] = []
    if not isinstance(registry, dict):
        return ["registry root must be a JSON object"], verified_files
    if registry.get("schema") != "disc_golf_motion_source_registry":
        errors.append("unexpected or missing registry schema")
    if registry.get("schema_version") != 1:
        errors.append("schema_version must be 1")

    policy = registry.get("policy")
    if not isinstance(policy, dict):
        errors.append("policy must be an object")
        policy = {}
    if policy.get("external_production_sources_present") is not False:
        errors.append("policy must declare external_production_sources_present=false")
    if policy.get("shipping_requires_usage_status") != "CLEARED_FOR_PRODUCTION":
        errors.append("shipping policy must require CLEARED_FOR_PRODUCTION")
    if policy.get("unknown_rights_policy") != "DO_NOT_SHIP":
        errors.append("unknown rights must fail safe to DO_NOT_SHIP")

    sources = registry.get("sources")
    if not isinstance(sources, list) or not sources:
        return errors + ["sources must be a non-empty array"], verified_files

    seen_ids: set[str] = set()
    seen_motion_ids: set[str] = set()
    legacy_prototype_count = 0
    pipeline_fixture_count = 0
    for index, source in enumerate(sources):
        label = f"sources[{index}]"
        if not isinstance(source, dict):
            errors.append(f"{label} must be an object")
            continue
        source_id = source.get("source_id")
        if isinstance(source_id, str):
            label = source_id
        missing = sorted(REQUIRED_FIELDS - source.keys())
        if missing:
            errors.append(f"{label}: missing fields: {', '.join(missing)}")
        if not isinstance(source_id, str) or not SOURCE_ID_RE.fullmatch(source_id):
            errors.append(f"{label}: invalid source_id")
        elif source_id in seen_ids:
            errors.append(f"{label}: duplicate source_id")
        else:
            seen_ids.add(source_id)

        motion_id = source.get("motion_id")
        if not isinstance(motion_id, str) or not MOTION_ID_RE.fullmatch(motion_id):
            errors.append(f"{label}: invalid motion_id")
        elif motion_id in seen_motion_ids:
            errors.append(f"{label}: duplicate motion_id")
        else:
            seen_motion_ids.add(motion_id)

        source_filename = source.get("source_filename")
        if not isinstance(source_filename, str) or not source_filename or "/" in source_filename or "\\" in source_filename:
            errors.append(f"{label}: source_filename must be a filename, not a path")
        for field in (
            "creator_or_source",
            "acquisition_method",
            "license_permission_status",
            "handedness",
            "throw_type",
            "pipeline_role",
            "production_status",
        ):
            if not isinstance(source.get(field), str) or not source[field]:
                errors.append(f"{label}: {field} must be a non-empty string")
        frame_rate = source.get("original_frame_rate")
        if not (
            (isinstance(frame_rate, (int, float)) and not isinstance(frame_rate, bool) and frame_rate > 0)
            or frame_rate in {"UNKNOWN", "NOT_APPLICABLE"}
        ):
            errors.append(f"{label}: original_frame_rate must be positive, UNKNOWN, or NOT_APPLICABLE")
        import_date = source.get("import_date")
        try:
            if not isinstance(import_date, str) or date.fromisoformat(import_date).isoformat() != import_date:
                raise ValueError
        except ValueError:
            errors.append(f"{label}: import_date must be an ISO YYYY-MM-DD date")
        for stage_field in (
            "raw_asset_path",
            "retargeted_asset_path",
            "cleaned_asset_path",
            "production_asset_path",
        ):
            stage_path = source.get(stage_field)
            if stage_path is not None and not (
                isinstance(stage_path, str) and stage_path.startswith("/Game/")
            ):
                errors.append(f"{label}: {stage_field} must be null or a /Game path")

        if source.get("external_production_claim") is not False:
            errors.append(f"{label}: external_production_claim must remain false")
        content_assets = source.get("content_assets")
        if not isinstance(content_assets, list) or not content_assets or not all(
            isinstance(asset, str) and asset.startswith("/Game/") for asset in content_assets
        ):
            errors.append(f"{label}: content_assets must contain /Game paths")

        evidence = source.get("rights_evidence")
        if not isinstance(evidence, list) or not evidence:
            errors.append(f"{label}: rights_evidence must be non-empty")
        else:
            for evidence_index, item in enumerate(evidence):
                if not isinstance(item, dict) or not isinstance(item.get("type"), str) or not isinstance(
                    item.get("reference"), str
                ):
                    errors.append(f"{label}: rights_evidence[{evidence_index}] is incomplete")
                    continue
                if item["type"] in {"AUTHORING_SOURCE", "PROJECT_RECORD"}:
                    evidence_path, path_error = _repo_path(item["reference"])
                    if path_error:
                        errors.append(f"{label}: {path_error}")
                    elif evidence_path is not None and not evidence_path.is_file():
                        errors.append(f"{label}: missing evidence file {item['reference']}")
                elif item["type"] == "GIT_COMMIT" and not re.fullmatch(r"[0-9a-f]{40}", item["reference"]):
                    errors.append(f"{label}: invalid GIT_COMMIT evidence")

        restrictions = source.get("restrictions")
        if not isinstance(restrictions, list) or not restrictions or not all(
            isinstance(item, str) and item for item in restrictions
        ):
            errors.append(f"{label}: restrictions must be a non-empty string array")
            restrictions = []
        elif len(restrictions) != len(set(restrictions)):
            errors.append(f"{label}: restrictions contain duplicates")

        source_kind = source.get("source_kind")
        if source_kind == "SYNTHETIC_TEST":
            required = {
                "SYNTHETIC_TEST_ONLY",
                "DO_NOT_LABEL_AS_MOCAP",
                "DO_NOT_CLAIM_EXTERNAL_PERFORMANCE",
                "DO_NOT_SHIP",
            }
            if source.get("usage_status") != "DO_NOT_SHIP":
                errors.append(f"{label}: SYNTHETIC_TEST must be DO_NOT_SHIP")
            if source.get("rights_status") != "PROJECT_AUTHORED":
                errors.append(f"{label}: synthetic source must be PROJECT_AUTHORED")
            if source.get("performer") != "NOT_APPLICABLE":
                errors.append(f"{label}: synthetic source cannot claim a performer")
            if source.get("license_permission_status") != "PROJECT_OWNED_SYNTHETIC_TEST_ONLY":
                errors.append(f"{label}: synthetic license status must remain test-only")
            if source.get("pipeline_role") == "LEGACY_SYNTHETIC_PROTOTYPE":
                legacy_prototype_count += 1
                if any(source.get(field) is not None for field in PIPELINE_FIXTURE_PATHS):
                    errors.append(f"{label}: legacy prototype must not claim pipeline stages")
            elif source.get("pipeline_role") == "PIPELINE_FIXTURE":
                pipeline_fixture_count += 1
                for field, expected_path in PIPELINE_FIXTURE_PATHS.items():
                    if source.get(field) != expected_path:
                        errors.append(f"{label}: {field} must be {expected_path}")
                required_assets = set(PIPELINE_FIXTURE_PATHS.values()) | {PIPELINE_FIXTURE_MONTAGE}
                actual_assets = set(content_assets) if isinstance(content_assets, list) else set()
                if not required_assets.issubset(actual_assets):
                    errors.append(f"{label}: pipeline content_assets must contain all stage assets and montage")
                if "PIPELINE_FIXTURE_ONLY" not in restrictions:
                    errors.append(f"{label}: pipeline fixture restriction is missing")
            else:
                errors.append(f"{label}: synthetic row has an unsupported pipeline_role")
            if source.get("production_status") != "SYNTHETIC_TEST_DO_NOT_SHIP":
                errors.append(f"{label}: synthetic production status must be DO_NOT_SHIP")
            if source.get("original_frame_rate") != 60 or source.get("handedness") != "RIGHT" or source.get("throw_type") != "RHBH":
                errors.append(f"{label}: synthetic motion metadata must remain 60 FPS RIGHT RHBH")
            if not required.issubset(restrictions):
                errors.append(f"{label}: synthetic restrictions are incomplete")
        elif source_kind == "VENDOR_DEMO_ANIMATION":
            required = {
                "DO_NOT_REPURPOSE",
                "DO_NOT_SHIP",
                "LICENSE_UNVERIFIED",
                "VENDOR_DEMO_CONTENT",
                "NOT_DISC_GOLF_THROW",
                "MISSING_ORIGINAL_SOURCE",
            }
            if source.get("usage_status") != "DO_NOT_REPURPOSE":
                errors.append(f"{label}: vendor demo must be DO_NOT_REPURPOSE")
            if source.get("rights_status") != "UNVERIFIED" or source.get("license_or_release") != "MISSING":
                errors.append(f"{label}: vendor rights must remain explicitly unverified")
            if source.get("license_permission_status") != "UNKNOWN_RECEIPT_PENDING":
                errors.append(f"{label}: vendor license permission must remain unknown")
            if source.get("pipeline_role") != "UNUSABLE_VENDOR_DEMO":
                errors.append(f"{label}: vendor row must identify the unusable demo role")
            if (
                not isinstance(source.get("raw_asset_path"), str)
                or source.get("retargeted_asset_path") is not None
                or source.get("cleaned_asset_path") is not None
                or source.get("production_asset_path") is not None
            ):
                errors.append(f"{label}: vendor row may record only its raw package path")
            if source.get("production_status") != "DO_NOT_SHIP_DO_NOT_REPURPOSE":
                errors.append(f"{label}: vendor production status must prohibit shipping and repurposing")
            if source.get("original_frame_rate") != "UNKNOWN" or source.get("handedness") != "NOT_APPLICABLE" or source.get("throw_type") != "NOT_DISC_GOLF_THROW":
                errors.append(f"{label}: vendor motion metadata must remain explicitly unknown/not applicable")
            if not required.issubset(restrictions):
                errors.append(f"{label}: vendor restrictions are incomplete")
            metadata = source.get("import_metadata")
            if not isinstance(metadata, dict):
                errors.append(f"{label}: import_metadata must be an object")
            else:
                if metadata.get("source_file_present") is not False:
                    errors.append(f"{label}: missing vendor source must not be marked present")
                if not isinstance(metadata.get("source_path_recorded_in_asset"), str):
                    errors.append(f"{label}: recorded vendor FBX path is required")
                if not isinstance(metadata.get("source_timestamp_unix"), int):
                    errors.append(f"{label}: source timestamp is required")
                if not isinstance(metadata.get("source_md5"), str) or not MD5_RE.fullmatch(metadata["source_md5"]):
                    errors.append(f"{label}: source_md5 must be lowercase MD5 metadata")
        else:
            errors.append(f"{label}: unsupported source_kind {source_kind!r}")

        files = source.get("files")
        if not isinstance(files, list) or not files:
            errors.append(f"{label}: files must be non-empty")
            continue
        for file_index, item in enumerate(files):
            if not isinstance(item, dict):
                errors.append(f"{label}: files[{file_index}] must be an object")
                continue
            if not {"path", "role", "bytes", "sha256"}.issubset(item):
                errors.append(f"{label}: files[{file_index}] is incomplete")
                continue
            local_path, path_error = _repo_path(item["path"])
            if path_error:
                errors.append(f"{label}: {path_error}")
                continue
            if local_path is None or not local_path.is_file():
                errors.append(f"{label}: missing local file {item['path']}")
                continue
            actual_size = local_path.stat().st_size
            actual_hash = _sha256(local_path)
            if not isinstance(item["bytes"], int) or item["bytes"] != actual_size:
                errors.append(f"{label}: size mismatch for {item['path']}")
            if not isinstance(item["sha256"], str) or not SHA256_RE.fullmatch(item["sha256"]):
                errors.append(f"{label}: invalid SHA-256 format for {item['path']}")
            elif item["sha256"] != actual_hash:
                errors.append(f"{label}: SHA-256 mismatch for {item['path']}")
            verified_files.append(
                {
                    "source_id": source_id,
                    "path": item["path"],
                    "bytes": actual_size,
                    "sha256": actual_hash,
                }
            )

    if legacy_prototype_count != 1:
        errors.append(f"registry must contain exactly one legacy synthetic prototype; found {legacy_prototype_count}")
    if pipeline_fixture_count != 1:
        errors.append(f"registry must contain exactly one PIPELINE_FIXTURE row; found {pipeline_fixture_count}")
    return errors, verified_files


def _require_exact_fields(
    value: Any,
    expected: set[str],
    label: str,
    errors: list[str],
) -> dict[str, Any] | None:
    if not isinstance(value, dict):
        errors.append(f"{label} must be an object")
        return None
    actual = set(value)
    missing = sorted(expected - actual)
    unexpected = sorted(actual - expected)
    if missing:
        errors.append(f"{label} missing fields: {', '.join(missing)}")
    if unexpected:
        errors.append(f"{label} has unexpected fields: {', '.join(unexpected)}")
    return value


def _alphanumeric_normalized(value: str) -> str:
    decomposed = unicodedata.normalize("NFKD", value).casefold()
    return "".join(character for character in decomposed if character.isalnum())


def _is_named_value(value: Any) -> bool:
    if not isinstance(value, str) or not value.strip():
        return False
    normalized = _alphanumeric_normalized(value)
    if not normalized or normalized in PLACEHOLDER_NORMALIZED_VALUES:
        return False
    decomposed = unicodedata.normalize("NFKD", value).casefold()
    tokens = {
        _alphanumeric_normalized(token)
        for token in re.split(r"[\W_]+", decomposed)
        if token
    }
    if tokens & PLACEHOLDER_NORMALIZED_VALUES:
        return False
    return not any(
        marker in normalized
        for marker in (
            "changeme",
            "dummy",
            "example",
            "insertname",
            "placeholder",
            "redactme",
            "redacted",
            "replaceme",
            "replacewith",
            "selftest",
            "synthetic",
            "yourname",
        )
    )


def _contains_pexels(value: Any) -> bool:
    if not isinstance(value, str):
        return False
    decoded = unquote(value)
    return "pexels" in _alphanumeric_normalized(decoded)


def _decoded_url_layers(value: str, max_decodes: int = 3) -> tuple[list[str], str | None]:
    layers = [value]
    current = value
    stability_error: str | None = None
    for depth in range(max_decodes):
        if "%" not in current:
            if len(layers) > 2 and stability_error is None:
                stability_error = "contains nested percent encoding"
            return layers, stability_error
        without_escapes = re.sub(r"%[0-9A-Fa-f]{2}", "", current)
        if "%" in without_escapes:
            return layers, "contains malformed percent encoding"
        if depth == 0:
            escapes = re.findall(r"%([0-9A-Fa-f]{2})", current)
            if any(escape != escape.upper() for escape in escapes) and stability_error is None:
                stability_error = "percent escapes must use uppercase hexadecimal"
            unstable_ascii = {
                byte
                for byte in range(128)
                if chr(byte).isalnum() or chr(byte) in "-._~/\\%"
            }
            if (
                any(int(escape, 16) in unstable_ascii for escape in escapes)
                and stability_error is None
            ):
                stability_error = "contains unstable percent encoding"
        try:
            decoded = unquote(current, errors="strict")
        except UnicodeDecodeError:
            return layers, "contains invalid UTF-8 percent encoding"
        if decoded == current:
            return layers, stability_error or "contains unstable percent encoding"
        layers.append(decoded)
        current = decoded
    if "%" in current:
        return layers, "contains excessive nested percent encoding"
    if len(layers) > 2 and stability_error is None:
        stability_error = "contains nested percent encoding"
    return layers, stability_error


def _canonical_https_dns_form_url_error(value: Any) -> str | None:
    if not isinstance(value, str) or not value:
        return "must be a non-empty canonical HTTPS DNS-form URL"
    layers, decoding_error = _decoded_url_layers(value)
    if any("pexels" in _alphanumeric_normalized(layer) for layer in layers):
        return "must not reference PEXELS at any encoding layer"
    if decoding_error:
        return decoding_error
    try:
        parsed = urlsplit(value)
        host = parsed.hostname
        port = parsed.port
    except (UnicodeError, ValueError):
        return "must be a non-empty canonical HTTPS DNS-form URL"
    if (
        not value.startswith("https://")
        or parsed.scheme != "https"
        or not host
        or parsed.username is not None
        or parsed.password is not None
        or port is not None
        or "\\" in value
        or "%" in parsed.netloc
        or any(character.isspace() for character in value)
        or parsed.fragment
    ):
        return "must be a canonical lowercase HTTPS DNS-form URL without credentials, port, or fragment"
    if value != value.strip() or host.endswith("."):
        return "must use canonical HTTPS DNS-form URL text"
    try:
        ipaddress.ip_address(host)
    except ValueError:
        pass
    else:
        return "must use DNS-form hostname syntax, not an IP address"
    try:
        ascii_host = host.encode("idna").decode("ascii").casefold()
    except UnicodeError:
        return "contains an invalid DNS hostname"
    if parsed.netloc != ascii_host:
        return "scheme and DNS hostname must be lowercase canonical ASCII without explicit :443"
    if len(ascii_host) > 253:
        return "contains an invalid DNS hostname"
    labels = ascii_host.split(".")
    if len(labels) < 2 or any(
        not label
        or len(label) > 63
        or re.fullmatch(r"[a-z0-9](?:[a-z0-9-]*[a-z0-9])?", label) is None
        for label in labels
    ):
        return "contains an invalid DNS hostname"
    if any(ascii_host == suffix or ascii_host.endswith("." + suffix) for suffix in RESERVED_DNS_SUFFIXES):
        return "must not use placeholder, test, local, invalid, or example DNS"
    top_level = labels[-1]
    if not (re.fullmatch(r"[a-z]{2,63}", top_level) or top_level.startswith("xn--")):
        return "must use plausible-public DNS-form top-level syntax"
    decoded_path = urlsplit(layers[-1]).path
    for path in (parsed.path, decoded_path):
        if path and (
            not path.startswith("/")
            or "//" in path
            or posixpath.normpath(path) != path
            or any(segment in {".", ".."} for segment in path.split("/"))
        ):
            return "path must be canonical and contain no empty or dot-segments"
    return None


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _is_finite_number(value: Any) -> bool:
    return _is_number(value) and (not isinstance(value, float) or math.isfinite(value))


def _read_exact_at(stream: BinaryIO, offset: int, size: int) -> bytes:
    if offset < 0 or size < 0:
        raise ValueError("negative BMFF read")
    stream.seek(offset)
    payload = stream.read(size)
    if len(payload) != size:
        raise ValueError("truncated BMFF box")
    return payload


def _bmff_boxes(
    stream: BinaryIO,
    start: int,
    end: int,
    *,
    allow_exact_four_zero_padding: bool = False,
    allow_zero_sized_boxes: bool = True,
) -> list[tuple[bytes, int, int]]:
    if start < 0 or end < start:
        raise ValueError("invalid BMFF bounds")
    boxes: list[tuple[bytes, int, int]] = []
    offset = start
    while offset < end:
        if allow_exact_four_zero_padding and end - offset == 4:
            if _read_exact_at(stream, offset, 4) != b"\x00\x00\x00\x00":
                raise ValueError("nonzero trailing bytes outside a BMFF box")
            offset = end
            break
        if end - offset < 8:
            raise ValueError("trailing bytes outside a BMFF box")
        size32, box_type = struct.unpack(">I4s", _read_exact_at(stream, offset, 8))
        header_size = 8
        if size32 == 1:
            if end - offset < 16:
                raise ValueError("truncated extended BMFF box")
            box_size = struct.unpack(">Q", _read_exact_at(stream, offset + 8, 8))[0]
            header_size = 16
        elif size32 == 0:
            if not allow_zero_sized_boxes:
                raise ValueError("zero-sized BMFF child box is unsupported")
            box_size = end - offset
        else:
            box_size = size32
        if box_size < header_size or offset + box_size > end:
            raise ValueError("invalid BMFF box size")
        box_end = offset + box_size
        boxes.append((box_type, offset + header_size, box_end))
        if len(boxes) > 10000:
            raise ValueError("BMFF container has an unreasonable box count")
        offset = box_end
    return boxes


def _one_bmff_box(
    boxes: list[tuple[bytes, int, int]],
    box_type: bytes,
    label: str,
) -> tuple[bytes, int, int]:
    matches = [box for box in boxes if box[0] == box_type]
    if len(matches) != 1:
        raise ValueError(f"BMFF {label} must contain exactly one {box_type.decode('ascii')} box")
    return matches[0]


def _parse_bmff_handler(stream: BinaryIO, start: int, end: int) -> bytes:
    if end - start < 12:
        raise ValueError("truncated BMFF hdlr")
    payload = _read_exact_at(stream, start, 12)
    if payload[0] != 0 or payload[1:4] != b"\x00\x00\x00":
        raise ValueError("unsupported BMFF hdlr version or flags")
    return payload[8:12]


def _parse_bmff_mdhd(stream: BinaryIO, start: int, end: int) -> tuple[int, int]:
    if end - start < 4:
        raise ValueError("truncated BMFF mdhd")
    version_flags = _read_exact_at(stream, start, 4)
    version = version_flags[0]
    if version_flags[1:4] != b"\x00\x00\x00":
        raise ValueError("unsupported BMFF mdhd flags")
    if version == 0:
        if end - start < 24:
            raise ValueError("truncated BMFF version-0 mdhd")
        timescale, duration = struct.unpack(">II", _read_exact_at(stream, start + 12, 8))
    elif version == 1:
        if end - start < 36:
            raise ValueError("truncated BMFF version-1 mdhd")
        timescale = struct.unpack(">I", _read_exact_at(stream, start + 20, 4))[0]
        duration = struct.unpack(">Q", _read_exact_at(stream, start + 24, 8))[0]
    else:
        raise ValueError("unsupported BMFF mdhd version")
    if timescale <= 0 or duration <= 0:
        raise ValueError("BMFF mdhd must declare positive timescale and duration")
    return timescale, duration


def _validate_avcc(payload: bytes) -> None:
    if (
        len(payload) < 7
        or payload[0] != 1
        or payload[4] & 0xFC != 0xFC
        or (payload[4] & 3) != 3
        or payload[5] & 0xE0 != 0xE0
    ):
        raise ValueError("BMFF avcC payload is malformed or unsupported")
    cursor = 6
    sps_count = payload[5] & 31
    if sps_count <= 0:
        raise ValueError("BMFF avcC must contain an SPS")
    for _ in range(sps_count):
        if cursor + 2 > len(payload):
            raise ValueError("BMFF avcC SPS table is truncated")
        size = struct.unpack(">H", payload[cursor:cursor + 2])[0]
        cursor += 2
        if size <= 0 or cursor + size > len(payload):
            raise ValueError("BMFF avcC SPS is invalid")
        cursor += size
    if cursor >= len(payload):
        raise ValueError("BMFF avcC PPS table is missing")
    pps_count = payload[cursor]
    cursor += 1
    if pps_count <= 0:
        raise ValueError("BMFF avcC must contain a PPS")
    for _ in range(pps_count):
        if cursor + 2 > len(payload):
            raise ValueError("BMFF avcC PPS table is truncated")
        size = struct.unpack(">H", payload[cursor:cursor + 2])[0]
        cursor += 2
        if size <= 0 or cursor + size > len(payload):
            raise ValueError("BMFF avcC PPS is invalid")
        cursor += size
    high_profiles = {44, 83, 86, 100, 110, 118, 122, 128, 134, 135, 138, 139, 144, 244}
    if cursor < len(payload):
        if payload[1] not in high_profiles or cursor + 4 > len(payload):
            raise ValueError("BMFF avcC contains unsupported trailing data")
        if (
            payload[cursor] & 0xFC != 0xFC
            or payload[cursor + 1] & 0xF8 != 0xF8
            or payload[cursor + 2] & 0xF8 != 0xF8
        ):
            raise ValueError("BMFF avcC high-profile extension has invalid reserved bits")
        extension_count = payload[cursor + 3]
        cursor += 4
        for _ in range(extension_count):
            if cursor + 2 > len(payload):
                raise ValueError("BMFF avcC SPS-extension table is truncated")
            size = struct.unpack(">H", payload[cursor:cursor + 2])[0]
            cursor += 2
            if size <= 0 or cursor + size > len(payload):
                raise ValueError("BMFF avcC SPS-extension is invalid")
            cursor += size
    if cursor != len(payload):
        raise ValueError("BMFF avcC contains unsupported trailing data")


def _validate_hvcc(payload: bytes) -> None:
    if len(payload) < 23 or payload[0] != 1 or (payload[21] & 3) != 3:
        raise ValueError("BMFF hvcC payload is malformed or unsupported")
    array_count = payload[22]
    cursor = 23
    nal_types: set[int] = set()
    if array_count <= 0:
        raise ValueError("BMFF hvcC contains no parameter arrays")
    for _ in range(array_count):
        if cursor + 3 > len(payload):
            raise ValueError("BMFF hvcC array table is truncated")
        nal_type = payload[cursor] & 63
        unit_count = struct.unpack(">H", payload[cursor + 1:cursor + 3])[0]
        cursor += 3
        if unit_count <= 0:
            raise ValueError("BMFF hvcC parameter array is empty")
        nal_types.add(nal_type)
        for _ in range(unit_count):
            if cursor + 2 > len(payload):
                raise ValueError("BMFF hvcC NAL table is truncated")
            size = struct.unpack(">H", payload[cursor:cursor + 2])[0]
            cursor += 2
            if size <= 0 or cursor + size > len(payload):
                raise ValueError("BMFF hvcC NAL unit is invalid")
            cursor += size
    if cursor != len(payload) or not {32, 33, 34}.issubset(nal_types):
        raise ValueError("BMFF hvcC must exactly contain valid VPS, SPS, and PPS data")


def _parse_bmff_stsd(stream: BinaryIO, start: int, end: int) -> tuple[str, int, int]:
    if end - start < 8:
        raise ValueError("truncated BMFF stsd")
    header = _read_exact_at(stream, start, 8)
    if header[:4] != b"\x00\x00\x00\x00" or struct.unpack(">I", header[4:8])[0] != 1:
        raise ValueError("BMFF stsd must contain exactly one version-0 sample entry")
    entry_start = start + 8
    if end - entry_start < 86:
        raise ValueError("truncated BMFF visual sample entry")
    entry_size, entry_type = struct.unpack(">I4s", _read_exact_at(stream, entry_start, 8))
    if entry_size < 86 or entry_start + entry_size != end:
        raise ValueError("BMFF stsd sample entry has invalid size")
    codec = BMFF_VIDEO_CODEC_MAP.get(entry_type)
    if codec is None:
        try:
            label = entry_type.decode("ascii")
        except UnicodeDecodeError:
            label = entry_type.hex().upper()
        raise ValueError(f"unsupported BMFF video codec sample entry {label}")
    width, height = struct.unpack(">HH", _read_exact_at(stream, entry_start + 32, 4))
    if width <= 0 or height <= 0:
        raise ValueError("BMFF visual sample entry has invalid dimensions")
    required_configuration = {
        b"avc1": b"avcC", b"avc3": b"avcC",
        b"hvc1": b"hvcC", b"hev1": b"hvcC",
    }.get(entry_type)
    if required_configuration is None:
        children = _bmff_boxes(stream, entry_start + 86, entry_start + entry_size)
    else:
        children = _bmff_boxes(
            stream,
            entry_start + 86,
            entry_start + entry_size,
            allow_exact_four_zero_padding=True,
            allow_zero_sized_boxes=False,
        )
    if required_configuration is not None:
        configuration = _one_bmff_box(children, required_configuration, "video sample entry")
        payload = _read_exact_at(stream, configuration[1], configuration[2] - configuration[1])
        if required_configuration == b"avcC":
            _validate_avcc(payload)
        else:
            _validate_hvcc(payload)
    return codec, width, height


def _parse_bmff_stts(stream: BinaryIO, start: int, end: int) -> tuple[int, int, set[int]]:
    if end - start < 8:
        raise ValueError("truncated BMFF stts")
    header = _read_exact_at(stream, start, 8)
    if header[:4] != b"\x00\x00\x00\x00":
        raise ValueError("unsupported BMFF stts version or flags")
    entry_count = struct.unpack(">I", header[4:8])[0]
    if entry_count <= 0 or entry_count > 1000000 or end - start != 8 + entry_count * 8:
        raise ValueError("BMFF stts has invalid entry count or size")
    total_samples = 0
    total_ticks = 0
    deltas: set[int] = set()
    for index in range(entry_count):
        sample_count, sample_delta = struct.unpack(
            ">II", _read_exact_at(stream, start + 8 + index * 8, 8)
        )
        if sample_count <= 0 or sample_delta <= 0:
            raise ValueError("BMFF stts entries must be positive")
        total_samples += sample_count
        total_ticks += sample_count * sample_delta
        deltas.add(sample_delta)
    if total_samples <= 0 or total_ticks <= 0:
        raise ValueError("BMFF stts contains no timed samples")
    return total_samples, total_ticks, deltas


def _parse_bmff_stsz(stream: BinaryIO, start: int, end: int) -> tuple[int, list[int]]:
    if end - start < 12:
        raise ValueError("truncated BMFF stsz")
    header = _read_exact_at(stream, start, 12)
    if header[:4] != b"\x00\x00\x00\x00":
        raise ValueError("unsupported BMFF stsz version or flags")
    sample_size, sample_count = struct.unpack(">II", header[4:12])
    expected_size = 12 if sample_size else 12 + sample_count * 4
    if sample_count <= 0 or sample_count > 1000000 or end - start != expected_size:
        raise ValueError("BMFF stsz has invalid sample count or size")
    if sample_size:
        sample_sizes = [sample_size] * sample_count
    else:
        sample_sizes = []
        for index in range(sample_count):
            variable_size = struct.unpack(
                ">I", _read_exact_at(stream, start + 12 + index * 4, 4)
            )[0]
            if variable_size <= 0:
                raise ValueError("BMFF variable stsz entries must be positive")
            sample_sizes.append(variable_size)
    return sample_count, sample_sizes


def _parse_bmff_chunk_offsets(
    stream: BinaryIO,
    start: int,
    end: int,
    wide: bool,
) -> list[int]:
    if end - start < 8:
        raise ValueError("truncated BMFF chunk-offset table")
    header = _read_exact_at(stream, start, 8)
    if header[:4] != b"\x00\x00\x00\x00":
        raise ValueError("unsupported BMFF chunk-offset version or flags")
    entry_count = struct.unpack(">I", header[4:8])[0]
    width = 8 if wide else 4
    if entry_count <= 0 or entry_count > 1000000 or end - start != 8 + entry_count * width:
        raise ValueError("BMFF chunk-offset table has invalid count or size")
    format_code = ">Q" if wide else ">I"
    offsets = [
        struct.unpack(
            format_code,
            _read_exact_at(stream, start + 8 + index * width, width),
        )[0]
        for index in range(entry_count)
    ]
    if len(offsets) != len(set(offsets)):
        raise ValueError("BMFF chunk offsets contain duplicates")
    return offsets


def _parse_bmff_stsc(
    stream: BinaryIO,
    start: int,
    end: int,
    chunk_count: int,
) -> list[int]:
    if end - start < 8:
        raise ValueError("truncated BMFF stsc")
    header = _read_exact_at(stream, start, 8)
    if header[:4] != b"\x00\x00\x00\x00":
        raise ValueError("unsupported BMFF stsc version or flags")
    entry_count = struct.unpack(">I", header[4:8])[0]
    if entry_count <= 0 or entry_count > 1000000 or end - start != 8 + entry_count * 12:
        raise ValueError("BMFF stsc has invalid entry count or size")
    entries: list[tuple[int, int, int]] = []
    for index in range(entry_count):
        entry = struct.unpack(">III", _read_exact_at(stream, start + 8 + index * 12, 12))
        first_chunk, samples_per_chunk, description_index = entry
        if (
            first_chunk <= 0
            or samples_per_chunk <= 0
            or description_index != 1
            or (entries and first_chunk <= entries[-1][0])
        ):
            raise ValueError("BMFF stsc entries are invalid")
        entries.append(entry)
    if entries[0][0] != 1 or entries[-1][0] > chunk_count:
        raise ValueError("BMFF stsc does not cover the chunk table")
    samples_per_chunk_table: list[int] = []
    for index, (first_chunk, samples_per_chunk, _) in enumerate(entries):
        next_first = entries[index + 1][0] if index + 1 < len(entries) else chunk_count + 1
        samples_per_chunk_table.extend([samples_per_chunk] * (next_first - first_chunk))
    if len(samples_per_chunk_table) != chunk_count:
        raise ValueError("BMFF stsc does not map every chunk exactly once")
    return samples_per_chunk_table


def _probe_bmff_stream(stream: BinaryIO) -> dict[str, Any]:
    stream.seek(0, os.SEEK_END)
    file_size = stream.tell()
    if file_size < 24:
        raise ValueError("source is too small to be a valid ISO-BMFF video")
    top_level = _bmff_boxes(stream, 0, file_size)
    ftyp = _one_bmff_box(top_level, b"ftyp", "file")
    ftyp_payload = _read_exact_at(stream, ftyp[1], ftyp[2] - ftyp[1])
    if len(ftyp_payload) < 8 or (len(ftyp_payload) - 8) % 4:
        raise ValueError("BMFF ftyp is incomplete or malformed")
    major_brand = ftyp_payload[:4]
    compatible_brands = {ftyp_payload[index:index + 4] for index in range(8, len(ftyp_payload), 4)}
    if major_brand not in ACCEPTED_BMFF_BRANDS:
        raise ValueError("BMFF ftyp major brand is not an accepted MOV/MP4 brand")
    if compatible_brands and not compatible_brands.intersection(ACCEPTED_BMFF_BRANDS):
        raise ValueError("BMFF ftyp has no accepted compatible MOV/MP4 brand")
    moov = _one_bmff_box(top_level, b"moov", "file")
    media_boxes = [box for box in top_level if box[0] == b"mdat"]
    if not media_boxes or sum(box[2] - box[1] for box in media_boxes) <= 0:
        raise ValueError("BMFF file contains no media payload")

    video_facts: list[dict[str, Any]] = []
    moov_children = _bmff_boxes(stream, moov[1], moov[2])
    for track in [box for box in moov_children if box[0] == b"trak"]:
        track_children = _bmff_boxes(stream, track[1], track[2])
        mdia = _one_bmff_box(track_children, b"mdia", "trak")
        mdia_children = _bmff_boxes(stream, mdia[1], mdia[2])
        handler = _one_bmff_box(mdia_children, b"hdlr", "mdia")
        if _parse_bmff_handler(stream, handler[1], handler[2]) != b"vide":
            continue
        mdhd = _one_bmff_box(mdia_children, b"mdhd", "video mdia")
        timescale, media_duration_ticks = _parse_bmff_mdhd(stream, mdhd[1], mdhd[2])
        minf = _one_bmff_box(mdia_children, b"minf", "video mdia")
        minf_children = _bmff_boxes(stream, minf[1], minf[2])
        stbl = _one_bmff_box(minf_children, b"stbl", "video minf")
        sample_boxes = _bmff_boxes(stream, stbl[1], stbl[2])
        stsd = _one_bmff_box(sample_boxes, b"stsd", "video stbl")
        codec, width, height = _parse_bmff_stsd(stream, stsd[1], stsd[2])
        stts = _one_bmff_box(sample_boxes, b"stts", "video stbl")
        sample_count, total_ticks, deltas = _parse_bmff_stts(stream, stts[1], stts[2])
        stsz = _one_bmff_box(sample_boxes, b"stsz", "video stbl")
        sized_sample_count, sample_sizes = _parse_bmff_stsz(stream, stsz[1], stsz[2])
        if sized_sample_count != sample_count:
            raise ValueError("BMFF stts/stsz sample counts differ")
        chunk_tables = [box for box in sample_boxes if box[0] in {b"stco", b"co64"}]
        if len(chunk_tables) != 1:
            raise ValueError("BMFF video stbl must contain exactly one stco or co64 table")
        chunk_table = chunk_tables[0]
        chunk_offsets = _parse_bmff_chunk_offsets(
            stream,
            chunk_table[1],
            chunk_table[2],
            chunk_table[0] == b"co64",
        )
        if not all(
            any(media_start <= offset < media_end for _, media_start, media_end in media_boxes)
            for offset in chunk_offsets
        ):
            raise ValueError("BMFF video chunk offset points outside mdat payload")
        stsc = _one_bmff_box(sample_boxes, b"stsc", "video stbl")
        samples_per_chunk = _parse_bmff_stsc(
            stream, stsc[1], stsc[2], len(chunk_offsets)
        )
        if sum(samples_per_chunk) != sample_count:
            raise ValueError("BMFF stsc does not cover the declared sample count")
        chunk_extents: list[tuple[int, int]] = []
        sample_cursor = 0
        for chunk_offset, chunk_sample_count in zip(chunk_offsets, samples_per_chunk):
            next_cursor = sample_cursor + chunk_sample_count
            if next_cursor > len(sample_sizes):
                raise ValueError("BMFF stsc consumes more samples than stsz declares")
            chunk_bytes = sum(sample_sizes[sample_cursor:next_cursor])
            chunk_end = chunk_offset + chunk_bytes
            if not any(
                media_start <= chunk_offset < chunk_end <= media_end
                for _, media_start, media_end in media_boxes
            ):
                raise ValueError("BMFF video chunk byte extent is not wholly inside one mdat")
            chunk_extents.append((chunk_offset, chunk_end))
            sample_cursor = next_cursor
        if sample_cursor != len(sample_sizes):
            raise ValueError("BMFF stsc/stsz sample coverage is incomplete")
        ordered_extents = sorted(chunk_extents)
        if any(
            left_end > right_start
            for (_, left_end), (right_start, _) in zip(ordered_extents, ordered_extents[1:])
        ):
            raise ValueError("BMFF video chunk byte extents overlap")
        largest_delta = max(deltas)
        if abs(media_duration_ticks - total_ticks) > largest_delta:
            raise ValueError("BMFF media duration and sample timing differ by more than one frame")
        duration = total_ticks / timescale
        frame_rate = sample_count / duration
        if not math.isfinite(duration) or not math.isfinite(frame_rate):
            raise ValueError("BMFF timing is not finite")
        video_facts.append(
            {
                "codec": codec,
                "width_pixels": width,
                "height_pixels": height,
                "frame_rate_fps": frame_rate,
                "frame_rate_mode": "CONSTANT" if len(deltas) == 1 else "VARIABLE",
                "duration_seconds": duration,
            }
        )
    if len(video_facts) != 1:
        raise ValueError(f"source must contain exactly one video track; found {len(video_facts)}")
    return video_facts[0]


def _hash_open_stream(stream: BinaryIO) -> tuple[int, str]:
    stream.seek(0)
    digest = hashlib.sha256()
    byte_count = 0
    for block in iter(lambda: stream.read(1024 * 1024), b""):
        byte_count += len(block)
        digest.update(block)
    return byte_count, digest.hexdigest().upper()


def _stat_identity(value: os.stat_result) -> tuple[int, int, int, int, int]:
    return (
        value.st_dev,
        value.st_ino,
        value.st_size,
        value.st_mtime_ns,
        value.st_ctime_ns,
    )


SourceVerifier = Callable[[str, Path], dict[str, Any]]
SOURCE_VERIFICATION_FIELDS = {
    "bytes",
    "sha256",
    "probe_input_sha256",
    "stable_rehash_sha256",
    "handle_stable",
    "path_identity_stable",
    "media",
    "probe_implementation",
    "probe_version",
}


def _verify_source_same_handle(relative_path: str, local_path: Path) -> dict[str, Any]:
    if PurePosixPath(relative_path).suffix.casefold() not in ACCEPTED_VIDEO_EXTENSIONS:
        raise ValueError("source_file must use .mov or .mp4 ISO-BMFF media")
    with local_path.open("rb") as stream:
        before = os.fstat(stream.fileno())
        byte_count, first_hash = _hash_open_stream(stream)
        stream.seek(0)
        media = _probe_bmff_stream(stream)
        stable_byte_count, stable_hash = _hash_open_stream(stream)
        after = os.fstat(stream.fileno())
        path_after = os.stat(local_path, follow_symlinks=True)
    handle_stable = (
        _stat_identity(before) == _stat_identity(after)
        and byte_count == stable_byte_count
        and byte_count == before.st_size
    )
    path_identity_stable = _stat_identity(after) == _stat_identity(path_after)
    return {
        "bytes": byte_count,
        "sha256": first_hash,
        "probe_input_sha256": stable_hash,
        "stable_rehash_sha256": stable_hash,
        "handle_stable": handle_stable,
        "path_identity_stable": path_identity_stable,
        "media": media,
        "probe_implementation": MEDIA_PROBE_IMPLEMENTATION,
        "probe_version": MEDIA_PROBE_VERSION,
    }


def _intake_repo_file_path(raw_path: Any, motion_id: Any) -> tuple[Path | None, str | None]:
    local_path, path_error = _repo_path(raw_path)
    if path_error:
        return None, path_error
    if raw_path != PurePosixPath(raw_path).as_posix():
        return None, f"file path must be canonical repo-relative POSIX text: {raw_path!r}"
    if not isinstance(motion_id, str):
        return None, "motion_id must be valid before file paths can be checked"
    parts = PurePosixPath(raw_path).parts
    required_prefix = ("SourceArt", "DiscGolf", "Mocap", motion_id)
    if len(parts) <= len(required_prefix) or tuple(parts[: len(required_prefix)]) != required_prefix:
        return None, (
            "file path must be below exact motion root "
            f"SourceArt/DiscGolf/Mocap/{motion_id}/: {raw_path!r}"
        )
    return local_path, None


IntakeFileReader = Callable[[str, Path], tuple[int, str]]


def _read_intake_file(relative_path: str, local_path: Path) -> tuple[int, str]:
    del relative_path
    digest = hashlib.sha256()
    byte_count = 0
    with local_path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            byte_count += len(block)
            digest.update(block)
    return byte_count, digest.hexdigest().upper()


def _validate_intake_file_record(
    value: Any,
    label: str,
    motion_id: Any,
    file_reader: IntakeFileReader,
    errors: list[str],
    verified_files: list[dict[str, Any]],
) -> tuple[str, Path] | None:
    record = _require_exact_fields(value, FILE_RECORD_FIELDS, label, errors)
    if record is None:
        return None
    raw_path = record.get("path")
    local_path, path_error = _intake_repo_file_path(raw_path, motion_id)
    if path_error:
        errors.append(f"{label}: {path_error}")

    expected_size = record.get("bytes")
    size_is_valid = (
        isinstance(expected_size, int)
        and not isinstance(expected_size, bool)
        and expected_size > 0
    )
    if not size_is_valid:
        errors.append(f"{label}.bytes must be a positive integer")
    expected_hash = record.get("sha256")
    hash_is_valid = isinstance(expected_hash, str) and SHA256_RE.fullmatch(expected_hash) is not None
    if not hash_is_valid:
        errors.append(f"{label}.sha256 must be exactly 64 uppercase hexadecimal characters")

    if local_path is None or not isinstance(raw_path, str):
        return None
    try:
        actual_size, actual_hash = file_reader(raw_path, local_path)
    except OSError:
        errors.append(f"{label}: file is missing or unreadable: {raw_path}")
        return raw_path, local_path
    if (
        not isinstance(actual_size, int)
        or isinstance(actual_size, bool)
        or not isinstance(actual_hash, str)
        or not SHA256_RE.fullmatch(actual_hash)
    ):
        errors.append(f"{label}: file reader returned invalid facts")
        return raw_path, local_path
    if expected_size != actual_size:
        errors.append(f"{label}.bytes does not match {raw_path}")
    if expected_hash != actual_hash:
        errors.append(f"{label}.sha256 does not match {raw_path}")
    if size_is_valid and hash_is_valid and expected_size == actual_size and expected_hash == actual_hash:
        verified_files.append(
            {
                "role": label,
                "path": raw_path,
                "bytes": actual_size,
                "sha256": actual_hash,
            }
        )
    return raw_path, local_path


def _validate_source_file_record(
    value: Any,
    motion_id: Any,
    source_verifier: SourceVerifier,
    errors: list[str],
    verified_files: list[dict[str, Any]],
) -> tuple[tuple[str, Path] | None, dict[str, Any] | None]:
    label = "source_file"
    record = _require_exact_fields(value, FILE_RECORD_FIELDS, label, errors)
    if record is None:
        return None, None
    raw_path = record.get("path")
    local_path, path_error = _intake_repo_file_path(raw_path, motion_id)
    if path_error:
        errors.append(f"{label}: {path_error}")
    expected_size = record.get("bytes")
    size_valid = (
        isinstance(expected_size, int)
        and not isinstance(expected_size, bool)
        and expected_size > 0
    )
    if not size_valid:
        errors.append(f"{label}.bytes must be a positive integer")
    expected_hash = record.get("sha256")
    hash_valid = isinstance(expected_hash, str) and SHA256_RE.fullmatch(expected_hash) is not None
    if not hash_valid:
        errors.append(f"{label}.sha256 must be exactly 64 uppercase hexadecimal characters")
    if local_path is None or not isinstance(raw_path, str):
        return None, None
    if PurePosixPath(raw_path).suffix.casefold() not in ACCEPTED_VIDEO_EXTENSIONS:
        errors.append("source_file.path must end in .mov or .mp4 for supported mono-video ingest")
        return (raw_path, local_path), None
    try:
        verification = source_verifier(raw_path, local_path)
    except (OSError, ValueError):
        errors.append("source_file is missing, unstable, malformed, or non-video")
        return (raw_path, local_path), None
    facts = _require_exact_fields(
        verification,
        SOURCE_VERIFICATION_FIELDS,
        "source_verification",
        errors,
    )
    if facts is None:
        return (raw_path, local_path), None
    actual_size = facts.get("bytes")
    actual_hash = facts.get("sha256")
    probe_hash = facts.get("probe_input_sha256")
    stable_hash = facts.get("stable_rehash_sha256")
    hashes_valid = all(
        isinstance(item, str) and SHA256_RE.fullmatch(item) is not None
        for item in (actual_hash, probe_hash, stable_hash)
    )
    if not isinstance(actual_size, int) or isinstance(actual_size, bool) or actual_size <= 0:
        errors.append("source_verification.bytes must be a positive integer")
    if not hashes_valid:
        errors.append("source_verification hashes must be uppercase SHA-256")
    if expected_size != actual_size:
        errors.append(f"{label}.bytes does not match {raw_path}")
    if expected_hash != actual_hash:
        errors.append(f"{label}.sha256 does not match {raw_path}")
    if hashes_valid and not actual_hash == probe_hash == stable_hash:
        errors.append("source hash and probed media bytes are not same-handle identical")
    if facts.get("handle_stable") is not True:
        errors.append("source file handle identity/size/time was unstable during verification")
    if facts.get("path_identity_stable") is not True:
        errors.append("source path identity changed while its open handle was verified")
    if facts.get("probe_implementation") != MEDIA_PROBE_IMPLEMENTATION:
        errors.append("source media probe implementation is not the required built-in parser")
    if facts.get("probe_version") != MEDIA_PROBE_VERSION:
        errors.append("source media probe version is not the required contract version")
    media = _require_exact_fields(facts.get("media"), TECHNICAL_FIELDS, "probed_media", errors)
    if (
        size_valid
        and hash_valid
        and expected_size == actual_size
        and expected_hash == actual_hash
        and hashes_valid
        and actual_hash == probe_hash == stable_hash
        and facts.get("handle_stable") is True
        and facts.get("path_identity_stable") is True
        and facts.get("probe_implementation") == MEDIA_PROBE_IMPLEMENTATION
        and facts.get("probe_version") == MEDIA_PROBE_VERSION
        and media is not None
    ):
        verified_files.append(
            {
                "role": label,
                "path": raw_path,
                "bytes": actual_size,
                "sha256": actual_hash,
            }
        )
    return (raw_path, local_path), facts


RegistryIdReader = Callable[[], tuple[set[str], set[str]]]
TargetExists = Callable[[str, Path], bool]
SameFile = Callable[[Path, Path], bool]


def _registry_ids_from_frozen_bytes(payload: bytes) -> tuple[set[str], set[str]]:
    if len(payload) != FROZEN_REGISTRY_BYTES:
        raise ValueError("frozen registry byte length differs")
    if hashlib.sha256(payload).hexdigest().upper() != FROZEN_REGISTRY_SHA256:
        raise ValueError("frozen registry SHA-256 differs")
    registry = json.loads(
        payload.decode("utf-8"),
        object_pairs_hook=_reject_duplicate_intake_keys,
    )
    if (
        not isinstance(registry, dict)
        or registry.get("schema") != "disc_golf_motion_source_registry"
        or type(registry.get("schema_version")) is not int
        or registry.get("schema_version") != 1
        or not isinstance(registry.get("sources"), list)
    ):
        raise ValueError("registry source array is missing")
    source_ids: set[str] = set()
    motion_ids: set[str] = set()
    for row in registry["sources"]:
        if not isinstance(row, dict):
            raise ValueError("registry source row is invalid")
        source_id = row.get("source_id")
        motion_id = row.get("motion_id")
        if (
            not isinstance(source_id, str)
            or not SOURCE_ID_RE.fullmatch(source_id)
        ):
            raise ValueError("registry source_id is invalid")
        if (
            not isinstance(motion_id, str)
            or not MOTION_ID_RE.fullmatch(motion_id)
        ):
            raise ValueError("registry motion_id is invalid")
        if source_id in source_ids or motion_id in motion_ids:
            raise ValueError("registry IDs are not unique")
        source_ids.add(source_id)
        motion_ids.add(motion_id)
    if not source_ids:
        raise ValueError("registry contains no IDs")
    return source_ids, motion_ids


def _read_registry_ids() -> tuple[set[str], set[str]]:
    return _registry_ids_from_frozen_bytes(REGISTRY_PATH.read_bytes())


def _target_file_exists(package_path: str, local_path: Path) -> bool:
    del package_path
    for suffix in TARGET_COLLISION_SUFFIXES:
        candidate = local_path.with_suffix(suffix)
        try:
            os.lstat(candidate)
        except FileNotFoundError:
            continue
        except OSError:
            raise
        return True
    return False


def _target_package_file(package_path: str) -> Path:
    relative = package_path.removeprefix("/Game/") + ".uasset"
    return PROJECT_ROOT / "Content" / Path(*PurePosixPath(relative).parts)


def _unreal_path_length_error(package_path: str) -> str | None:
    leaf = package_path.rsplit("/", 1)[-1]
    object_path = f"{package_path}.{leaf}"
    if len(package_path) > MAX_UNREAL_PACKAGE_PATH_LENGTH:
        return f"package path exceeds {MAX_UNREAL_PACKAGE_PATH_LENGTH} characters"
    if len(object_path) > MAX_UNREAL_OBJECT_PATH_LENGTH:
        return f"object path exceeds {MAX_UNREAL_OBJECT_PATH_LENGTH} characters"
    return None


def _same_file(left: Path, right: Path) -> bool:
    return left.samefile(right)


def _file_identity_errors(
    records: list[tuple[str, tuple[str, Path] | None]],
    same_file: SameFile,
) -> list[str]:
    errors: list[str] = []
    for left_index, (left_label, left) in enumerate(records):
        if left is None:
            continue
        left_raw, left_path = left
        for right_label, right in records[left_index + 1 :]:
            if right is None:
                continue
            right_raw, right_path = right
            identical = str(left_path).casefold() == str(right_path).casefold()
            if not identical:
                try:
                    identical = same_file(left_path, right_path)
                except OSError:
                    errors.append(
                        f"{left_label} and {right_label} identity could not be checked exactly"
                    )
                    continue
            if identical:
                errors.append(
                    f"{left_label} and {right_label} must be pairwise-distinct files "
                    f"({left_raw!r} aliases {right_raw!r})"
                )
    return errors


def _validate_intake(
    intake: Any,
    file_reader: IntakeFileReader = _read_intake_file,
    source_verifier: SourceVerifier = _verify_source_same_handle,
    registry_id_reader: RegistryIdReader = _read_registry_ids,
    target_exists: TargetExists = _target_file_exists,
    same_file: SameFile = _same_file,
) -> tuple[list[str], list[dict[str, Any]], dict[str, Any] | None]:
    """Validate one staging intake without mutating the project or granting promotion."""

    errors: list[str] = []
    verified_files: list[dict[str, Any]] = []
    probed_media: dict[str, Any] | None = None
    root = _require_exact_fields(intake, INTAKE_FIELDS, "intake", errors)
    if root is None:
        return errors, verified_files, None

    if root.get("schema") != INTAKE_SCHEMA:
        errors.append(f"schema must be {INTAKE_SCHEMA}")
    if type(root.get("schema_version")) is not int or root.get("schema_version") != INTAKE_SCHEMA_VERSION:
        errors.append(f"schema_version must be integer {INTAKE_SCHEMA_VERSION}")

    source_id = root.get("source_id")
    source_id_valid = (
        isinstance(source_id, str)
        and len(source_id) <= MAX_SOURCE_ID_LENGTH
        and SOURCE_ID_RE.fullmatch(source_id) is not None
        and _is_named_value(source_id)
    )
    if not source_id_valid:
        errors.append(
            f"source_id must be real non-placeholder lowercase snake_case no longer than {MAX_SOURCE_ID_LENGTH}"
        )
    motion_id = root.get("motion_id")
    motion_id_valid = (
        isinstance(motion_id, str)
        and len(motion_id) <= MAX_MOTION_ID_LENGTH
        and MOTION_ID_RE.fullmatch(motion_id) is not None
        and _is_named_value(motion_id)
    )
    if not motion_id_valid:
        errors.append(
            "motion_id must be real non-placeholder uppercase hyphen-separated text "
            f"no longer than {MAX_MOTION_ID_LENGTH}"
        )
    take_id = root.get("take_id")
    if (
        not isinstance(take_id, str)
        or len(take_id) > MAX_TAKE_ID_LENGTH
        or not TAKE_ID_RE.fullmatch(take_id)
        or not _is_named_value(take_id)
    ):
        errors.append(
            f"take_id must be real non-placeholder lowercase snake_case no longer than {MAX_TAKE_ID_LENGTH}"
        )
    asset_revision = root.get("asset_revision")
    revision_valid = isinstance(asset_revision, str) and ASSET_REVISION_RE.fullmatch(asset_revision) is not None
    if not revision_valid:
        errors.append("asset_revision must be v001 through v999")
    if root.get("handedness") != "RIGHT":
        errors.append("handedness must be exactly RIGHT")
    if root.get("throw_type") != "RHBH":
        errors.append("throw_type must be exactly RHBH")
    classification = root.get("classification")
    classification_valid = (
        isinstance(classification, str) and classification in ACCEPTED_INTAKE_CLASSIFICATIONS
    )
    if not classification_valid:
        errors.append(
            "classification must be exactly PROJECT_OWNED_CAPTURE, WRITTEN_PERMISSION, "
            "or VALID_COMMERCIAL_LICENSE"
        )

    try:
        registered_source_ids, registered_motion_ids = registry_id_reader()
        if not isinstance(registered_source_ids, set) or not all(
            isinstance(value, str) for value in registered_source_ids
        ):
            raise ValueError("invalid source ID set")
        if not isinstance(registered_motion_ids, set) or not all(
            isinstance(value, str) for value in registered_motion_ids
        ):
            raise ValueError("invalid motion ID set")
    except (OSError, TypeError, ValueError):
        registered_source_ids = set()
        registered_motion_ids = set()
        errors.append("current registry IDs could not be read exactly; intake is blocked")
    else:
        if source_id_valid and source_id in registered_source_ids:
            errors.append("source_id already exists in the current registry")
        if motion_id_valid and motion_id in registered_motion_ids:
            errors.append("motion_id already exists in the current registry")

    source_path, source_verification = _validate_source_file_record(
        root.get("source_file"),
        motion_id,
        source_verifier,
        errors,
        verified_files,
    )
    rights_path = _validate_intake_file_record(
        root.get("rights_evidence_file"),
        "rights_evidence_file",
        motion_id,
        file_reader,
        errors,
        verified_files,
    )

    provenance = _require_exact_fields(
        root.get("provenance"), PROVENANCE_FIELDS, "provenance", errors
    )
    if provenance is not None:
        provider = provenance.get("provider")
        if not _is_named_value(provider):
            errors.append("provenance.provider must be a real non-placeholder provider name")
        if _contains_pexels(provider):
            errors.append("provider PEXELS is unconditionally prohibited")

        source_url = provenance.get("source_url")
        acquisition_method = provenance.get("acquisition_method")
        if classification == "PROJECT_OWNED_CAPTURE":
            if source_url is not None:
                errors.append("PROJECT_OWNED_CAPTURE provenance.source_url must be null")
            if acquisition_method != "DIRECT_PROJECT_CAPTURE":
                errors.append(
                    "PROJECT_OWNED_CAPTURE acquisition_method must be exactly DIRECT_PROJECT_CAPTURE"
                )
        elif classification in {"WRITTEN_PERMISSION", "VALID_COMMERCIAL_LICENSE"}:
            url_error = _canonical_https_dns_form_url_error(source_url)
            if url_error:
                errors.append(
                    f"external provenance.source_url {url_error}; this is offline syntax validation only"
                )
            if acquisition_method == "DIRECT_PROJECT_CAPTURE":
                errors.append("external classifications cannot use DIRECT_PROJECT_CAPTURE")

        if not _is_named_value(provenance.get("creator")):
            errors.append("provenance.creator must be a real non-placeholder creator name")
        if not _is_named_value(provenance.get("acquisition_method")):
            errors.append("provenance.acquisition_method must be real and non-placeholder")
        acquired_on = provenance.get("acquired_on")
        try:
            if not isinstance(acquired_on, str):
                raise ValueError
            parsed_acquisition_date = date.fromisoformat(acquired_on)
            if parsed_acquisition_date.isoformat() != acquired_on:
                raise ValueError
        except ValueError:
            errors.append("provenance.acquired_on must be an ISO YYYY-MM-DD date")
        else:
            if parsed_acquisition_date > date.today():
                errors.append("provenance.acquired_on cannot be in the future")

    permissions = _require_exact_fields(root.get("permissions"), PERMISSION_FIELDS, "permissions", errors)
    if permissions is not None:
        for field in sorted(PERMISSION_FIELDS):
            if permissions.get(field) is not True:
                errors.append(f"permissions.{field} must be exactly true")

    performer_release = _require_exact_fields(
        root.get("performer_release"),
        PERFORMER_RELEASE_FIELDS,
        "performer_release",
        errors,
    )
    performer_evidence_path: tuple[str, Path] | None = None
    if performer_release is not None:
        if not _is_named_value(performer_release.get("performer_name")):
            errors.append("performer_release.performer_name must name the performer")
        if performer_release.get("verified") is not True:
            errors.append("performer_release.verified must be exactly true")
        performer_evidence_path = _validate_intake_file_record(
            performer_release.get("evidence_file"),
            "performer_release.evidence_file",
            motion_id,
            file_reader,
            errors,
            verified_files,
        )
    errors.extend(
        _file_identity_errors(
            [
                ("source_file", source_path),
                ("rights_evidence_file", rights_path),
                ("performer_release.evidence_file", performer_evidence_path),
            ],
            same_file,
        )
    )

    reviews = _require_exact_fields(root.get("reviews"), REVIEW_FIELDS, "reviews", errors)
    if reviews is not None:
        for review_name, required_disposition in (
            ("rights", RIGHTS_REVIEW_DISPOSITION),
            ("throw_form", FORM_REVIEW_DISPOSITION),
        ):
            review = _require_exact_fields(
                reviews.get(review_name),
                REVIEW_RECORD_FIELDS,
                f"reviews.{review_name}",
                errors,
            )
            if review is None:
                continue
            if not _is_named_value(review.get("reviewer")):
                errors.append(f"reviews.{review_name}.reviewer must name a reviewer")
            if review.get("disposition") != required_disposition:
                errors.append(
                    f"reviews.{review_name}.disposition must be exactly {required_disposition}"
                )

    assertions = _require_exact_fields(
        root.get("capture_assertions"),
        CAPTURE_ASSERTION_FIELDS,
        "capture_assertions",
        errors,
    )
    if assertions is not None:
        for field in sorted(CAPTURE_ASSERTION_FIELDS):
            if assertions.get(field) is not True:
                errors.append(f"capture_assertions.{field} must be exactly true")

    technical = _require_exact_fields(root.get("technical"), TECHNICAL_FIELDS, "technical", errors)
    if technical is not None:
        frame_rate = technical.get("frame_rate_fps")
        if (
            not _is_finite_number(frame_rate)
            or frame_rate < 60.0
        ):
            errors.append("technical.frame_rate_fps must be at least 60")
        width = technical.get("width_pixels")
        height = technical.get("height_pixels")
        if (
            not isinstance(width, int)
            or isinstance(width, bool)
            or not isinstance(height, int)
            or isinstance(height, bool)
            or min(width, height) < 1080
            or max(width, height) < 1920
        ):
            errors.append("technical resolution must be at least 1920x1080 in landscape or portrait")
        codec = technical.get("codec")
        if not isinstance(codec, str) or codec not in ACCEPTED_VIDEO_CODECS:
            errors.append(
                "technical.codec must be one of: " + ", ".join(sorted(ACCEPTED_VIDEO_CODECS))
            )
        if technical.get("frame_rate_mode") != "CONSTANT":
            errors.append("technical.frame_rate_mode must be exactly CONSTANT")
        duration = technical.get("duration_seconds")
        if (
            not _is_finite_number(duration)
            or not MIN_SOURCE_DURATION_SECONDS <= duration <= MAX_SOURCE_DURATION_SECONDS
        ):
            errors.append(
                "technical.duration_seconds must be between "
                f"{MIN_SOURCE_DURATION_SECONDS:g} and {MAX_SOURCE_DURATION_SECONDS:g} inclusive"
            )

    if source_verification is not None:
        probed_media = source_verification.get("media")
        if isinstance(probed_media, dict) and technical is not None:
            for field in ("codec", "width_pixels", "height_pixels", "frame_rate_mode"):
                if technical.get(field) != probed_media.get(field):
                    errors.append(f"technical.{field} does not match probed source media")
            declared_rate = technical.get("frame_rate_fps")
            probed_rate = probed_media.get("frame_rate_fps")
            if (
                not _is_finite_number(declared_rate)
                or not _is_finite_number(probed_rate)
                or abs(declared_rate - probed_rate) > 0.01
            ):
                errors.append("technical.frame_rate_fps does not match probed source media")
            declared_duration = technical.get("duration_seconds")
            probed_duration = probed_media.get("duration_seconds")
            duration_tolerance = (
                max(0.02, 1.0 / probed_rate)
                if _is_finite_number(probed_rate) and probed_rate > 0
                else 0.02
            )
            if (
                not _is_finite_number(declared_duration)
                or not _is_finite_number(probed_duration)
                or abs(declared_duration - probed_duration) > duration_tolerance
            ):
                errors.append("technical.duration_seconds does not match probed source media")
            if probed_media.get("frame_rate_mode") != "CONSTANT":
                errors.append("source media must have constant frame timing")

    targets = _require_exact_fields(root.get("targets"), TARGET_FIELDS, "targets", errors)
    if targets is not None:
        raw_asset = targets.get("raw_asset_path")
        expected_raw = (
            f"{RAW_ASSET_PREFIX}A_{source_id}_{asset_revision}_RAW"
            if source_id_valid and revision_valid
            else None
        )
        raw_length_error = (
            _unreal_path_length_error(raw_asset) if isinstance(raw_asset, str) else None
        )
        if raw_length_error:
            errors.append(f"targets.raw_asset_path {raw_length_error}")
        raw_valid = (
            isinstance(raw_asset, str)
            and raw_asset == expected_raw
            and raw_length_error is None
        )
        if not raw_valid:
            errors.append(
                "targets.raw_asset_path must exactly bind source_id and asset_revision as "
                f"{expected_raw or RAW_ASSET_PREFIX + 'A_<source_id>_<vNNN>_RAW'}"
            )
        retargeted_asset = targets.get("retargeted_asset_path")
        expected_retargeted = (
            f"{RETARGETED_ASSET_PREFIX}A_{source_id}_{asset_revision}_RTG"
            if source_id_valid and revision_valid
            else None
        )
        retargeted_length_error = (
            _unreal_path_length_error(retargeted_asset)
            if isinstance(retargeted_asset, str)
            else None
        )
        if retargeted_length_error:
            errors.append(f"targets.retargeted_asset_path {retargeted_length_error}")
        retargeted_valid = (
            isinstance(retargeted_asset, str)
            and retargeted_asset == expected_retargeted
            and retargeted_length_error is None
        )
        if not retargeted_valid:
            errors.append(
                "targets.retargeted_asset_path must exactly bind source_id and asset_revision as "
                f"{expected_retargeted or RETARGETED_ASSET_PREFIX + 'A_<source_id>_<vNNN>_RTG'}"
            )
        for label, package_path, path_valid in (
            ("raw_asset_path", raw_asset, raw_valid),
            ("retargeted_asset_path", retargeted_asset, retargeted_valid),
        ):
            if isinstance(package_path, str) and package_path.casefold() in FROZEN_SYNTHETIC_TARGETS:
                errors.append(f"targets.{label} must not equal a frozen synthetic fixture target")
            if path_valid:
                try:
                    local_target = _target_package_file(package_path)
                    exists = target_exists(package_path, local_target)
                except (OSError, ValueError):
                    errors.append(f"targets.{label} existence could not be checked exactly")
                else:
                    if not isinstance(exists, bool):
                        errors.append(
                            f"targets.{label} existence probe returned a non-boolean result"
                        )
                    elif exists:
                        errors.append(f"targets.{label} already exists and cannot be overwritten")
        if (
            isinstance(raw_asset, str)
            and isinstance(retargeted_asset, str)
            and raw_asset.casefold() == retargeted_asset.casefold()
        ):
            errors.append("raw and retargeted targets must be distinct")
        if targets.get("cleaned_asset_path") is not None:
            errors.append("targets.cleaned_asset_path must remain null during staging preflight")
        if targets.get("production_asset_path") is not None:
            errors.append("targets.production_asset_path must remain null during staging preflight")

    if root.get("runtime_authority") != "DGMASTER":
        errors.append("runtime_authority must be exactly DGMASTER")
    if root.get("production_promotion_allowed") is not False:
        errors.append("production_promotion_allowed must be exactly false")

    verified_files.sort(key=lambda item: (item["role"], item["path"]))
    canonical_facts: dict[str, Any] | None = None
    if not errors:
        canonical_facts = {
            "intake": root,
            "verified_files": verified_files,
            "probed_media": {
                key: round(value, 9) if isinstance(value, float) else value
                for key, value in sorted((probed_media or {}).items())
            },
            "registry_duplicate_check": "PASS_NO_DUPLICATE_IDS",
            "frozen_registry": {
                "bytes": FROZEN_REGISTRY_BYTES,
                "sha256": FROZEN_REGISTRY_SHA256,
                "verified": True,
            },
            "media_probe": {
                "implementation": MEDIA_PROBE_IMPLEMENTATION,
                "version": MEDIA_PROBE_VERSION,
                "same_open_handle_hash_probe_rehash": True,
                "external_tool_used": False,
            },
            "external_url_validation_scope": (
                "OFFLINE_CANONICAL_HTTPS_DNS_FORM_AND_PLAUSIBLE_PUBLIC_SYNTAX_ONLY; "
                "PROVIDER_OWNERSHIP_REMAINS_NAMED_HUMAN_RIGHTS_REVIEW"
            ),
            "target_nonexistence_check": "PASS_TARGETS_ABSENT",
            "runtime_authority_preserved": True,
        }
    return errors, verified_files, canonical_facts


def _preflight_result(
    intake: Any,
    errors: list[str],
    verified_files: list[dict[str, Any]],
    intake_bytes: bytes | None,
    canonical_facts: dict[str, Any] | None,
) -> dict[str, Any]:
    valid_root = intake if isinstance(intake, dict) else {}
    eligible = not errors and canonical_facts is not None
    canonical_bytes = (
        json.dumps(
            canonical_facts,
            ensure_ascii=False,
            separators=(",", ":"),
            sort_keys=True,
        ).encode("utf-8")
        if canonical_facts is not None
        else None
    )
    return {
        "schema": PREFLIGHT_RESULT_SCHEMA,
        "schema_version": PREFLIGHT_RESULT_SCHEMA_VERSION,
        "status": "PASS_STAGING_ELIGIBLE" if eligible else "FAIL_STAGING_INELIGIBLE",
        "source_id": valid_root.get("source_id") if isinstance(valid_root.get("source_id"), str) else None,
        "motion_id": valid_root.get("motion_id") if isinstance(valid_root.get("motion_id"), str) else None,
        "staging_eligible": eligible,
        "disk_mutation": "NONE",
        "runtime_authority": "DGMASTER",
        "runtime_authority_changed": False,
        "production_promotion_allowed": False,
        "release_approved": False,
        "legal_approval": False,
        "human_review_binding": (
            "NAMED_HUMAN_ATTESTATIONS_BOUND_BY_INTAKE_JSON_SHA256; "
            "NOT_AUTOMATED_AUTHENTICITY_OR_LEGAL_PROOF"
            if eligible
            else None
        ),
        "frozen_registry_contract": {
            "bytes": FROZEN_REGISTRY_BYTES,
            "sha256": FROZEN_REGISTRY_SHA256,
            "verified_for_this_pass": eligible,
        },
        "media_probe_contract": {
            "implementation": MEDIA_PROBE_IMPLEMENTATION,
            "version": MEDIA_PROBE_VERSION,
            "same_open_handle_hash_probe_rehash_required": True,
            "external_tool_used": False,
            "verified_for_this_pass": eligible,
        },
        "intake_json_bytes": len(intake_bytes) if intake_bytes is not None else None,
        "intake_json_sha256": (
            hashlib.sha256(intake_bytes).hexdigest().upper() if intake_bytes is not None else None
        ),
        "canonical_validated_facts": canonical_facts,
        "canonical_validated_facts_sha256": (
            hashlib.sha256(canonical_bytes).hexdigest().upper()
            if canonical_bytes is not None
            else None
        ),
        "verified_files": verified_files,
        "errors": errors,
    }


class _DuplicateIntakeKeyError(ValueError):
    pass


def _reject_duplicate_intake_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise _DuplicateIntakeKeyError(key)
        result[key] = value
    return result


def _run_preflight(intake_path: Path) -> int:
    intake: Any = {}
    intake_bytes: bytes | None = None
    errors: list[str] = []
    try:
        intake_bytes = intake_path.read_bytes()
        intake = json.loads(
            intake_bytes.decode("utf-8"),
            object_pairs_hook=_reject_duplicate_intake_keys,
        )
    except OSError:
        errors.append("intake JSON could not be read")
    except UnicodeDecodeError:
        errors.append("intake JSON must be strict UTF-8")
    except _DuplicateIntakeKeyError:
        errors.append("intake JSON contains duplicate object keys")
    except json.JSONDecodeError:
        errors.append("intake JSON is invalid")
    verified_files: list[dict[str, Any]] = []
    canonical_facts: dict[str, Any] | None = None
    if not errors:
        errors, verified_files, canonical_facts = _validate_intake(intake)
    print(
        json.dumps(
            _preflight_result(
                intake,
                errors,
                verified_files,
                intake_bytes,
                canonical_facts,
            ),
            indent=2,
            sort_keys=True,
        )
    )
    return 0 if not errors else 1


def _bmff_box(box_type: bytes, payload: bytes) -> bytes:
    return struct.pack(">I4s", 8 + len(payload), box_type) + payload


def _synthetic_bmff_video(
    *,
    width: int = 1920,
    height: int = 1080,
    frame_rate: int = 60,
    duration_seconds: int = 8,
    codec: bytes = b"avc1",
    variable_timing: bool = False,
    video_tracks: int = 1,
    chunk_count: int = 1,
    variable_sample_sizes: bool = False,
    include_codec_config: bool = True,
    duplicate_codec_config: bool = False,
    malformed_codec_config: bool = False,
    major_brand: bytes = b"isom",
    compatible_brands: tuple[bytes, ...] = (b"isom", b"mp42"),
    avc_profile: int = 66,
    visual_entry_trailing_bytes: bytes = b"",
) -> bytes:
    timescale = 60000
    sample_delta = timescale // frame_rate
    sample_count = frame_rate * duration_seconds
    if chunk_count <= 0 or sample_count % chunk_count != 0:
        raise ValueError("synthetic BMFF chunk_count must divide sample_count")
    samples_per_chunk = sample_count // chunk_count
    if variable_timing:
        first_count = sample_count // 2
        timing_entries = [(first_count, sample_delta - 1), (sample_count - first_count, sample_delta + 1)]
    else:
        timing_entries = [(sample_count, sample_delta)]
    total_ticks = sum(count * delta for count, delta in timing_entries)

    visual_entry = bytearray(86)
    struct.pack_into(">H", visual_entry, 14, 1)
    struct.pack_into(">HH", visual_entry, 32, width, height)
    configuration = b""
    if include_codec_config and codec in {b"avc1", b"avc3"}:
        avcc = bytes([1, avc_profile, 0, 30]) + b"\xff\xe1\x00\x04\x67\x42\x00\x1e\x01\x00\x02\x68\xce"
        if avc_profile in {44, 83, 86, 100, 110, 118, 122, 128, 134, 135, 138, 139, 144, 244}:
            avcc += b"\xfd\xf8\xf8\x01\x00\x02\x6d\x01"
        if malformed_codec_config:
            avcc = avcc[:-1]
        configuration = _bmff_box(b"avcC", avcc)
    elif include_codec_config and codec in {b"hvc1", b"hev1"}:
        hvcc = bytearray(23)
        hvcc[0], hvcc[21], hvcc[22] = 1, 3, 3
        for nal_type in (32, 33, 34):
            hvcc += bytes([0x80 | nal_type]) + struct.pack(">HH", 1, 2) + b"\x01\x01"
        if malformed_codec_config:
            hvcc = hvcc[:-1]
        configuration = _bmff_box(b"hvcC", bytes(hvcc))
    if duplicate_codec_config:
        configuration += configuration
    visual_entry += configuration + visual_entry_trailing_bytes
    struct.pack_into(">I4s", visual_entry, 0, len(visual_entry), codec)
    stsd = _bmff_box(b"stsd", b"\x00\x00\x00\x00" + struct.pack(">I", 1) + bytes(visual_entry))
    stts_payload = b"\x00\x00\x00\x00" + struct.pack(">I", len(timing_entries))
    stts_payload += b"".join(struct.pack(">II", count, delta) for count, delta in timing_entries)
    stts = _bmff_box(b"stts", stts_payload)
    stsz_payload = b"\x00\x00\x00\x00" + struct.pack(
        ">II", 0 if variable_sample_sizes else 1, sample_count
    )
    if variable_sample_sizes:
        stsz_payload += b"".join(struct.pack(">I", 1) for _ in range(sample_count))
    stsz = _bmff_box(b"stsz", stsz_payload)
    stsc = _bmff_box(
        b"stsc",
        b"\x00\x00\x00\x00"
        + struct.pack(">I", 1)
        + struct.pack(">III", 1, samples_per_chunk, 1),
    )
    # The fixed ftyp below is 24 bytes, followed by the 8-byte mdat header.
    stco = _bmff_box(
        b"stco",
        b"\x00\x00\x00\x00"
        + struct.pack(">I", chunk_count)
        + b"".join(
            struct.pack(">I", 32 + index * samples_per_chunk)
            for index in range(chunk_count)
        ),
    )
    stbl = _bmff_box(b"stbl", stsd + stts + stsz + stsc + stco)
    minf = _bmff_box(b"minf", stbl)
    mdhd = _bmff_box(
        b"mdhd",
        b"\x00\x00\x00\x00"
        + struct.pack(">IIII", 0, 0, timescale, total_ticks)
        + b"\x00\x00\x00\x00",
    )
    hdlr = _bmff_box(
        b"hdlr",
        b"\x00\x00\x00\x00" + b"\x00\x00\x00\x00" + b"vide" + b"\x00" * 12,
    )
    track = _bmff_box(b"trak", _bmff_box(b"mdia", mdhd + hdlr + minf))
    moov = _bmff_box(b"moov", track * video_tracks)
    ftyp = _bmff_box(b"ftyp", major_brand + struct.pack(">I", 0) + b"".join(compatible_brands))
    mdat = _bmff_box(b"mdat", b"\x00" * sample_count)
    return ftyp + mdat + moov


def _valid_self_test_intake() -> tuple[dict[str, Any], dict[str, bytes]]:
    motion_id = "DG-RHBH-CAPTURE-001"
    root = f"SourceArt/DiscGolf/Mocap/{motion_id}"
    payloads = {
        f"{root}/throw.mov": _synthetic_bmff_video(),
        f"{root}/rights.txt": b"rights-evidence-fixture",
        f"{root}/performer-release.txt": b"performer-release-fixture",
    }

    def record(path: str) -> dict[str, Any]:
        payload = payloads[path]
        return {
            "path": path,
            "bytes": len(payload),
            "sha256": hashlib.sha256(payload).hexdigest().upper(),
        }

    intake = {
        "schema": INTAKE_SCHEMA,
        "schema_version": INTAKE_SCHEMA_VERSION,
        "source_id": "dg_rhbh_capture_001",
        "motion_id": motion_id,
        "take_id": "primary_tee_take_001",
        "asset_revision": "v001",
        "handedness": "RIGHT",
        "throw_type": "RHBH",
        "classification": "PROJECT_OWNED_CAPTURE",
        "source_file": record(f"{root}/throw.mov"),
        "rights_evidence_file": record(f"{root}/rights.txt"),
        "provenance": {
            "provider": "DGTour Project Capture",
            "source_url": None,
            "creator": "Jordan Rivera",
            "acquisition_method": "DIRECT_PROJECT_CAPTURE",
            "acquired_on": "2024-06-15",
        },
        "permissions": {
            "commercial_interactive_use": True,
            "derivative_animation": True,
            "solver_output_use": True,
        },
        "performer_release": {
            "performer_name": "Morgan Hale",
            "verified": True,
            "evidence_file": record(f"{root}/performer-release.txt"),
        },
        "reviews": {
            "rights": {
                "reviewer": "Avery Morgan",
                "disposition": RIGHTS_REVIEW_DISPOSITION,
            },
            "throw_form": {
                "reviewer": "Casey Jordan",
                "disposition": FORM_REVIEW_DISPOSITION,
            },
        },
        "capture_assertions": {field: True for field in CAPTURE_ASSERTION_FIELDS},
        "technical": {
            "frame_rate_fps": 60,
            "frame_rate_mode": "CONSTANT",
            "width_pixels": 1920,
            "height_pixels": 1080,
            "codec": "H264",
            "duration_seconds": 8.0,
        },
        "targets": {
            "raw_asset_path": f"{RAW_ASSET_PREFIX}A_dg_rhbh_capture_001_v001_RAW",
            "retargeted_asset_path": f"{RETARGETED_ASSET_PREFIX}A_dg_rhbh_capture_001_v001_RTG",
            "cleaned_asset_path": None,
            "production_asset_path": None,
        },
        "runtime_authority": "DGMASTER",
        "production_promotion_allowed": False,
    }
    return intake, payloads


def _run_intake_self_tests() -> int:
    import copy

    valid, payloads = _valid_self_test_intake()

    def validate(
        candidate: Any,
        candidate_payloads: dict[str, bytes] | None = None,
        probe_payloads: dict[str, bytes] | None = None,
        registry_ids: tuple[set[str], set[str]] | None = (set(), set()),
        existing_targets: set[str] | None = None,
        target_probe_failure: bool = False,
        same_file_pairs: set[frozenset[str]] | None = None,
        same_file_failure: bool = False,
    ) -> tuple[list[str], list[dict[str, Any]], dict[str, Any] | None]:
        active_payloads = payloads if candidate_payloads is None else candidate_payloads

        def fake_reader(relative_path: str, local_path: Path) -> tuple[int, str]:
            del local_path
            try:
                payload = active_payloads[relative_path]
            except KeyError as exc:
                raise FileNotFoundError(relative_path) from exc
            return len(payload), hashlib.sha256(payload).hexdigest().upper()

        def fake_source_verifier(relative_path: str, local_path: Path) -> dict[str, Any]:
            del local_path
            if PurePosixPath(relative_path).suffix.casefold() not in ACCEPTED_VIDEO_EXTENSIONS:
                raise ValueError("unsupported media extension")
            try:
                hashed_payload = active_payloads[relative_path]
                probed_payload = (
                    probe_payloads[relative_path]
                    if probe_payloads is not None
                    else hashed_payload
                )
            except KeyError as exc:
                raise FileNotFoundError(relative_path) from exc
            hashed_sha = hashlib.sha256(hashed_payload).hexdigest().upper()
            probed_sha = hashlib.sha256(probed_payload).hexdigest().upper()
            return {
                "bytes": len(hashed_payload),
                "sha256": hashed_sha,
                "probe_input_sha256": probed_sha,
                "stable_rehash_sha256": hashed_sha,
                "handle_stable": True,
                "path_identity_stable": True,
                "media": _probe_bmff_stream(io.BytesIO(probed_payload)),
                "probe_implementation": MEDIA_PROBE_IMPLEMENTATION,
                "probe_version": MEDIA_PROBE_VERSION,
            }

        def fake_registry_reader() -> tuple[set[str], set[str]]:
            if registry_ids is None:
                raise OSError("injected registry failure")
            return registry_ids

        def fake_target_exists(package_path: str, local_path: Path) -> bool:
            del local_path
            if target_probe_failure:
                raise OSError("injected collision-probe failure")
            return package_path in (existing_targets or set())

        def fake_same_file(left: Path, right: Path) -> bool:
            if same_file_failure:
                raise OSError("injected samefile failure")
            pair = frozenset((str(left).casefold(), str(right).casefold()))
            return pair in (same_file_pairs or set())

        return _validate_intake(
            candidate,
            fake_reader,
            fake_source_verifier,
            fake_registry_reader,
            fake_target_exists,
            fake_same_file,
        )

    def set_external(candidate: dict[str, Any], classification: str) -> None:
        candidate["classification"] = classification
        candidate["provenance"]["source_url"] = "https://media.rightsholder.org/throws/take-001"
        candidate["provenance"]["acquisition_method"] = (
            "RIGHTSHOLDER_GRANT"
            if classification == "WRITTEN_PERMISSION"
            else "LICENSED_DOWNLOAD"
        )

    cases: list[tuple[str, Callable[[dict[str, Any]], None], str]] = [
        ("schema", lambda item: item.__setitem__("schema", "wrong"), "schema must be"),
        ("schema-version", lambda item: item.__setitem__("schema_version", True), "schema_version"),
        ("extra-root", lambda item: item.__setitem__("unexpected", True), "unexpected fields"),
        ("source-id", lambda item: item.__setitem__("source_id", "Bad-ID"), "source_id"),
        (
            "source-id-5000",
            lambda item: item.__setitem__("source_id", "a" * 5000),
            "no longer than",
        ),
        (
            "source-id-template",
            lambda item: item.__setitem__("source_id", "replace_with_lowercase_source_id"),
            "non-placeholder",
        ),
        (
            "source-id-test",
            lambda item: item.__setitem__("source_id", "test_capture_001"),
            "non-placeholder",
        ),
        ("motion-id", lambda item: item.__setitem__("motion_id", "bad_motion"), "motion_id"),
        (
            "motion-id-5000",
            lambda item: item.__setitem__("motion_id", "A" * 5000),
            "no longer than",
        ),
        (
            "motion-id-template",
            lambda item: item.__setitem__("motion_id", "REPLACE-WITH-MOTION-ID"),
            "non-placeholder",
        ),
        (
            "motion-id-example",
            lambda item: item.__setitem__("motion_id", "DG-EXAMPLE-MOTION-001"),
            "non-placeholder",
        ),
        ("take-id", lambda item: item.__setitem__("take_id", "Bad-Take"), "take_id"),
        (
            "take-id-5000",
            lambda item: item.__setitem__("take_id", "a" * 5000),
            "no longer than",
        ),
        (
            "take-id-template",
            lambda item: item.__setitem__("take_id", "redacted_take_id"),
            "non-placeholder",
        ),
        (
            "take-id-redactme",
            lambda item: item.__setitem__("take_id", "redactme_take_id"),
            "non-placeholder",
        ),
        ("revision-zero", lambda item: item.__setitem__("asset_revision", "v000"), "asset_revision"),
        ("revision-case", lambda item: item.__setitem__("asset_revision", "V001"), "asset_revision"),
        ("handedness", lambda item: item.__setitem__("handedness", "LEFT"), "exactly RIGHT"),
        ("throw-type", lambda item: item.__setitem__("throw_type", "FOREHAND"), "exactly RHBH"),
        ("classification", lambda item: item.__setitem__("classification", "SYNTHETIC_TEST"), "classification"),
        (
            "project-url",
            lambda item: item["provenance"].__setitem__("source_url", "https://capture.vendor.org/take"),
            "source_url must be null",
        ),
        (
            "project-method",
            lambda item: item["provenance"].__setitem__("acquisition_method", "LICENSED_DOWNLOAD"),
            "DIRECT_PROJECT_CAPTURE",
        ),
        ("provider-placeholder", lambda item: item["provenance"].__setitem__("provider", "Example Provider"), "non-placeholder"),
        ("provider-replace", lambda item: item["provenance"].__setitem__("provider", "REPLACE_WITH_CANONICAL_PROVIDER"), "non-placeholder"),
        ("provider-replaceme", lambda item: item["provenance"].__setitem__("provider", "REPLACEME PROVIDER"), "non-placeholder"),
        ("provider-null", lambda item: item["provenance"].__setitem__("provider", "null"), "non-placeholder"),
        ("provider-fillme", lambda item: item["provenance"].__setitem__("provider", "fillme"), "non-placeholder"),
        ("provider-spaced-pexels", lambda item: item["provenance"].__setitem__("provider", "P e x e l s"), "PEXELS"),
        ("creator-placeholder", lambda item: item["provenance"].__setitem__("creator", "Self Test Creator"), "non-placeholder"),
        ("method-placeholder", lambda item: item["provenance"].__setitem__("acquisition_method", "P L A C E H O L D E R"), "non-placeholder"),
        ("date-format", lambda item: item["provenance"].__setitem__("acquired_on", "06/15/2024"), "ISO"),
        (
            "future-date",
            lambda item: item["provenance"].__setitem__(
                "acquired_on", date.fromordinal(date.today().toordinal() + 1).isoformat()
            ),
            "cannot be in the future",
        ),
        (
            "source-path-root",
            lambda item: item["source_file"].__setitem__("path", "SourceArt/DiscGolf/Mocap/OTHER/throw.mov"),
            "exact motion root",
        ),
        (
            "source-extension",
            lambda item: item["source_file"].__setitem__(
                "path", f"SourceArt/DiscGolf/Mocap/{item['motion_id']}/throw.mkv"
            ),
            "must end in .mov or .mp4",
        ),
        (
            "source-traversal",
            lambda item: item["source_file"].__setitem__(
                "path", f"SourceArt/DiscGolf/Mocap/{item['motion_id']}/../throw.mov"
            ),
            "escapes the repository",
        ),
        ("rights-path-ads", lambda item: item["rights_evidence_file"].__setitem__("path", f"SourceArt/DiscGolf/Mocap/{item['motion_id']}/rights.pdf:secret"), "Win32-invalid"),
        ("rights-path-trailing-dot", lambda item: item["rights_evidence_file"].__setitem__("path", f"SourceArt/DiscGolf/Mocap/{item['motion_id']}/rights."), "Win32-invalid"),
        ("rights-path-trailing-space", lambda item: item["rights_evidence_file"].__setitem__("path", f"SourceArt/DiscGolf/Mocap/{item['motion_id']}/rights "), "Win32-invalid"),
        ("rights-path-dos-device", lambda item: item["rights_evidence_file"].__setitem__("path", f"SourceArt/DiscGolf/Mocap/{item['motion_id']}/CON.pdf"), "Win32-invalid"),
        ("rights-path-del-control", lambda item: item["rights_evidence_file"].__setitem__("path", f"SourceArt/DiscGolf/Mocap/{item['motion_id']}/rights\x7f.pdf"), "Win32-invalid"),
        ("rights-path-unicode-control", lambda item: item["rights_evidence_file"].__setitem__("path", f"SourceArt/DiscGolf/Mocap/{item['motion_id']}/rights\u0085.pdf"), "Win32-invalid"),
        ("source-bytes", lambda item: item["source_file"].__setitem__("bytes", 1), "bytes does not match"),
        (
            "source-lower-hash",
            lambda item: item["source_file"].__setitem__("sha256", item["source_file"]["sha256"].lower()),
            "uppercase hexadecimal",
        ),
        ("rights-hash", lambda item: item["rights_evidence_file"].__setitem__("sha256", "0" * 64), "sha256 does not match"),
        (
            "source-rights-same",
            lambda item: item.__setitem__("rights_evidence_file", copy.deepcopy(item["source_file"])),
            "pairwise-distinct",
        ),
        (
            "rights-release-same",
            lambda item: item["performer_release"].__setitem__(
                "evidence_file", copy.deepcopy(item["rights_evidence_file"])
            ),
            "pairwise-distinct",
        ),
        *[
            (
                f"permission-{field}",
                lambda item, field=field: item["permissions"].__setitem__(field, False),
                f"permissions.{field}",
            )
            for field in sorted(PERMISSION_FIELDS)
        ],
        ("performer-placeholder", lambda item: item["performer_release"].__setitem__("performer_name", "Dummy Performer"), "name the performer"),
        ("performer-unverified", lambda item: item["performer_release"].__setitem__("verified", False), "verified"),
        ("rights-reviewer", lambda item: item["reviews"]["rights"].__setitem__("reviewer", "TBD"), "name a reviewer"),
        ("rights-disposition", lambda item: item["reviews"]["rights"].__setitem__("disposition", "PENDING"), RIGHTS_REVIEW_DISPOSITION),
        ("form-reviewer", lambda item: item["reviews"]["throw_form"].__setitem__("reviewer", "Example Reviewer"), "name a reviewer"),
        ("form-disposition", lambda item: item["reviews"]["throw_form"].__setitem__("disposition", "PENDING"), FORM_REVIEW_DISPOSITION),
        *[
            (
                f"assertion-{field}",
                lambda item, field=field: item["capture_assertions"].__setitem__(field, False),
                f"capture_assertions.{field}",
            )
            for field in sorted(CAPTURE_ASSERTION_FIELDS)
        ],
        ("frame-rate-min", lambda item: item["technical"].__setitem__("frame_rate_fps", 59.94), "at least 60"),
        ("frame-rate-mode", lambda item: item["technical"].__setitem__("frame_rate_mode", "VARIABLE"), "exactly CONSTANT"),
        ("resolution-min", lambda item: item["technical"].__setitem__("width_pixels", 1919), "1920x1080"),
        ("codec", lambda item: item["technical"].__setitem__("codec", "UNKNOWN"), "codec"),
        ("codec-dnxhr-unsupported", lambda item: item["technical"].__setitem__("codec", "DNXHR_HQ"), "codec"),
        ("duration-low", lambda item: item["technical"].__setitem__("duration_seconds", 0.99), "between 1 and 30"),
        ("duration-high", lambda item: item["technical"].__setitem__("duration_seconds", 30.01), "between 1 and 30"),
        ("raw-binding", lambda item: item["targets"].__setitem__("raw_asset_path", f"{RAW_ASSET_PREFIX}A_wrong_v001_RAW"), "exactly bind"),
        ("raw-path-overlong", lambda item: item["targets"].__setitem__("raw_asset_path", RAW_ASSET_PREFIX + "A_" + "a" * 5000), "package path exceeds"),
        ("raw-object-path-overlong", lambda item: item["targets"].__setitem__("raw_asset_path", RAW_ASSET_PREFIX + "A_" + "a" * 120), "object path exceeds"),
        ("retarget-binding", lambda item: item["targets"].__setitem__("retargeted_asset_path", f"{RETARGETED_ASSET_PREFIX}A_wrong_v001_RTG"), "exactly bind"),
        ("frozen-raw", lambda item: item["targets"].__setitem__("raw_asset_path", PIPELINE_FIXTURE_PATHS["raw_asset_path"]), "frozen synthetic"),
        ("same-target", lambda item: item["targets"].__setitem__("retargeted_asset_path", item["targets"]["raw_asset_path"]), "must be distinct"),
        ("cleaned-target", lambda item: item["targets"].__setitem__("cleaned_asset_path", "/Game/Bad"), "must remain null"),
        ("production-target", lambda item: item["targets"].__setitem__("production_asset_path", "/Game/Bad"), "must remain null"),
        ("runtime-authority", lambda item: item.__setitem__("runtime_authority", "METAHUMAN"), "DGMASTER"),
        ("promotion", lambda item: item.__setitem__("production_promotion_allowed", True), "exactly false"),
    ]

    failures: list[str] = []
    scenario_count = 0

    def expect_pass(name: str, candidate: dict[str, Any], candidate_payloads: dict[str, bytes] | None = None) -> tuple[list[dict[str, Any]], dict[str, Any] | None]:
        nonlocal scenario_count
        scenario_count += 1
        case_errors, files, facts = validate(candidate, candidate_payloads)
        if case_errors:
            failures.append(f"{name}: valid intake rejected: {'; '.join(case_errors)}")
        return files, facts

    def expect_fail(
        name: str,
        candidate: Any,
        expected: str,
        candidate_payloads: dict[str, bytes] | None = None,
        **seams: Any,
    ) -> None:
        nonlocal scenario_count
        scenario_count += 1
        case_errors, _, facts = validate(candidate, candidate_payloads, **seams)
        if not any(expected in error for error in case_errors):
            failures.append(f"{name}: expected {expected!r}; got {case_errors!r}")
        if facts is not None:
            failures.append(f"{name}: rejected intake produced canonical validated facts")

    valid_files, valid_facts = expect_pass("baseline", copy.deepcopy(valid))
    if len(valid_files) != 3 or valid_facts is None:
        failures.append("baseline: expected three verified files and canonical facts")

    intake_bytes = json.dumps(valid, indent=2, sort_keys=True).encode("utf-8") + b"\n"
    valid_result = _preflight_result(valid, [], valid_files, intake_bytes, valid_facts)
    scenario_count += 1
    for field, expected in {
        "status": "PASS_STAGING_ELIGIBLE",
        "staging_eligible": True,
        "disk_mutation": "NONE",
        "runtime_authority_changed": False,
        "production_promotion_allowed": False,
        "release_approved": False,
        "legal_approval": False,
        "intake_json_sha256": hashlib.sha256(intake_bytes).hexdigest().upper(),
    }.items():
        if valid_result.get(field) != expected:
            failures.append(f"result-binding: {field} was {valid_result.get(field)!r}")
    canonical_bytes = json.dumps(
        valid_facts, ensure_ascii=False, separators=(",", ":"), sort_keys=True
    ).encode("utf-8")
    if valid_result.get("canonical_validated_facts_sha256") != hashlib.sha256(canonical_bytes).hexdigest().upper():
        failures.append("result-binding: canonical facts SHA-256 mismatch")
    if not isinstance(valid_result.get("human_review_binding"), str):
        failures.append("result-binding: passing result omitted human review binding")
    if valid_result.get("frozen_registry_contract") != {
        "bytes": FROZEN_REGISTRY_BYTES,
        "sha256": FROZEN_REGISTRY_SHA256,
        "verified_for_this_pass": True,
    }:
        failures.append("result-binding: frozen registry contract is not exact")
    if valid_result.get("media_probe_contract", {}).get("implementation") != MEDIA_PROBE_IMPLEMENTATION:
        failures.append("result-binding: media probe implementation is not exact")
    if json.dumps(valid_result, sort_keys=True) != json.dumps(
        _preflight_result(valid, [], valid_files, intake_bytes, valid_facts), sort_keys=True
    ):
        failures.append("result-binding: output is not deterministic")
    failed_result = _preflight_result(valid, ["injected failure"], [], intake_bytes, None)
    scenario_count += 1
    if failed_result.get("human_review_binding") is not None:
        failures.append("result-binding: failing result claimed human review binding")
    if failed_result.get("frozen_registry_contract", {}).get("verified_for_this_pass") is not False:
        failures.append("result-binding: failing result claimed frozen registry verification")
    if failed_result.get("media_probe_contract", {}).get("verified_for_this_pass") is not False:
        failures.append("result-binding: failing result claimed media probe verification")

    for classification in ("WRITTEN_PERMISSION", "VALID_COMMERCIAL_LICENSE"):
        alternate = copy.deepcopy(valid)
        set_external(alternate, classification)
        expect_pass(f"classification-{classification}", alternate)

    portrait = copy.deepcopy(valid)
    portrait_payloads = dict(payloads)
    source_key = portrait["source_file"]["path"]
    portrait_payloads[source_key] = _synthetic_bmff_video(width=1080, height=1920)
    portrait["source_file"]["bytes"] = len(portrait_payloads[source_key])
    portrait["source_file"]["sha256"] = hashlib.sha256(portrait_payloads[source_key]).hexdigest().upper()
    portrait["technical"]["width_pixels"] = 1080
    portrait["technical"]["height_pixels"] = 1920
    expect_pass("portrait-media", portrait, portrait_payloads)

    for name, mutation, expected_error in cases:
        candidate = copy.deepcopy(valid)
        mutation(candidate)
        expect_fail(name, candidate, expected_error)

    external_invalid: list[tuple[str, Any, str]] = [
        ("external-url-null", None, "canonical HTTPS DNS-form URL"),
        ("external-url-http", "http://media.rightsholder.org/take", "canonical lowercase HTTPS"),
        ("external-url-uppercase-scheme", "HTTPS://media.rightsholder.org/take", "canonical lowercase HTTPS"),
        ("external-url-uppercase-host", "https://Media.Rightsholder.org/take", "hostname must be lowercase"),
        ("external-url-explicit-443", "https://media.rightsholder.org:443/take", "without credentials, port"),
        ("external-url-ip", "https://8.8.8.8/take", "not an IP address"),
        ("external-url-local", "https://capture.local/take", "placeholder, test"),
        ("external-url-example", "https://media.example.com/take", "placeholder, test"),
        ("external-url-pexels", "https://cdn.pexels.com/video/1", "PEXELS at any encoding layer"),
        ("external-url-spaced-pexels", "https://media.rightsholder.org/P%20e%20x%20e%20l%20s", "PEXELS at any encoding layer"),
        ("external-url-nested-pexels", "https://media.rightsholder.org/%2550%2565%2578%2565%256C%2573", "PEXELS at any encoding layer"),
        ("external-url-dot-segment", "https://media.rightsholder.org/a/../take", "no empty or dot-segments"),
        ("external-url-encoded-dot", "https://media.rightsholder.org/%2E%2E/take", "unstable percent encoding"),
        ("external-url-excess-encoding", "https://media.rightsholder.org/%25252541", "excessive nested percent encoding"),
    ]
    for name, url, expected in external_invalid:
        candidate = copy.deepcopy(valid)
        set_external(candidate, "WRITTEN_PERMISSION")
        candidate["provenance"]["source_url"] = url
        expect_fail(name, candidate, expected)

    duplicate_source = copy.deepcopy(valid)
    expect_fail(
        "registry-source-duplicate",
        duplicate_source,
        "source_id already exists",
        registry_ids=({valid["source_id"]}, set()),
    )
    expect_fail(
        "registry-motion-duplicate",
        copy.deepcopy(valid),
        "motion_id already exists",
        registry_ids=(set(), {valid["motion_id"]}),
    )
    expect_fail(
        "registry-unreadable",
        copy.deepcopy(valid),
        "could not be read exactly",
        registry_ids=None,
    )
    frozen_registry_payload = REGISTRY_PATH.read_bytes()
    scenario_count += 1
    try:
        frozen_source_ids, frozen_motion_ids = _registry_ids_from_frozen_bytes(
            frozen_registry_payload
        )
    except (OSError, ValueError) as exc:
        failures.append(f"frozen-registry-valid: exact frozen registry rejected: {exc}")
    else:
        if len(frozen_source_ids) != 9 or len(frozen_motion_ids) != 9:
            failures.append("frozen-registry-valid: expected 9 source and 9 motion IDs")
    scenario_count += 1
    try:
        _registry_ids_from_frozen_bytes(frozen_registry_payload[:-1])
    except ValueError as exc:
        if "byte length" not in str(exc):
            failures.append(f"frozen-registry-length: wrong failure {exc}")
    else:
        failures.append("frozen-registry-length: truncated registry was accepted")
    changed_registry_payload = bytearray(frozen_registry_payload)
    changed_registry_payload[-1] ^= 1
    scenario_count += 1
    try:
        _registry_ids_from_frozen_bytes(bytes(changed_registry_payload))
    except ValueError as exc:
        if "SHA-256" not in str(exc):
            failures.append(f"frozen-registry-hash: wrong failure {exc}")
    else:
        failures.append("frozen-registry-hash: modified registry was accepted")
    expect_fail(
        "target-already-exists",
        copy.deepcopy(valid),
        "already exists",
        existing_targets={valid["targets"]["raw_asset_path"]},
    )
    expect_fail(
        "target-collision-probe-error",
        copy.deepcopy(valid),
        "existence could not be checked exactly",
        target_probe_failure=True,
    )

    alias_candidate = copy.deepcopy(valid)
    rights_local = (
        PROJECT_ROOT
        / Path(*PurePosixPath(alias_candidate["rights_evidence_file"]["path"]).parts)
    ).resolve()
    release_local = (
        PROJECT_ROOT
        / Path(
            *PurePosixPath(
                alias_candidate["performer_release"]["evidence_file"]["path"]
            ).parts
        )
    ).resolve()
    alias_pair = frozenset((str(rights_local).casefold(), str(release_local).casefold()))
    expect_fail(
        "hardlink-evidence-alias",
        alias_candidate,
        "pairwise-distinct",
        same_file_pairs={alias_pair},
    )
    expect_fail(
        "samefile-check-error",
        copy.deepcopy(valid),
        "identity could not be checked exactly",
        same_file_failure=True,
    )
    scenario_count += 1
    if ".umap" not in TARGET_COLLISION_SUFFIXES:
        failures.append("target-suffixes: .umap collision coverage is missing")
    scenario_count += 1
    if ACCEPTED_VIDEO_CODECS != set(BMFF_VIDEO_CODEC_MAP.values()):
        failures.append("codec-truth: accepted codecs exceed the fallback BMFF parser")

    bad_chunk_offset = bytearray(_synthetic_bmff_video())
    stco_type_offset = bad_chunk_offset.find(b"stco")
    struct.pack_into(">I", bad_chunk_offset, stco_type_offset + 12, 0)
    final_byte_chunk = bytearray(_synthetic_bmff_video())
    final_stco_type_offset = final_byte_chunk.find(b"stco")
    struct.pack_into(">I", final_byte_chunk, final_stco_type_offset + 12, 32 + 480 - 1)
    overlapping_chunks = bytearray(_synthetic_bmff_video(chunk_count=2))
    overlap_stco_type_offset = overlapping_chunks.find(b"stco")
    struct.pack_into(">I", overlapping_chunks, overlap_stco_type_offset + 16, 33)
    incomplete_coverage = bytearray(_synthetic_bmff_video(chunk_count=2))
    coverage_stsc_type_offset = incomplete_coverage.find(b"stsc")
    struct.pack_into(">I", incomplete_coverage, coverage_stsc_type_offset + 16, 239)
    zero_variable_sample = bytearray(
        _synthetic_bmff_video(variable_sample_sizes=True)
    )
    variable_stsz_type_offset = zero_variable_sample.find(b"stsz")
    struct.pack_into(">I", zero_variable_sample, variable_stsz_type_offset + 16, 0)
    oversized_samples = bytearray(_synthetic_bmff_video())
    stsz_type_offset = oversized_samples.find(b"stsz")
    struct.pack_into(">I", oversized_samples, stsz_type_offset + 8, 4096)
    avcc_bad_base_reserved = bytearray(_synthetic_bmff_video())
    avcc_type_offset = avcc_bad_base_reserved.find(b"avcC")
    avcc_bad_base_reserved[avcc_type_offset + 8] &= 0x7F
    avcc_bad_extension_reserved = bytearray(_synthetic_bmff_video(avc_profile=100))
    high_avcc_type_offset = avcc_bad_extension_reserved.find(b"avcC")
    avcc_bad_extension_reserved[high_avcc_type_offset + 21] &= 0x7F
    malformed_visual_child = _synthetic_bmff_video(
        visual_entry_trailing_bytes=struct.pack(">I4s", 4, b"junk")
    )
    truncated_visual_child = _synthetic_bmff_video(
        visual_entry_trailing_bytes=struct.pack(">I4s", 12, b"junk")
    )
    media_cases = [
        ("media-truncated", _synthetic_bmff_video()[:-7], "malformed, or non-video"),
        ("media-non-video", _synthetic_bmff_video(video_tracks=0), "malformed, or non-video"),
        ("media-multiple-video", _synthetic_bmff_video(video_tracks=2), "malformed, or non-video"),
        ("media-unsupported-codec", _synthetic_bmff_video(codec=b"vp09"), "malformed, or non-video"),
        ("media-missing-codec-config", _synthetic_bmff_video(include_codec_config=False), "malformed, or non-video"),
        ("media-duplicate-codec-config", _synthetic_bmff_video(duplicate_codec_config=True), "malformed, or non-video"),
        ("media-malformed-codec-config", _synthetic_bmff_video(malformed_codec_config=True), "malformed, or non-video"),
        ("media-avcc-base-reserved-bits", bytes(avcc_bad_base_reserved), "malformed, or non-video"),
        ("media-avcc-high-extension-truncated", _synthetic_bmff_video(avc_profile=100, malformed_codec_config=True), "malformed, or non-video"),
        ("media-avcc-high-extension-reserved-bits", bytes(avcc_bad_extension_reserved), "malformed, or non-video"),
        ("media-visual-padding-nonzero", _synthetic_bmff_video(visual_entry_trailing_bytes=b"\x00\x00\x00\x01"), "malformed, or non-video"),
        ("media-visual-padding-one-zero", _synthetic_bmff_video(visual_entry_trailing_bytes=b"\x00"), "malformed, or non-video"),
        ("media-visual-padding-three-zeros", _synthetic_bmff_video(visual_entry_trailing_bytes=b"\x00" * 3), "malformed, or non-video"),
        ("media-visual-padding-five-zeros", _synthetic_bmff_video(visual_entry_trailing_bytes=b"\x00" * 5), "malformed, or non-video"),
        ("media-visual-padding-eight-zeros", _synthetic_bmff_video(visual_entry_trailing_bytes=b"\x00" * 8), "malformed, or non-video"),
        ("media-visual-padding-hidden-payload", _synthetic_bmff_video(visual_entry_trailing_bytes=b"\x00" * 4 + b"HIDE"), "malformed, or non-video"),
        ("media-visual-padding-before-hidden-payload", _synthetic_bmff_video(visual_entry_trailing_bytes=b"HIDE" + b"\x00" * 4), "malformed, or non-video"),
        ("media-visual-padding-malformed-box", malformed_visual_child, "malformed, or non-video"),
        ("media-visual-padding-truncated-box", truncated_visual_child, "malformed, or non-video"),
        ("media-top-level-padding-remains-strict", _synthetic_bmff_video() + b"\x00" * 4, "malformed, or non-video"),
        ("media-prores-padding-remains-strict", _synthetic_bmff_video(codec=b"apcn", visual_entry_trailing_bytes=b"\x00" * 4), "malformed, or non-video"),
        ("media-unknown-major-brand", _synthetic_bmff_video(major_brand=b"zzzz"), "malformed, or non-video"),
        ("media-unknown-compatible-brand", _synthetic_bmff_video(compatible_brands=(b"zzzz",)), "malformed, or non-video"),
        ("media-variable-rate", _synthetic_bmff_video(variable_timing=True), "frame_rate_mode does not match"),
        ("media-bad-chunk-offset", bytes(bad_chunk_offset), "malformed, or non-video"),
        ("media-final-byte-chunk-offset", bytes(final_byte_chunk), "malformed, or non-video"),
        ("media-overlapping-chunks", bytes(overlapping_chunks), "malformed, or non-video"),
        ("media-incomplete-sample-coverage", bytes(incomplete_coverage), "malformed, or non-video"),
        ("media-zero-variable-sample", bytes(zero_variable_sample), "malformed, or non-video"),
        ("media-samples-exceed-mdat", bytes(oversized_samples), "malformed, or non-video"),
    ]
    for name, media_payload, expected in media_cases:
        candidate = copy.deepcopy(valid)
        active = dict(payloads)
        active[source_key] = media_payload
        candidate["source_file"]["bytes"] = len(media_payload)
        candidate["source_file"]["sha256"] = hashlib.sha256(media_payload).hexdigest().upper()
        expect_fail(name, candidate, expected, active)

    padded_avc = copy.deepcopy(valid)
    padded_avc_payloads = dict(payloads)
    padded_avc_payloads[source_key] = _synthetic_bmff_video(
        visual_entry_trailing_bytes=b"\x00" * 4
    )
    padded_avc["source_file"]["bytes"] = len(padded_avc_payloads[source_key])
    padded_avc["source_file"]["sha256"] = hashlib.sha256(
        padded_avc_payloads[source_key]
    ).hexdigest().upper()
    expect_pass("media-valid-avc-four-zero-padding", padded_avc, padded_avc_payloads)

    padded_hvc = copy.deepcopy(valid)
    padded_hvc_payloads = dict(payloads)
    padded_hvc_payloads[source_key] = _synthetic_bmff_video(
        codec=b"hvc1", visual_entry_trailing_bytes=b"\x00" * 4
    )
    padded_hvc["source_file"]["bytes"] = len(padded_hvc_payloads[source_key])
    padded_hvc["source_file"]["sha256"] = hashlib.sha256(
        padded_hvc_payloads[source_key]
    ).hexdigest().upper()
    padded_hvc["technical"]["codec"] = "H265"
    expect_pass("media-valid-hvc-four-zero-padding", padded_hvc, padded_hvc_payloads)

    h265 = copy.deepcopy(valid)
    h265_payloads = dict(payloads)
    h265_payloads[source_key] = _synthetic_bmff_video(codec=b"hvc1")
    h265["source_file"]["bytes"] = len(h265_payloads[source_key])
    h265["source_file"]["sha256"] = hashlib.sha256(h265_payloads[source_key]).hexdigest().upper()
    h265["technical"]["codec"] = "H265"
    expect_pass("media-valid-hvc1-config", h265, h265_payloads)

    high_avc = copy.deepcopy(valid)
    high_avc_payloads = dict(payloads)
    high_avc_payloads[source_key] = _synthetic_bmff_video(avc_profile=100)
    high_avc["source_file"]["bytes"] = len(high_avc_payloads[source_key])
    high_avc["source_file"]["sha256"] = hashlib.sha256(high_avc_payloads[source_key]).hexdigest().upper()
    expect_pass("media-valid-avc-high-extension", high_avc, high_avc_payloads)

    swapped_probe_candidate = copy.deepcopy(valid)
    swapped_probe_payloads = dict(payloads)
    swapped_probe_payloads[source_key] = _synthetic_bmff_video(width=1080, height=1920)
    swapped_probe_candidate["technical"]["width_pixels"] = 1080
    swapped_probe_candidate["technical"]["height_pixels"] = 1920
    expect_fail(
        "source-a-hash-b-metadata-a-rehash",
        swapped_probe_candidate,
        "same-handle identical",
        probe_payloads=swapped_probe_payloads,
    )

    low_rate = copy.deepcopy(valid)
    low_rate_payloads = dict(payloads)
    low_rate_payloads[source_key] = _synthetic_bmff_video(frame_rate=30)
    low_rate["source_file"]["bytes"] = len(low_rate_payloads[source_key])
    low_rate["source_file"]["sha256"] = hashlib.sha256(low_rate_payloads[source_key]).hexdigest().upper()
    low_rate["technical"]["frame_rate_fps"] = 30
    expect_fail("media-low-rate", low_rate, "at least 60", low_rate_payloads)

    malformed_root_errors, _, _ = validate([])
    scenario_count += 1
    if not any("intake must be an object" in error for error in malformed_root_errors):
        failures.append(f"root-object: unexpected errors {malformed_root_errors!r}")
    try:
        json.loads('{"schema":"first","schema":"second"}', object_pairs_hook=_reject_duplicate_intake_keys)
    except _DuplicateIntakeKeyError:
        pass
    else:
        failures.append("duplicate JSON object keys were accepted")
    scenario_count += 1

    if failures:
        for failure in failures:
            print(f"SELF-TEST ERROR: {failure}", file=sys.stderr)
        print(f"FAIL: {scenario_count} motion source intake self-tests; {len(failures)} failures")
        return 1
    print(f"PASS: {scenario_count} motion source intake self-tests")
    return 0


def _run_registry_validation() -> int:
    errors: list[str] = []
    registry: dict[str, Any] = {}
    try:
        registry = json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        errors.append(f"could not read registry: {exc}")
    verified_files: list[dict[str, Any]] = []
    if not errors:
        errors, verified_files = _validate_registry(registry)

    sources = registry.get("sources", []) if isinstance(registry, dict) else []
    valid_sources = [source for source in sources if isinstance(source, dict)]
    report = {
        "schema": "disc_golf_motion_source_registry_validation",
        "schema_version": 1,
        "status": "PASS" if not errors else "FAIL",
        "registry": "SourceArt/DiscGolf/Mocap/motion_source_registry.json",
        "source_count": len(valid_sources),
        "counts_by_kind": dict(sorted(Counter(source.get("source_kind") for source in valid_sources).items())),
        "counts_by_usage_status": dict(
            sorted(Counter(source.get("usage_status") for source in valid_sources).items())
        ),
        "counts_by_production_status": dict(
            sorted(Counter(source.get("production_status") for source in valid_sources).items())
        ),
        "motion_ids": sorted(source.get("motion_id") for source in valid_sources),
        "pipeline_fixture_count": sum(
            source.get("pipeline_role") == "PIPELINE_FIXTURE" for source in valid_sources
        ),
        "external_production_sources_present": any(
            source.get("external_production_claim") is True for source in valid_sources
        ),
        "verified_files": verified_files,
        "errors": errors,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(
        f"{report['status']}: {report['source_count']} sources, "
        f"{len(verified_files)} files; report={REPORT_PATH}"
    )
    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    return 0 if not errors else 1


def main(argv: list[str] | None = None) -> int:
    arguments = list(sys.argv[1:] if argv is None else argv)
    if not arguments:
        return _run_registry_validation()
    if arguments == ["--self-test"]:
        return _run_intake_self_tests()
    if len(arguments) == 2 and arguments[0] == "--preflight":
        return _run_preflight(Path(arguments[1]))
    print(
        "usage: validate_motion_source_registry.py [--preflight <intake.json> | --self-test]",
        file=sys.stderr,
    )
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
