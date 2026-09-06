"""Audit external motion video as a quarantined, read-only development reference.

This command is deliberately separate from the authentic-footage intake preflight.
It can report stable source identity and structural media facts, but it can never
grant staging, solver, derivative-animation, Unreal-import, production, or release
permission.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import tempfile
from pathlib import Path
from typing import Any, Callable

import validate_motion_source_registry as production_intake


PROJECT_ROOT = Path(__file__).absolute().parents[1].resolve()
AUDIT_SCHEMA = "disc_golf_motion_development_reference_audit"
AUDIT_SCHEMA_VERSION = 1
QUARANTINE_CLASS = "EXTERNAL_READ_ONLY_DEVELOPMENT_REFERENCE"
SUCCESS_STATUS = "QUARANTINED_DO_NOT_SHIP"
FAILURE_STATUS = "AUDIT_FAILED_DO_NOT_USE"

PERMANENT_BLOCKERS = (
    "RIGHTS_NOT_EVALUATED",
    "PERFORMER_RELEASE_NOT_EVALUATED",
    "THROW_FORM_REVIEW_NOT_BOUND",
    "CAPTURE_ASSERTIONS_NOT_BOUND",
    "PRODUCTION_INTAKE_NOT_RUN",
)

RESULT_FIELDS = {
    "schema",
    "schema_version",
    "status",
    "audit_completed",
    "source_identity",
    "media_probe_contract",
    "media",
    "technical_comparison",
    "quarantine",
    "blockers",
    "errors",
}

Verifier = Callable[[str, Path], dict[str, Any]]


def _canonical_sha256(value: Any) -> str:
    payload = json.dumps(
        value,
        ensure_ascii=False,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("utf-8")
    return hashlib.sha256(payload).hexdigest().upper()


def _is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
    except ValueError:
        return False
    return True


def _technical_comparison(media: dict[str, Any]) -> tuple[dict[str, Any], list[str]]:
    width = media.get("width_pixels")
    height = media.get("height_pixels")
    frame_rate = media.get("frame_rate_fps")
    duration = media.get("duration_seconds")
    codec = media.get("codec")
    frame_rate_mode = media.get("frame_rate_mode")

    dimensions_valid = (
        type(width) is int
        and type(height) is int
        and min(width, height) >= 1080
        and max(width, height) >= 1920
    )
    frame_rate_valid = (
        type(frame_rate) in {int, float}
        and not isinstance(frame_rate, bool)
        and math.isfinite(frame_rate)
        and frame_rate >= 60.0
    )
    duration_valid = (
        type(duration) in {int, float}
        and not isinstance(duration, bool)
        and math.isfinite(duration)
        and production_intake.MIN_SOURCE_DURATION_SECONDS
        <= duration
        <= production_intake.MAX_SOURCE_DURATION_SECONDS
    )
    checks = {
        "codec_accepted": codec in production_intake.ACCEPTED_VIDEO_CODECS,
        "constant_frame_rate": frame_rate_mode == "CONSTANT",
        "frame_rate_at_least_60_fps": frame_rate_valid,
        "resolution_at_least_1920_by_1080_either_orientation": dimensions_valid,
        "duration_between_1_and_30_seconds": duration_valid,
    }
    blockers: list[str] = []
    if not checks["codec_accepted"]:
        blockers.append("SOURCE_CODEC_NOT_ACCEPTED")
    if not checks["constant_frame_rate"]:
        blockers.append("SOURCE_FRAME_RATE_MODE_NOT_CONSTANT")
    if not checks["frame_rate_at_least_60_fps"]:
        blockers.append("SOURCE_FRAME_RATE_BELOW_60_FPS")
    if not checks["resolution_at_least_1920_by_1080_either_orientation"]:
        blockers.append("SOURCE_RESOLUTION_BELOW_1920_BY_1080")
    if not checks["duration_between_1_and_30_seconds"]:
        blockers.append("SOURCE_DURATION_OUTSIDE_1_TO_30_SECONDS")
    comparison = {
        "policy_source": "AUTHENTIC_FOOTAGE_PREFLIGHT_TECHNICAL_MINIMUMS",
        "requirements": {
            "accepted_codecs": sorted(production_intake.ACCEPTED_VIDEO_CODECS),
            "frame_rate_mode": "CONSTANT",
            "minimum_frame_rate_fps": 60,
            "minimum_resolution_either_orientation": [1920, 1080],
            "minimum_duration_seconds": production_intake.MIN_SOURCE_DURATION_SECONDS,
            "maximum_duration_seconds": production_intake.MAX_SOURCE_DURATION_SECONDS,
        },
        "checks": checks,
        "technical_policy_passed": all(checks.values()),
    }
    return comparison, blockers


def _quarantine_contract() -> dict[str, Any]:
    return {
        "class": QUARANTINE_CLASS,
        "classification": "UNCLASSIFIED_REFERENCE_ONLY",
        "reference_only": True,
        "usage_status": "DO_NOT_SHIP",
        "permitted_scope": "LOCAL_VISUAL_FORM_REFERENCE_ONLY",
        "exit_code_zero_meaning": "READ_ONLY_AUDIT_COMPLETED_ONLY",
        "source_payload_write_performed": False,
        "source_path_or_directory_disclosed_in_result": False,
        "raw_media_copy_allowed": False,
        "repo_source_copy_allowed": False,
        "unreal_import_allowed": False,
        "solver_execution_allowed": False,
        "derivative_animation_use_allowed": False,
        "production_staging_eligible": False,
        "production_promotion_allowed": False,
        "release_approved": False,
        "legal_approval": False,
        "rights_status": "NOT_EVALUATED",
        "performer_release_status": "NOT_EVALUATED",
        "human_capture_review_status": "NOT_BOUND",
        "runtime_authority_changed": False,
        "disk_mutation": "NONE",
    }


def _failed_result(
    source_label: str | None,
    expected_bytes: int,
    expected_sha256: str,
    errors: list[str],
    observed: dict[str, Any] | None = None,
) -> dict[str, Any]:
    observed = observed or {}
    result = {
        "schema": AUDIT_SCHEMA,
        "schema_version": AUDIT_SCHEMA_VERSION,
        "status": FAILURE_STATUS,
        "audit_completed": False,
        "source_identity": {
            "filename_only": source_label,
            "expected_bytes": expected_bytes,
            "observed_bytes": observed.get("bytes"),
            "expected_sha256": expected_sha256,
            "observed_sha256": observed.get("sha256"),
            "identity_matches_expectation": False,
            "identity_and_media_facts_sha256": None,
        },
        "media_probe_contract": {
            "implementation": production_intake.MEDIA_PROBE_IMPLEMENTATION,
            "version": production_intake.MEDIA_PROBE_VERSION,
            "same_open_handle_hash_probe_rehash_required": True,
            "same_open_handle_hash_probe_rehash_verified": False,
            "handle_stability_verified": False,
            "path_identity_stability_verified": False,
            "verified_for_this_audit": False,
            "external_tool_used": False,
        },
        "media": None,
        "technical_comparison": None,
        "quarantine": _quarantine_contract(),
        "blockers": ["SOURCE_AUDIT_FAILED", *PERMANENT_BLOCKERS],
        "errors": errors,
    }
    assert set(result) == RESULT_FIELDS
    return result


def audit_external_reference(
    source: Path,
    expected_bytes: int,
    expected_sha256: str,
    *,
    verifier: Verifier = production_intake._verify_source_same_handle,
) -> dict[str, Any]:
    """Return a sanitized, non-promotable audit result without writing any files."""

    source_label = source.name or None
    errors: list[str] = []
    if type(expected_bytes) is not int or expected_bytes <= 0:
        errors.append("EXPECTED_BYTE_COUNT_MUST_BE_A_POSITIVE_INTEGER")
    if (
        not isinstance(expected_sha256, str)
        or production_intake.SHA256_RE.fullmatch(expected_sha256) is None
    ):
        errors.append("EXPECTED_SHA256_MUST_BE_UPPERCASE_HEXADECIMAL")
    if errors:
        return _failed_result(
            source_label,
            expected_bytes,
            expected_sha256,
            errors,
        )
    if not source.is_absolute():
        return _failed_result(
            source_label,
            expected_bytes,
            expected_sha256,
            ["SOURCE_PATH_MUST_BE_ABSOLUTE"],
        )
    try:
        resolved = source.resolve(strict=True)
    except (OSError, RuntimeError):
        return _failed_result(
            source_label,
            expected_bytes,
            expected_sha256,
            ["SOURCE_IS_MISSING_OR_UNREADABLE"],
        )
    if _is_within(resolved, PROJECT_ROOT):
        return _failed_result(
            source_label,
            expected_bytes,
            expected_sha256,
            ["SOURCE_MUST_REMAIN_OUTSIDE_PROJECT_ROOT"],
        )
    if not resolved.is_file():
        return _failed_result(
            source_label,
            expected_bytes,
            expected_sha256,
            ["SOURCE_MUST_BE_A_REGULAR_FILE"],
        )
    if resolved.suffix.casefold() not in production_intake.ACCEPTED_VIDEO_EXTENSIONS:
        return _failed_result(
            source_label,
            expected_bytes,
            expected_sha256,
            ["SOURCE_EXTENSION_MUST_BE_MOV_OR_MP4"],
        )
    try:
        verification = verifier(resolved.name, resolved)
    except (OSError, ValueError):
        return _failed_result(
            source_label,
            expected_bytes,
            expected_sha256,
            ["SOURCE_IS_UNSTABLE_MALFORMED_OR_NON_VIDEO"],
        )

    required_verification_fields = production_intake.SOURCE_VERIFICATION_FIELDS
    if not isinstance(verification, dict) or set(verification) != required_verification_fields:
        return _failed_result(
            source_label,
            expected_bytes,
            expected_sha256,
            ["SOURCE_VERIFIER_RETURNED_INVALID_FIELDS"],
        )
    observed_size = verification.get("bytes")
    observed_hash = verification.get("sha256")
    probe_hash = verification.get("probe_input_sha256")
    stable_hash = verification.get("stable_rehash_sha256")
    observed = {"bytes": observed_size, "sha256": observed_hash}
    hashes_valid = all(
        isinstance(value, str) and production_intake.SHA256_RE.fullmatch(value) is not None
        for value in (observed_hash, probe_hash, stable_hash)
    )
    if type(observed_size) is not int or observed_size <= 0:
        errors.append("SOURCE_VERIFIER_RETURNED_INVALID_BYTE_COUNT")
    if not hashes_valid:
        errors.append("SOURCE_VERIFIER_RETURNED_INVALID_HASH")
    elif not observed_hash == probe_hash == stable_hash:
        errors.append("SOURCE_HASH_PROBE_REHASH_MISMATCH")
    if verification.get("handle_stable") is not True:
        errors.append("SOURCE_HANDLE_WAS_UNSTABLE")
    if verification.get("path_identity_stable") is not True:
        errors.append("SOURCE_PATH_IDENTITY_WAS_UNSTABLE")
    if verification.get("probe_implementation") != production_intake.MEDIA_PROBE_IMPLEMENTATION:
        errors.append("SOURCE_PROBE_IMPLEMENTATION_MISMATCH")
    if verification.get("probe_version") != production_intake.MEDIA_PROBE_VERSION:
        errors.append("SOURCE_PROBE_VERSION_MISMATCH")
    media = verification.get("media")
    if not isinstance(media, dict) or set(media) != production_intake.TECHNICAL_FIELDS:
        errors.append("SOURCE_MEDIA_FACTS_INVALID")
    if observed_size != expected_bytes:
        errors.append("SOURCE_BYTE_COUNT_DOES_NOT_MATCH_EXPECTATION")
    if observed_hash != expected_sha256:
        errors.append("SOURCE_SHA256_DOES_NOT_MATCH_EXPECTATION")
    if errors:
        return _failed_result(
            source_label,
            expected_bytes,
            expected_sha256,
            errors,
            observed,
        )

    technical, technical_blockers = _technical_comparison(media)
    bound_facts = {
        "filename_only": source_label,
        "bytes": observed_size,
        "sha256": observed_hash,
        "media": media,
        "probe_implementation": verification["probe_implementation"],
        "probe_version": verification["probe_version"],
    }
    result = {
        "schema": AUDIT_SCHEMA,
        "schema_version": AUDIT_SCHEMA_VERSION,
        "status": SUCCESS_STATUS,
        "audit_completed": True,
        "source_identity": {
            "filename_only": source_label,
            "expected_bytes": expected_bytes,
            "observed_bytes": observed_size,
            "expected_sha256": expected_sha256,
            "observed_sha256": observed_hash,
            "identity_matches_expectation": True,
            "identity_and_media_facts_sha256": _canonical_sha256(bound_facts),
        },
        "media_probe_contract": {
            "implementation": production_intake.MEDIA_PROBE_IMPLEMENTATION,
            "version": production_intake.MEDIA_PROBE_VERSION,
            "same_open_handle_hash_probe_rehash_required": True,
            "same_open_handle_hash_probe_rehash_verified": True,
            "handle_stability_verified": True,
            "path_identity_stability_verified": True,
            "verified_for_this_audit": True,
            "external_tool_used": False,
        },
        "media": media,
        "technical_comparison": technical,
        "quarantine": _quarantine_contract(),
        "blockers": [*technical_blockers, *PERMANENT_BLOCKERS],
        "errors": [],
    }
    assert set(result) == RESULT_FIELDS
    return result


def _positive_int(raw: str) -> int:
    try:
        value = int(raw, 10)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("must be a positive base-10 integer") from exc
    if value <= 0 or str(value) != raw:
        raise argparse.ArgumentTypeError("must be a canonical positive base-10 integer")
    return value


def _uppercase_sha256(raw: str) -> str:
    if production_intake.SHA256_RE.fullmatch(raw) is None:
        raise argparse.ArgumentTypeError("must be exactly 64 uppercase hexadecimal characters")
    return raw


def _run_self_tests() -> int:
    failures: list[str] = []
    scenarios = 0

    def check(condition: bool, label: str) -> None:
        nonlocal scenarios
        scenarios += 1
        if not condition:
            failures.append(label)

    with tempfile.TemporaryDirectory(prefix="dg-motion-reference-audit-") as raw_temp:
        temp_root = Path(raw_temp).resolve()
        valid_path = temp_root / "reference.mov"
        valid_payload = production_intake._synthetic_bmff_video(
            width=1920,
            height=1080,
            frame_rate=60,
            duration_seconds=8,
            codec=b"hvc1",
        )
        valid_path.write_bytes(valid_payload)
        valid_hash = hashlib.sha256(valid_payload).hexdigest().upper()
        valid = audit_external_reference(valid_path, len(valid_payload), valid_hash)
        serialized = json.dumps(valid, sort_keys=True)
        check(valid["status"] == SUCCESS_STATUS, "valid reference audit status")
        check(valid["audit_completed"] is True, "valid reference audit completion")
        check(valid["technical_comparison"]["technical_policy_passed"] is True, "technical pass")
        check(valid["quarantine"]["production_staging_eligible"] is False, "staging denied")
        check(valid["quarantine"]["classification"] == "UNCLASSIFIED_REFERENCE_ONLY", "classification isolated")
        check(valid["quarantine"]["usage_status"] == "DO_NOT_SHIP", "usage DO_NOT_SHIP")
        check(valid["quarantine"]["reference_only"] is True, "reference-only flag")
        check(valid["quarantine"]["repo_source_copy_allowed"] is False, "repo copy denied")
        check(valid["quarantine"]["unreal_import_allowed"] is False, "Unreal import denied")
        check(valid["quarantine"]["solver_execution_allowed"] is False, "solver denied")
        check(valid["quarantine"]["derivative_animation_use_allowed"] is False, "derivative denied")
        check(valid["quarantine"]["release_approved"] is False, "release denied")
        check(valid["quarantine"]["legal_approval"] is False, "legal approval denied")
        check(valid["quarantine"]["rights_status"] == "NOT_EVALUATED", "rights not evaluated")
        check(
            valid["media_probe_contract"]["same_open_handle_hash_probe_rehash_verified"] is True,
            "same-handle identity verified",
        )
        check(
            valid["media_probe_contract"]["path_identity_stability_verified"] is True,
            "path identity verified",
        )
        check(set(valid) == RESULT_FIELDS, "exact result fields")
        check(str(temp_root) not in serialized, "absolute source directory suppressed")
        check("PASS_STAGING_ELIGIBLE" not in serialized, "production pass vocabulary absent")
        check("performer_name" not in serialized, "performer identity field absent")
        check("provider" not in serialized and "creator" not in serialized, "provenance claims absent")
        check(valid["errors"] == [], "valid audit errors empty")
        check(
            audit_external_reference(valid_path, len(valid_payload), valid_hash) == valid,
            "audit output deterministic",
        )

        low_rate_path = temp_root / "low-rate.mov"
        low_rate_payload = production_intake._synthetic_bmff_video(frame_rate=30)
        low_rate_path.write_bytes(low_rate_payload)
        low_rate = audit_external_reference(
            low_rate_path,
            len(low_rate_payload),
            hashlib.sha256(low_rate_payload).hexdigest().upper(),
        )
        check(low_rate["status"] == SUCCESS_STATUS, "low-rate audit remains quarantined")
        check(
            low_rate["technical_comparison"]["technical_policy_passed"] is False,
            "low-rate technical failure",
        )
        check(
            "SOURCE_FRAME_RATE_BELOW_60_FPS" in low_rate["blockers"],
            "low-rate blocker",
        )
        check(low_rate["quarantine"]["production_staging_eligible"] is False, "low-rate not staged")

        wrong_hash = audit_external_reference(valid_path, len(valid_payload), "0" * 64)
        check(wrong_hash["status"] == FAILURE_STATUS, "hash mismatch fails audit")
        check(wrong_hash["media"] is None, "hash mismatch withholds media blessing")
        check(
            "SOURCE_SHA256_DOES_NOT_MATCH_EXPECTATION" in wrong_hash["errors"],
            "hash mismatch error",
        )
        check(wrong_hash["quarantine"]["production_staging_eligible"] is False, "hash mismatch denied")

        wrong_size = audit_external_reference(valid_path, len(valid_payload) + 1, valid_hash)
        check(wrong_size["status"] == FAILURE_STATUS, "byte mismatch fails audit")
        check(
            "SOURCE_BYTE_COUNT_DOES_NOT_MATCH_EXPECTATION" in wrong_size["errors"],
            "byte mismatch error",
        )

        relative = audit_external_reference(Path("reference.mov"), len(valid_payload), valid_hash)
        check(relative["status"] == FAILURE_STATUS, "relative source fails audit")
        check(relative["errors"] == ["SOURCE_PATH_MUST_BE_ABSOLUTE"], "relative path error")

        missing = audit_external_reference(temp_root / "missing.mov", 1, "0" * 64)
        check(missing["status"] == FAILURE_STATUS, "missing source fails audit")
        check(
            missing["errors"] == ["SOURCE_IS_MISSING_OR_UNREADABLE"],
            "missing source error",
        )

        text_path = temp_root / "reference.txt"
        text_path.write_bytes(valid_payload)
        bad_extension = audit_external_reference(text_path, len(valid_payload), valid_hash)
        check(bad_extension["status"] == FAILURE_STATUS, "bad extension fails audit")
        check(
            bad_extension["errors"] == ["SOURCE_EXTENSION_MUST_BE_MOV_OR_MP4"],
            "bad extension error",
        )

        malformed_path = temp_root / "malformed.mov"
        malformed_path.write_bytes(b"not an ISO-BMFF video")
        malformed_hash = hashlib.sha256(malformed_path.read_bytes()).hexdigest().upper()
        malformed = audit_external_reference(
            malformed_path,
            malformed_path.stat().st_size,
            malformed_hash,
        )
        check(malformed["status"] == FAILURE_STATUS, "malformed source fails audit")
        check(
            malformed["errors"] == ["SOURCE_IS_UNSTABLE_MALFORMED_OR_NON_VIDEO"],
            "malformed source error",
        )

        valid_verification = production_intake._verify_source_same_handle(
            valid_path.name,
            valid_path,
        )

        def mutated_verifier(**updates: Any) -> Verifier:
            def verify(_name: str, _path: Path) -> dict[str, Any]:
                return {**valid_verification, **updates}

            return verify

        mismatched_probe = audit_external_reference(
            valid_path,
            len(valid_payload),
            valid_hash,
            verifier=mutated_verifier(probe_input_sha256="F" * 64),
        )
        check(mismatched_probe["status"] == FAILURE_STATUS, "same-handle hash mismatch fails")
        check(
            "SOURCE_HASH_PROBE_REHASH_MISMATCH" in mismatched_probe["errors"],
            "same-handle hash mismatch error",
        )
        unstable_handle = audit_external_reference(
            valid_path,
            len(valid_payload),
            valid_hash,
            verifier=mutated_verifier(handle_stable=False),
        )
        check(unstable_handle["status"] == FAILURE_STATUS, "unstable handle fails")
        check("SOURCE_HANDLE_WAS_UNSTABLE" in unstable_handle["errors"], "unstable handle error")
        unstable_path = audit_external_reference(
            valid_path,
            len(valid_payload),
            valid_hash,
            verifier=mutated_verifier(path_identity_stable=False),
        )
        check(unstable_path["status"] == FAILURE_STATUS, "unstable path fails")
        check(
            "SOURCE_PATH_IDENTITY_WAS_UNSTABLE" in unstable_path["errors"],
            "unstable path error",
        )
        bad_probe_contract = audit_external_reference(
            valid_path,
            len(valid_payload),
            valid_hash,
            verifier=mutated_verifier(probe_version=-1),
        )
        check(bad_probe_contract["status"] == FAILURE_STATUS, "probe contract mismatch fails")
        check(
            "SOURCE_PROBE_VERSION_MISMATCH" in bad_probe_contract["errors"],
            "probe contract mismatch error",
        )
        extra_fields = audit_external_reference(
            valid_path,
            len(valid_payload),
            valid_hash,
            verifier=lambda _name, _path: {**valid_verification, "unexpected": True},
        )
        check(extra_fields["status"] == FAILURE_STATUS, "verifier extra fields fail")
        check(
            extra_fields["errors"] == ["SOURCE_VERIFIER_RETURNED_INVALID_FIELDS"],
            "verifier exact fields enforced",
        )

    repo_local = audit_external_reference(
        Path(__file__).absolute(),
        Path(__file__).stat().st_size,
        production_intake._sha256(Path(__file__)),
    )
    check(repo_local["status"] == FAILURE_STATUS, "repo-local source fails audit")
    check(
        repo_local["errors"] == ["SOURCE_MUST_REMAIN_OUTSIDE_PROJECT_ROOT"],
        "repo-local quarantine boundary",
    )
    check(_positive_int("1") == 1, "positive integer parser")
    invalid_expected_size = audit_external_reference(Path("ignored.mov"), True, "0" * 64)
    check(invalid_expected_size["status"] == FAILURE_STATUS, "boolean expected byte count rejected")
    check(
        invalid_expected_size["errors"] == ["EXPECTED_BYTE_COUNT_MUST_BE_A_POSITIVE_INTEGER"],
        "invalid expected byte count error",
    )
    invalid_expected_hash = audit_external_reference(Path("ignored.mov"), 1, "a" * 64)
    check(invalid_expected_hash["status"] == FAILURE_STATUS, "lowercase expected hash rejected")
    check(
        invalid_expected_hash["errors"] == ["EXPECTED_SHA256_MUST_BE_UPPERCASE_HEXADECIMAL"],
        "invalid expected hash error",
    )
    try:
        _positive_int("01")
    except argparse.ArgumentTypeError:
        check(True, "noncanonical integer rejected")
    else:
        check(False, "noncanonical integer rejected")
    try:
        _uppercase_sha256("a" * 64)
    except argparse.ArgumentTypeError:
        check(True, "lowercase hash rejected")
    else:
        check(False, "lowercase hash rejected")

    if failures:
        print(f"FAIL: {scenarios} development-reference audit self-tests; {len(failures)} failures")
        for failure in failures:
            print(f"ERROR: {failure}")
        return 1
    print(f"PASS: {scenarios} development-reference audit self-tests")
    return 0


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=__doc__,
        allow_abbrev=False,
    )
    parser.add_argument("--source", type=Path, help="absolute external .mov or .mp4 path")
    parser.add_argument("--expected-bytes", type=_positive_int)
    parser.add_argument("--expected-sha256", type=_uppercase_sha256)
    parser.add_argument("--self-test", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    if args.self_test:
        if any(value is not None for value in (args.source, args.expected_bytes, args.expected_sha256)):
            _parser().error("--self-test cannot be combined with source-audit arguments")
        return _run_self_tests()
    if any(value is None for value in (args.source, args.expected_bytes, args.expected_sha256)):
        _parser().error(
            "--source, --expected-bytes, and --expected-sha256 are all required for an audit"
        )
    result = audit_external_reference(
        args.source,
        args.expected_bytes,
        args.expected_sha256,
    )
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["audit_completed"] is True else 1


if __name__ == "__main__":
    raise SystemExit(main())
