"""Validate motion-source provenance without loading Unreal Engine."""

from __future__ import annotations

import hashlib
import json
import re
import sys
from collections import Counter
from datetime import date
from pathlib import Path, PurePosixPath
from typing import Any


PROJECT_ROOT = Path(__file__).absolute().parents[1]
PROJECT_ROOT_RESOLVED = PROJECT_ROOT.resolve()
REGISTRY_PATH = PROJECT_ROOT / "SourceArt/DiscGolf/Mocap/motion_source_registry.json"
REPORT_PATH = PROJECT_ROOT / "Saved/CharacterFramework/Session5MotionSourceRegistryValidation.json"
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
    posix_path = PurePosixPath(raw_path)
    if posix_path.is_absolute() or ".." in posix_path.parts:
        return None, f"file path escapes the repository: {raw_path!r}"
    resolved = PROJECT_ROOT.joinpath(*posix_path.parts).resolve()
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


def main() -> int:
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


if __name__ == "__main__":
    raise SystemExit(main())
