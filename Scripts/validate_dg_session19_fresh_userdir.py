#!/usr/bin/env python3
"""Fail-closed validator for a fresh Session 19 Shipping UserDir.

The validator intentionally emits only candidate/token identities and paths
relative to the supplied UserDir. The default remains an exact production-file
allowlist. An explicit four-input mode permits only the candidate-bound game
runtime journal after independently reconciling its external capture. Absolute
host paths are used for read-only checks but never copied into the receipt.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import stat
import sys
import tempfile
import uuid
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any

import validate_dg_session19_external_technical_evidence as external_evidence


SCHEMA = "DiscGolfTour.Session19FreshUserDirValidation.v1"
SESSION = 19
PROJECT_ROOT = Path(__file__).resolve().parent.parent

CANDIDATE_ID_RE = re.compile(r"^S19_WindowsShipping_[A-Za-z0-9_-]{1,96}$")
USERDIR_TOKEN_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]{7,127}$")

REQUIRED_PROFILE = "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
RUNTIME_JOURNAL_RELATIVE = (
    "Saved/TechnicalEvidence/three-hole-runtime-journal-v1.jsonl"
)
RUNTIME_JOURNAL_SCHEMA = "DiscGolfTour.Session19ThreeHoleRuntimeJournal.v1"
RUNTIME_JOURNAL_PASS = "PASS_GAME_EMITTED_THREE_HOLE_RUNTIME_JOURNAL"
RUNTIME_MANIFEST_SCHEMA = (
    "DiscGolfTour.Session19ExternalTechnicalEvidenceManifest.v2"
)
RUNTIME_LAUNCH_SCHEMA = (
    "DiscGolfTour.Session19ExternalTechnicalEvidenceLaunchRecord.v2"
)
APPROVED_EXACT_FILES: dict[str, str] = {
    REQUIRED_PROFILE: "production_profile",
    "Saved/Config/Windows/GameUserSettings.ini": "production_user_settings",
    "Saved/DiscGolfTour_PCD3D_SM6.upipelinecache": "production_pso_pipeline_cache",
}
APPROVED_PATTERN_FILES: tuple[tuple[re.Pattern[str], str, str], ...] = (
    (
        re.compile(
            r"^Saved/Config/CrashReportClient/"
            r"UECC-Windows-[0-9A-F]{32}/CrashReportClient\.ini$",
            re.IGNORECASE,
        ),
        "crash_reporter_config_cache",
        "Saved/Config/CrashReportClient/UECC-Windows-<32-hex>/CrashReportClient.ini",
    ),
)

MAX_APPROVED_FILE_BYTES = 16 * 1024 * 1024
MAX_CRASH_REPORTER_CONFIGS = 32

# These are diagnostic labels as well as defense in depth.  The fixed
# allowlist below rejects every other file even if a forbidden term is missed.
FORBIDDEN_PATH_MARKERS: tuple[tuple[str, str], ...] = (
    ("dgt_equipment_dev_v1", "DEVELOPMENT_EQUIPMENT_SAVE"),
    ("dgt_throwlab", "DEVELOPMENT_THROWLAB_SAVE"),
    ("_dev_", "DEVELOPMENT_ARTIFACT"),
    ("environmentreport", "ENVIRONMENT_REPORT"),
    ("trajectoryexport", "TRAJECTORY_EXPORT"),
    ("diagnostic", "DIAGNOSTICS_OUTPUT"),
    ("physics", "PHYSICS_OUTPUT"),
    ("regression", "REGRESSION_OUTPUT"),
    ("automation", "TEST_OUTPUT"),
    ("test", "TEST_OUTPUT"),
    ("capture", "CAPTURE_OUTPUT"),
    ("screenshot", "CAPTURE_OUTPUT"),
)


def _sha256_file(path: Path) -> str:
    # Preserve this receipt's historical lowercase encoding while delegating
    # the read to the descriptor/path identity-stable evidence hasher.
    return external_evidence.sha256_file(path).lower()


def _binding_sha256(candidate_id: str, userdir_token: str) -> str:
    material = f"{SCHEMA}\n{candidate_id}\n{userdir_token}\n".encode("utf-8")
    return hashlib.sha256(material).hexdigest()


def _is_within(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def _is_reparse_point(path: Path) -> bool:
    try:
        file_stat = os.lstat(path)
    except OSError:
        return True
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return path.is_symlink() or bool(
        getattr(file_stat, "st_file_attributes", 0) & reparse_flag
    )


def _classify_allowed(relative_path: str) -> str | None:
    exact = APPROVED_EXACT_FILES.get(relative_path)
    if exact is not None:
        return exact
    for pattern, classification, _display in APPROVED_PATTERN_FILES:
        if pattern.fullmatch(relative_path):
            return classification
    return None


def _forbidden_codes(relative_path: str) -> list[str]:
    lowered = relative_path.casefold()
    codes = {code for marker, code in FORBIDDEN_PATH_MARKERS if marker in lowered}

    savegames_prefix = "saved/savegames/"
    if lowered.startswith(savegames_prefix) and relative_path != REQUIRED_PROFILE:
        codes.add("UNKNOWN_SAVEGAME")
    elif lowered.endswith(".sav") and relative_path != REQUIRED_PROFILE:
        codes.add("UNKNOWN_SAVEGAME")
    return sorted(codes)


def _failure(code: str, relative_path: str | None = None) -> dict[str, str]:
    result = {"code": code}
    if relative_path is not None:
        result["relativePath"] = relative_path
    return result


def _load_json_stable(path: Path) -> tuple[dict[str, Any], bytes]:
    before = path.stat()
    raw = path.read_bytes()
    after = path.stat()
    if before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns:
        raise ValueError("changed")

    def reject_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        value: dict[str, Any] = {}
        for key, item in pairs:
            if key in value:
                raise ValueError("duplicate")
            value[key] = item
        return value

    value = json.loads(
        raw.decode("utf-8"),
        object_pairs_hook=reject_duplicates,
        parse_constant=lambda _value: (_ for _ in ()).throw(ValueError("nonfinite")),
    )
    if not isinstance(value, dict):
        raise ValueError("not-object")
    return value, raw


def _runtime_journal_binding(
    *,
    user_dir: Path,
    candidate_id: str,
    userdir_token: str,
    technical_manifest: Path,
    launch_record: Path,
    shipping_exe: Path,
    archive_root: Path,
) -> dict[str, Any]:
    validation = external_evidence.validate_evidence(
        archive_root=archive_root,
        shipping_exe=shipping_exe,
        user_dir=user_dir,
        manifest_path=technical_manifest,
    )
    if validation.get("state") != (
        "PASS_CANDIDATE_BOUND_EXTERNAL_TECHNICAL_EVIDENCE"
    ):
        raise ValueError("external-validation")

    manifest, manifest_raw = _load_json_stable(technical_manifest)
    launch, launch_raw = _load_json_stable(launch_record)
    if (
        hashlib.sha256(manifest_raw).hexdigest().upper()
        != validation.get("manifest", {}).get("sha256")
        or
        manifest.get("schema") != RUNTIME_MANIFEST_SCHEMA
        or manifest.get("schemaVersion") != 2
        or manifest.get("candidate", {}).get("candidateId") != candidate_id
        or manifest.get("userDir", {}).get("token") != userdir_token
        or launch.get("schema") != RUNTIME_LAUNCH_SCHEMA
        or launch.get("schemaVersion") != 2
        or launch.get("candidateId") != candidate_id
        or launch.get("userDirToken") != userdir_token
    ):
        raise ValueError("root-binding")

    launch_binding = manifest.get("evidence", {}).get("launchRecord", {})
    expected_launch = technical_manifest.parent / str(
        launch_binding.get("relativePath", "")
    )
    if (
        launch_record.resolve(strict=True) != expected_launch.resolve(strict=True)
        or launch_binding.get("bytes") != len(launch_raw)
        or launch_binding.get("sha256")
        != hashlib.sha256(launch_raw).hexdigest().upper()
    ):
        raise ValueError("launch-binding")

    inventory = manifest.get("evidence", {}).get("runtimeCheckpointJournal", {})
    journal_path = external_evidence.resolve_descendant_file(
        user_dir,
        RUNTIME_JOURNAL_RELATIVE,
        "RUNTIME_JOURNAL_PATH_INVALID",
    )
    journal_raw, _journal_stat = external_evidence._read_file_stable(
        journal_path, external_evidence.RUNTIME_JOURNAL_MAX_BYTES
    )
    if (
        inventory.get("userDirRelativePath") != RUNTIME_JOURNAL_RELATIVE
        or inventory.get("bytes") != len(journal_raw)
        or inventory.get("sha256")
        != hashlib.sha256(journal_raw).hexdigest().upper()
        or inventory.get("schema") != RUNTIME_JOURNAL_SCHEMA
        or inventory.get("captureNonce") != launch.get("captureNonce")
        or inventory.get("validationFailures") != []
        or inventory.get("validationState") != RUNTIME_JOURNAL_PASS
        or launch.get("runtimeCheckpointJournalUserDirRelativePath")
        != RUNTIME_JOURNAL_RELATIVE
    ):
        raise ValueError("journal-inventory")

    first_line, separator, _remainder = journal_raw.partition(b"\n")
    if not separator or b"\r" in journal_raw:
        raise ValueError("journal-lines")
    header = json.loads(first_line.decode("ascii"))
    canonical_header = json.dumps(
        header, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("ascii")
    candidate = manifest["candidate"]
    executable_sha256 = external_evidence.sha256_file(shipping_exe.resolve(strict=True))
    archive_sha256 = external_evidence.collect_archive(
        archive_root.resolve(strict=True)
    )["canonicalManifestSha256"]
    if (
        first_line != canonical_header
        or header.get("schema") != RUNTIME_JOURNAL_SCHEMA
        or header.get("candidateId") != candidate_id
        or header.get("userDirToken") != userdir_token
        or header.get("captureNonce") != launch.get("captureNonce")
        or header.get("roundId") != inventory.get("roundId")
        or header.get("executableSha256") != executable_sha256
        or header.get("archiveManifestSha256") != archive_sha256
        or candidate.get("expectedExeSha256") != executable_sha256
        or candidate.get("observedExeSha256") != executable_sha256
        or candidate.get("expectedArchiveManifestSha256") != archive_sha256
    ):
        raise ValueError("journal-header")
    try:
        capture_nonce = str(uuid.UUID(str(header["captureNonce"])))
        round_id = str(uuid.UUID(str(header["roundId"])))
    except (KeyError, ValueError) as exc:
        raise ValueError("journal-uuid") from exc
    if capture_nonce != header["captureNonce"] or round_id != header["roundId"]:
        raise ValueError("journal-uuid")

    return {
        "mode": "CANDIDATE_BOUND_EXTERNAL_CAPTURE",
        "userDirRelativePath": RUNTIME_JOURNAL_RELATIVE,
        "bytes": len(journal_raw),
        "sha256": hashlib.sha256(journal_raw).hexdigest().upper(),
        "captureNonce": capture_nonce,
        "roundId": round_id,
        "launchRecordSha256": hashlib.sha256(launch_raw).hexdigest().upper(),
        "technicalEvidenceManifestSha256": hashlib.sha256(
            manifest_raw
        ).hexdigest().upper(),
        "executableSha256": executable_sha256,
        "archiveManifestSha256": archive_sha256,
        "validationState": RUNTIME_JOURNAL_PASS,
    }


def _base_receipt(candidate_id: str, userdir_token: str) -> dict[str, Any]:
    return {
        "schema": SCHEMA,
        "schemaVersion": 1,
        "session": SESSION,
        "candidateId": candidate_id,
        "userDirToken": userdir_token,
        "candidateUserDirBindingSha256": _binding_sha256(
            candidate_id, userdir_token
        ),
        "hostPathRecorded": False,
        "policy": {
            "mode": "FAIL_CLOSED_EXACT_ALLOWLIST",
            "requiredProductionProfile": REQUIRED_PROFILE,
            "approvedExactFiles": sorted(APPROVED_EXACT_FILES),
            "approvedPatternFiles": [
                display for _pattern, _classification, display in APPROVED_PATTERN_FILES
            ],
            "unknownFilesRejected": True,
            "unknownSaveGamesRejected": True,
            "reparsePointsRejected": True,
            "maximumApprovedFileBytes": MAX_APPROVED_FILE_BYTES,
            "maximumCrashReporterConfigFiles": MAX_CRASH_REPORTER_CONFIGS,
            "forbiddenArtifactClasses": sorted(
                {code for _marker, code in FORBIDDEN_PATH_MARKERS}
                | {"UNKNOWN_SAVEGAME"}
            ),
        },
        "observedFiles": [],
        "counts": {
            "observedFiles": 0,
            "approvedFiles": 0,
            "unknownFiles": 0,
            "forbiddenArtifacts": 0,
        },
        "failures": [],
        "state": "FAIL_FRESH_SHIPPING_USERDIR_ALLOWLIST",
        "releaseBoundary": {
            "technicalUserDirAcceptanceOnly": True,
            "gameplayAcceptanceClaimed": False,
            "releaseReadinessClaimed": False,
        },
    }


def validate_userdir(
    user_dir: Path,
    candidate_id: str,
    userdir_token: str,
    *,
    runtime_journal_manifest: Path | None = None,
    runtime_launch_record: Path | None = None,
    shipping_exe: Path | None = None,
    archive_root: Path | None = None,
) -> dict[str, Any]:
    """Return a path-safe validation receipt; never mutate ``user_dir``."""

    receipt = _base_receipt(candidate_id, userdir_token)
    failures: list[dict[str, str]] = receipt["failures"]

    try:
        root = user_dir.resolve(strict=True)
    except (OSError, RuntimeError):
        failures.append(_failure("USERDIR_NOT_RESOLVABLE"))
        return receipt

    if not root.is_dir():
        failures.append(_failure("USERDIR_NOT_DIRECTORY"))
        return receipt
    if root.name != userdir_token:
        failures.append(_failure("USERDIR_TOKEN_BASENAME_MISMATCH"))
        return receipt
    if _is_within(root, PROJECT_ROOT):
        failures.append(_failure("USERDIR_NOT_EXTERNAL_TO_PROJECT"))
        return receipt
    if _is_reparse_point(root):
        failures.append(_failure("USERDIR_ROOT_REPARSE_POINT"))
        return receipt

    runtime_inputs = (
        runtime_journal_manifest,
        runtime_launch_record,
        shipping_exe,
        archive_root,
    )
    runtime_mode_requested = any(value is not None for value in runtime_inputs)
    runtime_binding: dict[str, Any] | None = None
    if runtime_mode_requested:
        # This field and policy clause exist only in the explicit v2 capture
        # path. The historical/default receipt remains byte-for-byte shaped as
        # before this feature.
        receipt["runtimeJournalBinding"] = None
        receipt["policy"]["runtimeJournalAllowance"] = (
            "EXACT_CANDIDATE_BOUND_EXTERNAL_CAPTURE_ONLY"
        )
        if not all(value is not None for value in runtime_inputs):
            failures.append(_failure("RUNTIME_JOURNAL_BINDING_INPUTS_INCOMPLETE"))
        else:
            try:
                runtime_binding = _runtime_journal_binding(
                    user_dir=root,
                    candidate_id=candidate_id,
                    userdir_token=userdir_token,
                    technical_manifest=runtime_journal_manifest,
                    launch_record=runtime_launch_record,
                    shipping_exe=shipping_exe,
                    archive_root=archive_root,
                )
                receipt["runtimeJournalBinding"] = runtime_binding
            except (OSError, RuntimeError, UnicodeError, ValueError, KeyError):
                failures.append(_failure("RUNTIME_JOURNAL_BINDING_INVALID"))

    observed: list[dict[str, Any]] = receipt["observedFiles"]
    casefolded_paths: set[str] = set()
    scan_failed = False

    def on_walk_error(_error: OSError) -> None:
        nonlocal scan_failed
        scan_failed = True

    for current_root, directory_names, file_names in os.walk(
        root, topdown=True, followlinks=False, onerror=on_walk_error
    ):
        current = Path(current_root)

        for directory_name in list(directory_names):
            directory_path = current / directory_name
            relative = directory_path.relative_to(root).as_posix()
            if _is_reparse_point(directory_path):
                failures.append(_failure("REPARSE_POINT", relative))
                directory_names.remove(directory_name)

        for file_name in file_names:
            file_path = current / file_name
            relative = file_path.relative_to(root).as_posix()
            relative_folded = relative.casefold()

            if relative_folded in casefolded_paths:
                failures.append(_failure("CASE_INSENSITIVE_PATH_COLLISION", relative))
                continue
            casefolded_paths.add(relative_folded)

            if _is_reparse_point(file_path):
                failures.append(_failure("REPARSE_POINT", relative))
                continue

            try:
                file_stat = file_path.stat()
            except OSError:
                failures.append(_failure("FILE_STAT_FAILED", relative))
                continue
            if not stat.S_ISREG(file_stat.st_mode):
                failures.append(_failure("NON_REGULAR_FILE", relative))
                continue

            forbidden_codes = _forbidden_codes(relative)
            classification = _classify_allowed(relative)
            if relative == RUNTIME_JOURNAL_RELATIVE and runtime_binding is not None:
                classification = "runtime_checkpoint_journal_candidate_bound"
            if classification is None:
                failures.append(_failure("UNKNOWN_FILE", relative))
                receipt["counts"]["unknownFiles"] += 1
            else:
                receipt["counts"]["approvedFiles"] += 1

            for forbidden_code in forbidden_codes:
                failures.append(_failure(forbidden_code, relative))
            if forbidden_codes:
                receipt["counts"]["forbiddenArtifacts"] += 1

            size = file_stat.st_size
            if size <= 0:
                failures.append(_failure("EMPTY_FILE", relative))
            if size > MAX_APPROVED_FILE_BYTES:
                failures.append(_failure("FILE_EXCEEDS_SIZE_LIMIT", relative))

            try:
                file_sha256: str | None = _sha256_file(file_path)
            except OSError:
                failures.append(_failure("FILE_HASH_FAILED", relative))
                file_sha256 = None

            file_record: dict[str, Any] = {
                "relativePath": relative,
                "bytes": size,
                "sha256": file_sha256,
                "classification": classification or "unapproved",
            }
            observed.append(file_record)

            if relative == REQUIRED_PROFILE and size > 0:
                try:
                    with file_path.open("rb") as stream:
                        magic = stream.read(4)
                except OSError:
                    magic = b""
                if magic != b"GVAS":
                    failures.append(_failure("INVALID_PRODUCTION_PROFILE_MAGIC", relative))

    if scan_failed:
        failures.append(_failure("USERDIR_SCAN_FAILED"))

    observed.sort(key=lambda item: item["relativePath"].casefold())
    receipt["counts"]["observedFiles"] = len(observed)

    if not any(item["relativePath"] == REQUIRED_PROFILE for item in observed):
        failures.append(_failure("MISSING_PRODUCTION_PROFILE", REQUIRED_PROFILE))
    if runtime_mode_requested and not any(
        item["relativePath"] == RUNTIME_JOURNAL_RELATIVE
        and item["classification"] == "runtime_checkpoint_journal_candidate_bound"
        for item in observed
    ):
        failures.append(_failure(
            "BOUND_RUNTIME_JOURNAL_NOT_OBSERVED", RUNTIME_JOURNAL_RELATIVE
        ))
    if runtime_binding is not None:
        journal_rows = [
            item for item in observed
            if item["relativePath"] == RUNTIME_JOURNAL_RELATIVE
        ]
        if (
            len(journal_rows) != 1
            or journal_rows[0]["bytes"] != runtime_binding["bytes"]
            or not isinstance(journal_rows[0]["sha256"], str)
            or journal_rows[0]["sha256"].upper() != runtime_binding["sha256"]
        ):
            failures.append(_failure(
                "BOUND_RUNTIME_JOURNAL_CHANGED_DURING_VALIDATION",
                RUNTIME_JOURNAL_RELATIVE,
            ))

    crash_reporter_count = sum(
        item["classification"] == "crash_reporter_config_cache" for item in observed
    )
    if crash_reporter_count > MAX_CRASH_REPORTER_CONFIGS:
        failures.append(_failure("TOO_MANY_CRASH_REPORTER_CONFIG_FILES"))

    failures.sort(key=lambda item: (item["code"], item.get("relativePath", "")))
    if not failures:
        receipt["state"] = "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST"
    return receipt


def _write_fixture(root: Path, files: dict[str, bytes]) -> None:
    for relative, content in files.items():
        destination = root / Path(relative)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(content)


def _self_test() -> dict[str, Any]:
    candidate_id = "S19_WindowsShipping_20990101T000000Z_candidate3"
    valid_profile = b"GVAS" + (b"\x00" * 124)
    valid_settings = b"[/Script/Engine.GameUserSettings]\r\nbUseVSync=False\r\n"
    valid_pso_cache = b"DGTour-production-pso-cache"
    cases: list[tuple[str, dict[str, bytes], bool, set[str]]] = [
        ("minimal-valid", {REQUIRED_PROFILE: valid_profile}, True, set()),
        (
            "valid-production-pso-cache",
            {
                REQUIRED_PROFILE: valid_profile,
                "Saved/DiscGolfTour_PCD3D_SM6.upipelinecache": valid_pso_cache,
            },
            True,
            set(),
        ),
        (
            "pso-cache-lookalike",
            {
                REQUIRED_PROFILE: valid_profile,
                "Saved/DiscGolfTour_PCD3D_SM6-copy.upipelinecache": valid_pso_cache,
            },
            False,
            {"UNKNOWN_FILE"},
        ),
        (
            "other-pipeline-cache-path",
            {
                REQUIRED_PROFILE: valid_profile,
                "Saved/PipelineCaches/DiscGolfTour_PCD3D_SM6.upipelinecache": valid_pso_cache,
            },
            False,
            {"UNKNOWN_FILE"},
        ),
        (
            "valid-config-cache",
            {
                REQUIRED_PROFILE: valid_profile,
                "Saved/Config/Windows/GameUserSettings.ini": valid_settings,
                "Saved/Config/CrashReportClient/"
                "UECC-Windows-0123456789ABCDEF0123456789ABCDEF/"
                "CrashReportClient.ini": b"[CrashReportClient]\r\n",
            },
            True,
            set(),
        ),
        (
            "missing-profile",
            {"Saved/Config/Windows/GameUserSettings.ini": valid_settings},
            False,
            {"MISSING_PRODUCTION_PROFILE"},
        ),
        (
            "development-equipment-save",
            {
                REQUIRED_PROFILE: valid_profile,
                "Saved/SaveGames/DGT_Equipment_Dev_v1.sav": b"GVAS-dev",
            },
            False,
            {"DEVELOPMENT_EQUIPMENT_SAVE", "UNKNOWN_SAVEGAME"},
        ),
        (
            "unknown-savegame",
            {
                REQUIRED_PROFILE: valid_profile,
                "Saved/SaveGames/UnapprovedSlot.sav": b"GVAS-other",
            },
            False,
            {"UNKNOWN_SAVEGAME"},
        ),
        (
            "environment-report",
            {
                REQUIRED_PROFILE: valid_profile,
                "Saved/EnvironmentReports/Hole1.json": b"{}",
            },
            False,
            {"ENVIRONMENT_REPORT"},
        ),
        (
            "trajectory-export",
            {
                REQUIRED_PROFILE: valid_profile,
                "Saved/TrajectoryExports/throw.csv": b"x,y\r\n",
            },
            False,
            {"TRAJECTORY_EXPORT"},
        ),
        (
            "diagnostics",
            {
                REQUIRED_PROFILE: valid_profile,
                "Saved/Diagnostics/frame.json": b"{}",
            },
            False,
            {"DIAGNOSTICS_OUTPUT"},
        ),
        (
            "physics-output",
            {
                REQUIRED_PROFILE: valid_profile,
                "Saved/Physics/flight.csv": b"x,y\r\n",
            },
            False,
            {"PHYSICS_OUTPUT"},
        ),
        (
            "regression-output",
            {
                REQUIRED_PROFILE: valid_profile,
                "Saved/Regression/result.json": b"{}",
            },
            False,
            {"REGRESSION_OUTPUT"},
        ),
        (
            "test-output",
            {
                REQUIRED_PROFILE: valid_profile,
                "Saved/Tests/result.xml": b"<tests />",
            },
            False,
            {"TEST_OUTPUT"},
        ),
        (
            "capture-output",
            {
                REQUIRED_PROFILE: valid_profile,
                "Saved/Captures/frame.png": b"not-a-real-png",
            },
            False,
            {"CAPTURE_OUTPUT"},
        ),
        (
            "unknown-file",
            {REQUIRED_PROFILE: valid_profile, "Saved/Logs/DGTour.log": b"log"},
            False,
            {"UNKNOWN_FILE"},
        ),
        (
            "profile-magic",
            {REQUIRED_PROFILE: b"NOT-A-GVAS-PROFILE"},
            False,
            {"INVALID_PRODUCTION_PROFILE_MAGIC"},
        ),
    ]

    completed = 0
    for index, (name, files, should_pass, expected_codes) in enumerate(cases):
        token = f"candidate3-selftest-{index:02d}"
        with tempfile.TemporaryDirectory(prefix="dg_s19_userdir_") as temporary:
            root = Path(temporary) / token
            root.mkdir()
            _write_fixture(root, files)
            receipt = validate_userdir(root, candidate_id, token)
            did_pass = receipt["state"] == "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST"
            actual_codes = {failure["code"] for failure in receipt["failures"]}
            if did_pass != should_pass:
                raise AssertionError(f"{name}: unexpected pass/fail state")
            if not expected_codes.issubset(actual_codes):
                missing = sorted(expected_codes - actual_codes)
                raise AssertionError(f"{name}: missing failure codes {missing}")

            serialized = json.dumps(receipt, sort_keys=True)
            if str(root) in serialized or str(Path(temporary)) in serialized:
                raise AssertionError(f"{name}: absolute host path leaked into receipt")
            if receipt["hostPathRecorded"] is not False:
                raise AssertionError(f"{name}: hostPathRecorded is not false")
            expected_binding = _binding_sha256(candidate_id, token)
            if receipt["candidateUserDirBindingSha256"] != expected_binding:
                raise AssertionError(f"{name}: candidate/token binding mismatch")
            pso_records = [
                item for item in receipt["observedFiles"]
                if item["relativePath"]
                == "Saved/DiscGolfTour_PCD3D_SM6.upipelinecache"
            ]
            if name == "valid-production-pso-cache" and (
                len(pso_records) != 1
                or pso_records[0]["classification"]
                != "production_pso_pipeline_cache"
            ):
                raise AssertionError(
                    "valid-production-pso-cache: exact classification missing"
                )
            completed += 1

    with tempfile.TemporaryDirectory(prefix="dg_s19_userdir_") as temporary:
        root = Path(temporary) / "candidate3-selftest-token"
        root.mkdir()
        _write_fixture(root, {REQUIRED_PROFILE: valid_profile})
        mismatch = validate_userdir(root, candidate_id, "different-safe-token")
        mismatch_codes = {failure["code"] for failure in mismatch["failures"]}
        if "USERDIR_TOKEN_BASENAME_MISMATCH" not in mismatch_codes:
            raise AssertionError("token-basename-mismatch: failure was not detected")
        completed += 1

    # Explicit runtime-journal mode is candidate-bound and does not widen the
    # historical allowlist. Build a complete synthetic external-capture fixture
    # and exercise both sides of that boundary.
    import run_dg_session19_external_technical_evidence as runner

    with tempfile.TemporaryDirectory(prefix="dg_s19_userdir_runtime_") as temporary:
        base = Path(temporary)
        policy, policy_binding = external_evidence.load_policy()
        runtime_candidate = "S19_WindowsShipping_20990101T000000Z_runtime"
        runtime_token = "11111111-2222-4333-8444-555555555555"
        capture_nonce = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"
        round_id = "12345678-1234-4234-8234-123456789abc"
        archive = base / "archive"
        executable = (
            archive / "DiscGolfTour/Binaries/Win64"
            / policy["shippingExecutableBasename"]
        )
        executable.parent.mkdir(parents=True)
        executable.write_bytes(b"MZ-runtime-fresh-userdir-self-test")
        archive_binding = external_evidence.collect_archive(archive)
        exe_sha = external_evidence.sha256_file(executable)
        user_dir = base / "users" / runtime_token
        user_dir.mkdir(parents=True)
        _write_fixture(user_dir, {REQUIRED_PROFILE: valid_profile})
        started = datetime.now(timezone.utc) - timedelta(milliseconds=10)
        journal = user_dir / RUNTIME_JOURNAL_RELATIVE
        journal.parent.mkdir(parents=True)
        journal_raw = runner._self_test_runtime_journal_bytes(
            candidate_id=runtime_candidate,
            user_token=runtime_token,
            exe_sha256=exe_sha,
            archive_sha256=archive_binding["canonicalManifestSha256"],
            capture_nonce=capture_nonce,
            round_id=round_id,
            started_utc=external_evidence.utc_text(started),
        )
        journal.write_bytes(journal_raw)
        run_dir = base / "runs" / f"{runtime_candidate}_{runtime_token}"
        screenshots = run_dir / policy["screenshots"]["relativeDirectory"]
        screenshots.mkdir(parents=True)
        for name in policy["screenshots"]["requiredFiles"]:
            (screenshots / name).write_bytes(external_evidence._png())
        finished = datetime.now(timezone.utc)
        inventory = runner.inspect_runtime_journal(
            user_dir=user_dir,
            candidate_id=runtime_candidate,
            user_token=runtime_token,
            exe_sha256=exe_sha,
            archive_sha256=archive_binding["canonicalManifestSha256"],
            capture_nonce=capture_nonce,
            started=started,
            finished=finished,
            timestamp_tolerance_seconds=5,
            normal_exit=True,
        )
        launch = {
            "schema": runner.LAUNCH_SCHEMA_V2,
            "schemaVersion": 2,
            "session": 19,
            "recordedUtc": external_evidence.utc_text(started),
            "policySha256": policy_binding["sha256"],
            "candidateId": runtime_candidate,
            "exeRelativePath": executable.relative_to(archive).as_posix(),
            "expectedExeSha256": exe_sha,
            "observedExeSha256": exe_sha,
            "expectedArchiveManifestSha256": archive_binding[
                "canonicalManifestSha256"
            ],
            "observedArchiveManifestSha256": archive_binding[
                "canonicalManifestSha256"
            ],
            "userDirToken": runtime_token,
            "captureNonce": capture_nonce,
            "runtimeCheckpointJournalUserDirRelativePath": RUNTIME_JOURNAL_RELATIVE,
            "sanitizedArguments": policy["launch"]["requiredArguments"]
            + [f"-UserDir=<EXTERNAL_UUID:{runtime_token}>"],
            "pid": 12345,
            "hostPathsRecorded": False,
            "claimBoundary": policy["claimBoundary"],
        }
        launch_path = run_dir / policy["evidence"]["launchRecordName"]
        runner.write_json_exclusive(launch_path, launch)
        manifest_path = runner.build_manifest(
            run_dir=run_dir,
            candidate_id=runtime_candidate,
            archive_before=archive_binding,
            archive_after=archive_binding,
            exe_relative=executable.relative_to(archive).as_posix(),
            expected_exe_sha256=exe_sha,
            observed_exe_sha256=exe_sha,
            expected_archive_sha256=archive_binding["canonicalManifestSha256"],
            user_token=runtime_token,
            pid=12345,
            started=started,
            finished=finished,
            exit_code=0,
            timed_out=False,
            policy=policy,
            policy_binding=policy_binding,
            presentmon_requested=False,
            presentmon_tool_expected_sha256=None,
            presentmon_tool_observed_sha256=None,
            runtime_journal=inventory,
        )

        historical = validate_userdir(user_dir, runtime_candidate, runtime_token)
        if historical["state"] == "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST":
            raise AssertionError("default allowlist accepted runtime journal")
        completed += 1

        explicit = validate_userdir(
            user_dir,
            runtime_candidate,
            runtime_token,
            runtime_journal_manifest=manifest_path,
            runtime_launch_record=launch_path,
            shipping_exe=executable,
            archive_root=archive,
        )
        if explicit["state"] != "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST":
            raise AssertionError(explicit["failures"])
        if explicit.get("runtimeJournalBinding", {}).get("sha256") != (
            hashlib.sha256(journal_raw).hexdigest().upper()
        ):
            raise AssertionError("runtime journal binding missing")
        journal_rows = [
            row for row in explicit["observedFiles"]
            if row["relativePath"] == RUNTIME_JOURNAL_RELATIVE
        ]
        if len(journal_rows) != 1 or journal_rows[0]["classification"] != (
            "runtime_checkpoint_journal_candidate_bound"
        ):
            raise AssertionError("runtime journal classification missing")
        completed += 1

        journal.chmod(stat.S_IWRITE | stat.S_IREAD)
        journal.write_bytes(journal_raw + b"\n")
        mutated = validate_userdir(
            user_dir,
            runtime_candidate,
            runtime_token,
            runtime_journal_manifest=manifest_path,
            runtime_launch_record=launch_path,
            shipping_exe=executable,
            archive_root=archive,
        )
        if mutated["state"] == "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST":
            raise AssertionError("mutated runtime journal accepted")
        completed += 1

        output = base / "fresh-runtime-receipt.json"
        if not _emit_receipt(explicit, output):
            raise AssertionError("exclusive receipt was not created")
        original_output = output.read_bytes()
        if _emit_receipt(explicit, output) or output.read_bytes() != original_output:
            raise AssertionError("existing receipt was overwritten")
        completed += 1

    return {
        "schema": f"{SCHEMA}.SelfTest.v1",
        "state": "PASS",
        "testsPassed": completed,
    }


def _input_failure(code: str) -> dict[str, Any]:
    return {
        "schema": SCHEMA,
        "schemaVersion": 1,
        "session": SESSION,
        "candidateId": None,
        "userDirToken": None,
        "hostPathRecorded": False,
        "state": "FAIL_INPUT",
        "failures": [{"code": code}],
        "releaseBoundary": {
            "technicalUserDirAcceptanceOnly": True,
            "gameplayAcceptanceClaimed": False,
            "releaseReadinessClaimed": False,
        },
    }


def _emit_receipt(receipt: dict[str, Any], output: Path | None) -> bool:
    encoded = (
        json.dumps(receipt, indent=2, sort_keys=True, ensure_ascii=True) + "\n"
    ).encode("utf-8")
    if output is None:
        sys.stdout.buffer.write(encoded)
        return True
    try:
        with output.open("xb") as stream:
            stream.write(encoded)
    except OSError:
        return False
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate a fresh external Session 19 Shipping UserDir."
    )
    parser.add_argument("--candidate-id")
    parser.add_argument("--user-dir")
    parser.add_argument("--userdir-token")
    parser.add_argument("--runtime-journal-manifest")
    parser.add_argument("--launch-record")
    parser.add_argument("--shipping-exe")
    parser.add_argument("--archive-root")
    parser.add_argument("--output")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)

    if args.self_test:
        try:
            result = _self_test()
        except Exception as error:  # Self-test diagnostics never include a host path.
            result = {
                "schema": f"{SCHEMA}.SelfTest.v1",
                "state": "FAIL",
                "errorType": type(error).__name__,
            }
            _emit_receipt(result, Path(args.output) if args.output else None)
            return 1
        return 0 if _emit_receipt(
            result, Path(args.output) if args.output else None
        ) else 1

    if not args.candidate_id or not CANDIDATE_ID_RE.fullmatch(args.candidate_id):
        _emit_receipt(_input_failure("INVALID_CANDIDATE_ID"), None)
        return 1
    if not args.userdir_token or not USERDIR_TOKEN_RE.fullmatch(args.userdir_token):
        _emit_receipt(_input_failure("INVALID_USERDIR_TOKEN"), None)
        return 1
    if not args.user_dir:
        _emit_receipt(_input_failure("MISSING_USERDIR_ARGUMENT"), None)
        return 1

    runtime_values = (
        args.runtime_journal_manifest,
        args.launch_record,
        args.shipping_exe,
        args.archive_root,
    )
    if any(runtime_values) and not all(runtime_values):
        _emit_receipt(_input_failure("RUNTIME_JOURNAL_ARGUMENTS_INCOMPLETE"), None)
        return 1

    try:
        receipt = validate_userdir(
            Path(args.user_dir),
            args.candidate_id,
            args.userdir_token,
            runtime_journal_manifest=(
                Path(args.runtime_journal_manifest)
                if args.runtime_journal_manifest else None
            ),
            runtime_launch_record=(
                Path(args.launch_record) if args.launch_record else None
            ),
            shipping_exe=Path(args.shipping_exe) if args.shipping_exe else None,
            archive_root=Path(args.archive_root) if args.archive_root else None,
        )
    except Exception:
        # Never surface an exception string: filesystem exceptions commonly embed
        # the absolute host path that this receipt is designed not to record.
        receipt = _input_failure("UNEXPECTED_VALIDATION_FAILURE")
    output = Path(args.output) if args.output else None
    if not _emit_receipt(receipt, output):
        _emit_receipt(_input_failure("OUTPUT_CREATE_EXCLUSIVE_FAILED"), None)
        return 1
    return 0 if receipt["state"] == "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST" else 1


if __name__ == "__main__":
    sys.exit(main())
