#!/usr/bin/env python3
"""Validate one explicitly selected Win64 Shipping executable.

This gate is intentionally narrower than a package or release gate. With no
arguments it reports a safe pending state and writes nothing. Evidence is only
written when both --exe and --output are explicitly supplied.
"""

from __future__ import annotations

import argparse
from contextlib import redirect_stdout
import datetime as dt
import hashlib
from io import StringIO
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "Config" / "DG_Session19ShippingBinaryPolicy.json"
POLICY_SHA256 = "8ae17ed2bb100c99828aed8a2e459b55fdf0cf2840d05894d72349c79483cef2"

POLICY_SCHEMA = "DiscGolfTour.Session19ShippingBinaryPolicy.v1"
EVIDENCE_SCHEMA = "DiscGolfTour.Session19ShippingBinaryEvidence.v1"
PASS_STATE = "PASS_BINARY_MARKER_POLICY_ONLY"
FAIL_STATE = "FAIL_BINARY_MARKER_POLICY"
PENDING_STATE = "PENDING_EXPLICIT_EXECUTABLE"

IMAGE_FILE_EXECUTABLE_IMAGE = 0x0002
IMAGE_FILE_DLL = 0x2000
IMAGE_FILE_MACHINE_AMD64 = 0x8664
PE32_PLUS_MAGIC = 0x020B


class PolicyError(RuntimeError):
    pass


class InputValidationError(RuntimeError):
    pass


def _utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat().replace("+00:00", "Z")


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _reject_duplicate_keys(pairs: Sequence[Tuple[str, Any]]) -> Dict[str, Any]:
    result: Dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise PolicyError("policy JSON contains a duplicate object key")
        result[key] = value
    return result


def _load_policy_at(path: Path) -> Tuple[Dict[str, Any], str]:
    try:
        raw = path.read_bytes()
    except OSError as exc:
        raise PolicyError(
            "policy could not be read ({}, errno={})".format(
                exc.__class__.__name__, getattr(exc, "errno", None)
            )
        ) from None

    digest = _sha256_bytes(raw)
    if digest != POLICY_SHA256:
        raise PolicyError("policy SHA-256 does not match the validator-bound authority")
    try:
        policy = json.loads(raw.decode("utf-8"), object_pairs_hook=_reject_duplicate_keys)
    except (UnicodeDecodeError, json.JSONDecodeError, PolicyError) as exc:
        if isinstance(exc, PolicyError):
            raise
        raise PolicyError("policy is not strict UTF-8 JSON") from None
    _validate_policy_shape(policy)
    return policy, digest


def _require_exact_keys(value: Any, keys: Iterable[str], label: str) -> None:
    if type(value) is not dict:
        raise PolicyError(label + " must be an object")
    expected = set(keys)
    actual = set(value)
    if actual != expected:
        raise PolicyError(label + " keys differ from the bound schema")


def _validate_policy_shape(policy: Dict[str, Any]) -> None:
    _require_exact_keys(
        policy,
        {
            "schema",
            "schemaVersion",
            "session",
            "policyId",
            "target",
            "inputContract",
            "scanContract",
            "requiredMarkers",
            "allowedPublicGameplayMarkers",
            "forbiddenMarkerGroups",
            "evidenceContract",
        },
        "policy",
    )
    if policy["schema"] != POLICY_SCHEMA or type(policy["schemaVersion"]) is not int:
        raise PolicyError("policy schema is unsupported")
    if policy["schemaVersion"] != 1 or policy["session"] != 19:
        raise PolicyError("policy version/session is unsupported")
    if policy["policyId"] != "windows_shipping_v05_executable_marker_policy_v1":
        raise PolicyError("policyId is unsupported")

    if policy["target"] != {
        "platform": "Win64",
        "configuration": "Shipping",
        "format": "PE32+",
        "machine": "AMD64",
        "milestone": "v0.5",
    }:
        raise PolicyError("target contract differs from Win64 Shipping v0.5")

    input_contract = policy["inputContract"]
    _require_exact_keys(
        input_contract,
        {
            "explicitExecutableArgumentRequiredForValidation",
            "minimumBytes",
            "maximumBytes",
            "requireExecutableImage",
            "rejectDllImage",
        },
        "inputContract",
    )
    if input_contract["explicitExecutableArgumentRequiredForValidation"] is not True:
        raise PolicyError("explicit executable input must remain required")
    if input_contract["requireExecutableImage"] is not True:
        raise PolicyError("PE executable-image validation must remain required")
    if input_contract["rejectDllImage"] is not True:
        raise PolicyError("DLL rejection must remain required")
    if type(input_contract["minimumBytes"]) is not int or type(
        input_contract["maximumBytes"]
    ) is not int:
        raise PolicyError("input byte bounds must be integers")
    if not 4096 <= input_contract["minimumBytes"] < input_contract["maximumBytes"]:
        raise PolicyError("input byte bounds are invalid")
    if input_contract["maximumBytes"] > 2147483648:
        raise PolicyError("input maximum exceeds the bounded scanner contract")

    if policy["scanContract"] != {
        "caseInsensitive": True,
        "encodings": ["ascii", "utf-16le"],
        "matchMode": "literal-substring",
    }:
        raise PolicyError("scan contract must remain case-insensitive ASCII and UTF-16LE")

    required = policy["requiredMarkers"]
    if required != [
        {
            "id": "project_owned_runtime_foundation",
            "value": "DiscGolfRuntimeFoundation",
            "requireAnyEncoding": True,
        },
        {
            "id": "release_performance_capture_entrypoint",
            "value": "PerformanceCaptureSeconds=",
            "requireAnyEncoding": True,
        },
        {
            "id": "release_performance_capture_fail_closed_rejection",
            "value": "RELEASE PERFORMANCE CAPTURE REJECTED:",
            "requireAnyEncoding": True,
        },
        {
            "id": "release_performance_capture_exclusive_namespace",
            "value": "ReleasePerformanceCapture.guard",
            "requireAnyEncoding": True,
        },
        {
            "id": "release_performance_capture_external_uuid_guard",
            "value": "release performance capture requires a fresh external UUID UserDir",
            "requireAnyEncoding": True,
        },
        {
            "id": "release_performance_capture_exact_render_guard",
            "value": "release performance capture requires exact Pine Ridge hole, duration, resolution, offscreen, and D3D12 flags",
            "requireAnyEncoding": True,
        },
        {
            "id": "release_performance_capture_public_snapshot_rejection",
            "value": "Shipping public performance snapshot rejected.",
            "requireAnyEncoding": True,
        },
        {
            "id": "release_performance_capture_public_reset_rejection",
            "value": "Shipping public performance telemetry reset rejected.",
            "requireAnyEncoding": True,
        },
    ]:
        raise PolicyError("required runtime and guarded performance-capture markers differ")

    if policy["allowedPublicGameplayMarkers"] != [
        "DGT_Scorecard",
        "DGT_Replay",
        "DGT_ReplayPause",
        "DGT_ReplaySeek",
        "DGT_ReplayRate",
        "DGT_NextHole",
        "DGT_RestartRound",
        "DGT_PreviewFlyover",
        "DGT_ToggleDiscFavorite",
    ]:
        raise PolicyError("allowed public gameplay markers differ from the bound authority")

    groups = policy["forbiddenMarkerGroups"]
    if type(groups) is not list or not groups:
        raise PolicyError("forbiddenMarkerGroups must be a non-empty array")
    group_ids = set()
    marker_values = set()
    for index, group in enumerate(groups):
        _require_exact_keys(group, {"id", "markers"}, "forbiddenMarkerGroups[]")
        if type(group["id"]) is not str or not group["id"] or group["id"] in group_ids:
            raise PolicyError("forbidden group ids must be unique non-empty strings")
        group_ids.add(group["id"])
        if type(group["markers"]) is not list or not group["markers"]:
            raise PolicyError("each forbidden marker group must be non-empty")
        for marker in group["markers"]:
            if type(marker) is not str or not marker or not marker.isascii():
                raise PolicyError("marker values must be non-empty ASCII strings")
            folded = marker.casefold()
            if folded in marker_values:
                raise PolicyError("forbidden marker values must be unique ignoring case")
            marker_values.add(folded)
    expected_groups = {
        "legacy_character_framework",
        "throw_lab",
        "career_ai_runtime_runner_save",
        "developer_editor_modules",
        "session_runner_capture_cook_packaged_classes",
        "session_validation_commands_and_saves",
        "developer_console_and_commandline_entrypoints",
        "developer_input_actions",
        "character_prototype_proxy_assets",
        "regression_and_cook_manifest_json",
        "never_cook_and_quarantined_paths",
    }
    if group_ids != expected_groups:
        raise PolicyError("forbidden marker group ids differ from the bound authority")
    if len(marker_values) != 124:
        raise PolicyError("forbidden marker count differs from the bound authority")

    evidence = policy["evidenceContract"]
    _require_exact_keys(
        evidence,
        {
            "writeOnlyWithExplicitOutputArgument",
            "recordHostPaths",
            "passState",
            "failState",
            "releaseReady",
            "resolvedBlockers",
            "disclaimer",
        },
        "evidenceContract",
    )
    if evidence["writeOnlyWithExplicitOutputArgument"] is not True:
        raise PolicyError("implicit evidence writes are forbidden")
    if evidence["recordHostPaths"] is not False:
        raise PolicyError("host paths must not be recorded")
    if evidence["passState"] != PASS_STATE or evidence["failState"] != FAIL_STATE:
        raise PolicyError("evidence states differ from the bounded policy")
    if evidence["releaseReady"] is not False or evidence["resolvedBlockers"] != []:
        raise PolicyError("binary evidence cannot claim readiness or blocker closure")
    if type(evidence["disclaimer"]) is not str or "does not" not in evidence[
        "disclaimer"
    ]:
        raise PolicyError("bounded evidence disclaimer is missing")


def _iter_markers(policy: Dict[str, Any]) -> List[Dict[str, str]]:
    markers: List[Dict[str, str]] = []
    for item in policy["requiredMarkers"]:
        markers.append(
            {"kind": "required", "group": "required", "id": item["id"], "value": item["value"]}
        )
    for group in policy["forbiddenMarkerGroups"]:
        for index, value in enumerate(group["markers"]):
            markers.append(
                {
                    "kind": "forbidden",
                    "group": group["id"],
                    "id": "{}_{:02d}".format(group["id"], index + 1),
                    "value": value,
                }
            )
    return markers


def _scan_and_hash(
    path: Path, policy: Dict[str, Any], chunk_bytes: int = 1024 * 1024
) -> Tuple[int, str, Dict[str, Dict[str, bool]]]:
    if chunk_bytes < 8:
        raise ValueError("chunk_bytes is too small")
    markers = _iter_markers(policy)
    encoded: List[Tuple[str, str, bytes]] = []
    max_needle = 1
    for marker in markers:
        ascii_needle = marker["value"].lower().encode("ascii")
        utf16_needle = marker["value"].lower().encode("utf-16le")
        encoded.append((marker["id"], "ascii", ascii_needle))
        encoded.append((marker["id"], "utf-16le", utf16_needle))
        max_needle = max(max_needle, len(ascii_needle), len(utf16_needle))

    found: Dict[str, Dict[str, bool]] = {
        marker["id"]: {"ascii": False, "utf-16le": False} for marker in markers
    }
    hasher = hashlib.sha256()
    total = 0
    tail = b""
    try:
        with path.open("rb") as handle:
            while True:
                chunk = handle.read(chunk_bytes)
                if not chunk:
                    break
                total += len(chunk)
                hasher.update(chunk)
                window = tail + chunk
                folded = window.lower()
                for marker_id, encoding, needle in encoded:
                    if not found[marker_id][encoding] and needle in folded:
                        found[marker_id][encoding] = True
                tail = window[-(max_needle - 1) :] if max_needle > 1 else b""
    except OSError as exc:
        raise InputValidationError(
            "input could not be read ({}, errno={})".format(
                exc.__class__.__name__, getattr(exc, "errno", None)
            )
        ) from None
    return total, hasher.hexdigest(), found


def _inspect_pe(path: Path, size: int) -> Dict[str, Any]:
    try:
        with path.open("rb") as handle:
            dos = handle.read(64)
            if len(dos) != 64 or dos[:2] != b"MZ":
                raise InputValidationError("input is not an MZ executable")
            pe_offset = struct.unpack_from("<I", dos, 0x3C)[0]
            if pe_offset < 64 or pe_offset > size - 24:
                raise InputValidationError("PE header offset is outside the input")
            handle.seek(pe_offset)
            signature = handle.read(4)
            coff = handle.read(20)
            if signature != b"PE\0\0" or len(coff) != 20:
                raise InputValidationError("input does not contain a complete PE signature/header")
            (
                machine,
                section_count,
                _timestamp,
                _symbol_table,
                _symbol_count,
                optional_size,
                characteristics,
            ) = struct.unpack("<HHIIIHH", coff)
            optional_offset = pe_offset + 24
            section_table_end = optional_offset + optional_size + (section_count * 40)
            if section_count < 1 or section_count > 96:
                raise InputValidationError("PE section count is outside the bounded contract")
            if optional_size < 2 or section_table_end > size:
                raise InputValidationError("PE optional/section headers are truncated")
            optional_magic_raw = handle.read(2)
            if len(optional_magic_raw) != 2:
                raise InputValidationError("PE optional header is truncated")
            optional_magic = struct.unpack("<H", optional_magic_raw)[0]
    except InputValidationError:
        raise
    except OSError as exc:
        raise InputValidationError(
            "input PE header could not be read ({}, errno={})".format(
                exc.__class__.__name__, getattr(exc, "errno", None)
            )
        ) from None

    if machine != IMAGE_FILE_MACHINE_AMD64:
        raise InputValidationError("PE machine is not AMD64")
    if optional_magic != PE32_PLUS_MAGIC:
        raise InputValidationError("PE optional header is not PE32+")
    if not characteristics & IMAGE_FILE_EXECUTABLE_IMAGE:
        raise InputValidationError("PE is not marked as an executable image")
    if characteristics & IMAGE_FILE_DLL:
        raise InputValidationError("PE is marked as a DLL")
    return {
        "format": "PE32+",
        "machine": "AMD64",
        "sectionCount": section_count,
        "executableImage": True,
        "dllImage": False,
    }


def _base_evidence(policy: Dict[str, Any], policy_sha256: str, file_name: str) -> Dict[str, Any]:
    return {
        "schema": EVIDENCE_SCHEMA,
        "schemaVersion": 1,
        "session": 19,
        "policyId": policy["policyId"],
        "policySha256": policy_sha256,
        "checkedUtc": _utc_now(),
        "state": FAIL_STATE,
        "passed": False,
        "input": {
            "fileName": file_name,
            "hostPathRecorded": False,
            "bytes": 0,
            "sha256": "",
            "pe": None,
        },
        "scan": {
            "caseInsensitive": True,
            "encodings": ["ascii", "utf-16le"],
            "required": [],
            "missingRequiredMarkerIds": [],
            "forbiddenMatches": [],
            "forbiddenMatchCount": 0,
        },
        "errors": [],
        "resolvedBlockers": [],
        "releaseReady": False,
        "disclaimer": policy["evidenceContract"]["disclaimer"],
    }


def validate_executable(
    executable: Path,
    policy: Dict[str, Any],
    policy_sha256: str,
    chunk_bytes: int = 1024 * 1024,
) -> Dict[str, Any]:
    result = _base_evidence(policy, policy_sha256, executable.name)
    errors: List[str] = result["errors"]
    try:
        before = executable.stat()
    except OSError as exc:
        errors.append(
            "input executable is missing or inaccessible ({}, errno={})".format(
                exc.__class__.__name__, getattr(exc, "errno", None)
            )
        )
        return result
    if not executable.is_file():
        errors.append("input executable is not a regular file")
        return result

    result["input"]["bytes"] = before.st_size
    minimum = policy["inputContract"]["minimumBytes"]
    maximum = policy["inputContract"]["maximumBytes"]
    if before.st_size < minimum or before.st_size > maximum:
        errors.append("input byte size is outside the bounded policy")
        return result

    try:
        scanned_bytes, digest, found = _scan_and_hash(executable, policy, chunk_bytes)
    except InputValidationError as exc:
        errors.append(str(exc))
        return result
    result["input"]["bytes"] = scanned_bytes
    result["input"]["sha256"] = digest
    try:
        after = executable.stat()
    except OSError as exc:
        errors.append(
            "input disappeared after scanning ({}, errno={})".format(
                exc.__class__.__name__, getattr(exc, "errno", None)
            )
        )
        return result
    if (
        scanned_bytes != before.st_size
        or after.st_size != before.st_size
        or after.st_mtime_ns != before.st_mtime_ns
    ):
        errors.append("input changed while it was being scanned")
        return result

    try:
        result["input"]["pe"] = _inspect_pe(executable, scanned_bytes)
    except InputValidationError as exc:
        errors.append(str(exc))

    marker_by_id = {marker["id"]: marker for marker in _iter_markers(policy)}
    for required in policy["requiredMarkers"]:
        status = found[required["id"]]
        required_result = {
            "id": required["id"],
            "foundAscii": status["ascii"],
            "foundUtf16Le": status["utf-16le"],
        }
        result["scan"]["required"].append(required_result)
        if not (status["ascii"] or status["utf-16le"]):
            result["scan"]["missingRequiredMarkerIds"].append(required["id"])

    for marker_id, status in found.items():
        marker = marker_by_id[marker_id]
        if marker["kind"] != "forbidden" or not (status["ascii"] or status["utf-16le"]):
            continue
        result["scan"]["forbiddenMatches"].append(
            {
                "group": marker["group"],
                "marker": marker["value"],
                "foundAscii": status["ascii"],
                "foundUtf16Le": status["utf-16le"],
            }
        )
    result["scan"]["forbiddenMatches"].sort(
        key=lambda item: (item["group"], item["marker"].casefold())
    )
    result["scan"]["forbiddenMatchCount"] = len(result["scan"]["forbiddenMatches"])

    if result["scan"]["missingRequiredMarkerIds"]:
        errors.append("one or more required Shipping markers are missing")
    if result["scan"]["forbiddenMatchCount"]:
        errors.append("one or more forbidden Shipping markers are present")

    result["passed"] = not errors
    result["state"] = PASS_STATE if result["passed"] else FAIL_STATE
    return result


def _write_evidence(output: Path, result: Dict[str, Any]) -> None:
    payload = (json.dumps(result, indent=2, sort_keys=True) + "\n").encode("utf-8")
    try:
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("xb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError:
        raise InputValidationError(
            "evidence output already exists; refusing overwrite"
        ) from None
    except OSError as exc:
        raise InputValidationError(
            "evidence output failed ({}, errno={})".format(
                exc.__class__.__name__, getattr(exc, "errno", None)
            )
        ) from None


def _minimal_pe(payload: bytes, machine: int = IMAGE_FILE_MACHINE_AMD64) -> bytes:
    pe_offset = 0x80
    optional_size = 0xF0
    dos = bytearray(pe_offset)
    dos[:2] = b"MZ"
    struct.pack_into("<I", dos, 0x3C, pe_offset)
    coff = struct.pack(
        "<HHIIIHH",
        machine,
        1,
        0,
        0,
        0,
        optional_size,
        IMAGE_FILE_EXECUTABLE_IMAGE,
    )
    optional = bytearray(optional_size)
    struct.pack_into("<H", optional, 0, PE32_PLUS_MAGIC)
    header = bytes(dos) + b"PE\0\0" + coff + bytes(optional) + bytes(40)
    padding = bytes(max(0, 4096 - len(header) - len(payload)))
    return header + padding + payload


def _mixed_case(value: str) -> str:
    output = []
    upper = True
    for character in value:
        if character.isalpha():
            output.append(character.upper() if upper else character.lower())
            upper = not upper
        else:
            output.append(character)
    return "".join(output)


def _run_self_test() -> int:
    checks = 0

    def require(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError(message)

    policy, policy_sha = _load_policy_at(POLICY_PATH)
    required_payload = b"\0".join(
        item["value"].encode("ascii") for item in policy["requiredMarkers"]
    )
    with tempfile.TemporaryDirectory(prefix="dg-s19-binary-selftest-") as directory:
        root = Path(directory)

        ascii_valid = root / "ascii-valid.exe"
        ascii_valid.write_bytes(_minimal_pe(b"x" + required_payload + b"y"))
        ascii_result = validate_executable(ascii_valid, policy, policy_sha, chunk_bytes=17)
        require(ascii_result["passed"], "mixed-case ASCII required marker should pass")
        require(
            ascii_result["input"]["sha256"] == _sha256_bytes(ascii_valid.read_bytes()),
            "evidence hash must bind the exact input",
        )
        require(
            ascii_result["input"]["bytes"] == ascii_valid.stat().st_size,
            "evidence size must bind the exact input",
        )

        utf16_valid = root / "utf16-valid.exe"
        utf16_valid.write_bytes(_minimal_pe(b"\0\0".join(
            _mixed_case(item["value"]).encode("utf-16le")
            for item in policy["requiredMarkers"]
        )))
        utf16_result = validate_executable(utf16_valid, policy, policy_sha, chunk_bytes=19)
        require(utf16_result["passed"], "mixed-case UTF-16LE required marker should pass")
        require(
            all(item["foundUtf16Le"] for item in utf16_result["scan"]["required"]),
            "every UTF-16LE required marker must be reported",
        )

        allowed_public = root / "allowed-public-gameplay.exe"
        allowed_payload = "\0".join(policy["allowedPublicGameplayMarkers"]).encode("ascii")
        allowed_public.write_bytes(
            _minimal_pe(required_payload + b"\0" + allowed_payload)
        )
        require(
            validate_executable(allowed_public, policy, policy_sha)["passed"],
            "public scorecard/replay/next/restart/flyover/favorite markers must remain allowed",
        )

        missing_required = root / "missing-required.exe"
        missing_required.write_bytes(_minimal_pe(b"ordinary shipping payload"))
        missing_result = validate_executable(missing_required, policy, policy_sha)
        require(not missing_result["passed"], "missing required marker must fail")

        forbidden_markers = [
            marker for marker in _iter_markers(policy) if marker["kind"] == "forbidden"
        ]
        require(len(forbidden_markers) == 124, "self-test must cover every forbidden marker")
        require(
            "PerformanceCaptureSeconds=" in {
                item["value"] for item in policy["requiredMarkers"]
            },
            "release performance entrypoint must be an explicitly required marker",
        )
        require(
            "performancecaptureseconds=" not in {
                marker["value"].casefold() for marker in forbidden_markers
            },
            "guarded release performance entrypoint cannot remain globally forbidden",
        )
        for index, marker in enumerate(forbidden_markers):
            for encoding in ("ascii", "utf-16le"):
                fixture = root / "forbidden-{:03d}-{}.exe".format(index, encoding)
                value = _mixed_case(marker["value"])
                encoded = value.encode("ascii" if encoding == "ascii" else "utf-16le")
                fixture.write_bytes(
                    _minimal_pe(required_payload + b"\0" + encoded)
                )
                result = validate_executable(fixture, policy, policy_sha, chunk_bytes=23)
                require(not result["passed"], "forbidden marker must fail in " + encoding)
                matched = {
                    (item["group"], item["marker"].casefold())
                    for item in result["scan"]["forbiddenMatches"]
                }
                require(
                    (marker["group"], marker["value"].casefold()) in matched,
                    "target forbidden marker was not reported in " + encoding,
                )

        non_pe = root / "not-pe.exe"
        non_pe.write_bytes(b"N" * 4096 + required_payload)
        non_pe_result = validate_executable(non_pe, policy, policy_sha)
        require(not non_pe_result["passed"], "non-PE input must fail")
        require(bool(non_pe_result["input"]["sha256"]), "non-PE input must still be hashed")

        malformed = root / "malformed-pe.exe"
        malformed_data = bytearray(4096)
        malformed_data[:2] = b"MZ"
        struct.pack_into("<I", malformed_data, 0x3C, 0x2000)
        malformed.write_bytes(bytes(malformed_data) + required_payload)
        require(
            not validate_executable(malformed, policy, policy_sha)["passed"],
            "malformed PE input must fail",
        )

        wrong_machine = root / "wrong-machine.exe"
        wrong_machine.write_bytes(_minimal_pe(required_payload, machine=0x014C))
        require(
            not validate_executable(wrong_machine, policy, policy_sha)["passed"],
            "non-AMD64 PE input must fail",
        )

        missing_file = root / "missing.exe"
        require(
            not validate_executable(missing_file, policy, policy_sha)["passed"],
            "missing input must fail closed",
        )

        mutated_policy = root / "mutated-policy.json"
        mutated_policy.write_bytes(POLICY_PATH.read_bytes().replace(b"ThrowLab", b"ThrowLax", 1))
        try:
            _load_policy_at(mutated_policy)
        except PolicyError:
            mutation_rejected = True
        else:
            mutation_rejected = False
        require(mutation_rejected, "policy mutation must fail the bound SHA-256")

        evidence_path = root / "explicit" / "receipt.json"
        _write_evidence(evidence_path, ascii_result)
        evidence_bytes = evidence_path.read_bytes()
        evidence = json.loads(evidence_bytes.decode("utf-8"))
        try:
            _write_evidence(evidence_path, missing_result)
        except InputValidationError as exc:
            existing_output_rejected = "already exists" in str(exc)
        else:
            existing_output_rejected = False
        require(
            existing_output_rejected,
            "an existing explicit evidence output must be rejected",
        )
        require(
            evidence_path.read_bytes() == evidence_bytes,
            "a rejected evidence write must preserve the existing bytes",
        )
        require(evidence["releaseReady"] is False, "evidence must never claim release readiness")
        require(evidence["resolvedBlockers"] == [], "evidence must never close blockers")
        require(
            str(root).casefold().encode("utf-8") not in evidence_bytes.lower(),
            "evidence must not contain a host path",
        )
        require(
            ascii_result["input"]["fileName"] == "ascii-valid.exe",
            "evidence may retain only the input basename",
        )

        before_no_argument = {path.relative_to(root) for path in root.rglob("*")}
        pending_stdout = StringIO()
        with redirect_stdout(pending_stdout):
            pending_exit = main([])
        pending = json.loads(pending_stdout.getvalue())
        after_no_argument = {path.relative_to(root) for path in root.rglob("*")}
        require(pending_exit == 0, "no-argument pending mode must be non-blocking")
        require(
            pending["state"] == PENDING_STATE and pending["passed"] is False,
            "no-argument mode must be pending, never passing",
        )
        require(
            before_no_argument == after_no_argument,
            "no-argument pending mode must not write files",
        )

        before_read_only = {path.relative_to(root) for path in root.rglob("*")}
        read_only_stdout = StringIO()
        with redirect_stdout(read_only_stdout):
            read_only_exit = main(["--exe", str(ascii_valid)])
        after_read_only = {path.relative_to(root) for path in root.rglob("*")}
        require(read_only_exit == 0, "read-only explicit validation fixture should pass")
        require(
            before_read_only == after_read_only,
            "--exe without --output must not write evidence",
        )
        require(
            str(root).casefold() not in read_only_stdout.getvalue().casefold(),
            "read-only summary must not echo a host path",
        )

    print(
        "Session 19 Shipping binary validator self-test OK ({} assertions; {} markers x ASCII/UTF-16LE).".format(
            checks, len(forbidden_markers)
        )
    )
    return 0


def _parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate one explicit Win64 Shipping PE against the Session 19 marker policy."
    )
    parser.add_argument("--exe", type=Path, help="Exact executable to hash and validate.")
    parser.add_argument(
        "--output",
        type=Path,
        help="Explicit evidence JSON destination. Omit to perform a read-only validation.",
    )
    parser.add_argument("--self-test", action="store_true", help="Run synthetic adversarial fixtures.")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = _parse_args(argv)
    if args.self_test:
        if args.exe is not None or args.output is not None:
            print("Self-test does not accept --exe or --output.", file=sys.stderr)
            return 2
        try:
            return _run_self_test()
        except (AssertionError, PolicyError, OSError) as exc:
            print(
                "Session 19 Shipping binary validator self-test FAILED: {}".format(
                    exc.__class__.__name__
                ),
                file=sys.stderr,
            )
            return 1

    if args.exe is None:
        if args.output is not None:
            print("--output requires an explicit --exe input.", file=sys.stderr)
            return 2
        print(
            json.dumps(
                {
                    "state": PENDING_STATE,
                    "passed": False,
                    "evidenceWritten": False,
                    "releaseReady": False,
                    "message": "Pass --exe to evaluate one exact executable; add --output only for explicit evidence.",
                },
                sort_keys=True,
            )
        )
        return 0

    try:
        policy, policy_sha = _load_policy_at(POLICY_PATH)
    except PolicyError as exc:
        print("Shipping binary policy failed closed: {}".format(str(exc)), file=sys.stderr)
        return 2

    result = validate_executable(args.exe, policy, policy_sha)
    if args.output is not None:
        try:
            _write_evidence(args.output, result)
        except InputValidationError as exc:
            print(str(exc), file=sys.stderr)
            return 2

    summary = {
        "state": result["state"],
        "passed": result["passed"],
        "bytes": result["input"]["bytes"],
        "sha256": result["input"]["sha256"],
        "missingRequiredMarkerIds": result["scan"]["missingRequiredMarkerIds"],
        "forbiddenMatchCount": result["scan"]["forbiddenMatchCount"],
        "forbiddenMarkers": [
            item["marker"] for item in result["scan"]["forbiddenMatches"]
        ],
        "evidenceWritten": args.output is not None,
        "releaseReady": False,
    }
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
