#!/usr/bin/env python3
"""Candidate-bound v0.5 equipment Shipping technical evidence gate.

This gate intentionally does not decide naming clearance, manual play feel,
legal/provenance approval, blocker closure, or release readiness. Absolute host
paths are consumed read-only and are never serialized into the receipt.
"""

from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys
import tempfile
from typing import Any, Callable, Iterable


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_POLICY = ROOT / "Config/DG_Session19V05EquipmentShippingTechnicalPolicy.json"
SCHEMA = "DiscGolfTour.Session19V05EquipmentShippingTechnicalReceipt.v1"
POLICY_SCHEMA = "DiscGolfTour.Session19V05EquipmentShippingTechnicalPolicy.v1"
SESSION = 19
INNER_EXE = "DiscGolfTour/Binaries/Win64/DiscGolfTour-Win64-Shipping.exe"
CANDIDATE_RE = re.compile(
    r"^S19_WindowsShipping_(?P<build_utc>\d{8}T\d{6}Z)_"
    r"(?P<build_id>[0-9a-f]{12})$"
)
SHA256_RE = re.compile(r"^[0-9A-F]{64}$")
PAK_ENTRY_RE = re.compile(r'^LogPakFile: Display: "(?P<path>[^"]+)" offset:')
PAK_SUMMARY_RE = re.compile(r"^LogPakFile: Display: (\d+) files \(")
COMPLETION_RE = re.compile(
    r"Test Completed\. Result=\{(?P<result>[^}]+)\} .*? Path=\{(?P<path>[^}]+)\}"
)
EXPECTED_VECTOR_SHA256 = "7201CC14DC8E81773850B1576194B20B0A62FD6E1ECA2D81CB1823709FFED60B"
EXPECTED_MOLDS = [
    {"id": "Apex", "speed": 12, "glide": 5, "turn": -1.0, "fade": 3.0},
    {"id": "Vector", "speed": 9, "glide": 5, "turn": -2.0, "fade": 2.0},
    {"id": "Line", "speed": 7, "glide": 5, "turn": -1.0, "fade": 2.0},
    {"id": "Compass", "speed": 5, "glide": 5, "turn": 0.0, "fade": 1.0},
    {"id": "Touch", "speed": 2, "glide": 3, "turn": 0.0, "fade": 1.0},
]
EXPECTED_PLASTICS = [
    {"id": "Base", "turnScale": 1.18, "fadeScale": 0.88, "restitutionScale": 0.72, "frictionScale": 1.18},
    {"id": "Tour", "turnScale": 1.0, "fadeScale": 1.0, "restitutionScale": 1.0, "frictionScale": 1.0},
    {"id": "Crystal", "turnScale": 0.84, "fadeScale": 1.12, "restitutionScale": 1.2, "frictionScale": 0.86},
]
EXPECTED_ASSETS = [
    "DiscGolfTour/Content/Data/Discs/Molds/DA_Mold_Apex.uasset",
    "DiscGolfTour/Content/Data/Discs/Molds/DA_Mold_Compass.uasset",
    "DiscGolfTour/Content/Data/Discs/Molds/DA_Mold_Line.uasset",
    "DiscGolfTour/Content/Data/Discs/Molds/DA_Mold_Touch.uasset",
    "DiscGolfTour/Content/Data/Discs/Molds/DA_Mold_Vector.uasset",
    "DiscGolfTour/Content/Data/Discs/Plastics/DA_Plastic_Base.uasset",
    "DiscGolfTour/Content/Data/Discs/Plastics/DA_Plastic_Crystal.uasset",
    "DiscGolfTour/Content/Data/Discs/Plastics/DA_Plastic_Tour.uasset",
]
EXPECTED_GUID_WORDS = [
    [0xD6110001, 0x53455353, 0x494F4E31, 0x31000001],
    [0xD6110002, 0x53455353, 0x494F4E31, 0x31000002],
    [0xD6110003, 0x53455353, 0x494F4E31, 0x31000003],
    [0xD6110004, 0x53455353, 0x494F4E31, 0x31000004],
    [0xD6110005, 0x53455353, 0x494F4E31, 0x31000005],
]
EXPECTED_SOURCE_PATHS = [
    "Source/DiscGolfTour/DiscGolfTypes.h",
    "Source/DiscGolfTour/DiscCatalogSubsystem.cpp",
    "Source/DiscGolfTour/DiscBagComponent.cpp",
    "Source/DiscGolfTour/DiscGolfSaveGame.h",
    "Source/DiscGolfTour/DiscGolfTourGameInstance.cpp",
    "Source/DiscGolfTour/Tests/DiscEquipmentSession11Tests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfSaveSchemaTests.cpp",
    "Source/DiscGolfTour/Tests/DiscGolfMathTests.cpp",
]
EXPECTED_AUTOMATION_TESTS = [
    "DiscGolfTour.Data.Catalog.PrimaryAssetsRegistered",
    "DiscGolfTour.Data.Catalog.AssetFallbackParity",
    "DiscGolfTour.Data.Catalog.DuplicateIdsRejected",
    "DiscGolfTour.Data.Catalog.MissingIdsRejected",
    "DiscGolfTour.Data.Catalog.InvalidAssetsUseFallback",
    "DiscGolfTour.Data.Catalog.BagEquipmentSelection",
    "DiscGolfTour.Session11.Equipment.DefaultLoadoutIsStableAndGeneric",
    "DiscGolfTour.Session11.Equipment.FavoriteToggleIsInstanceScoped",
    "DiscGolfTour.Session11.Equipment.LegacySelectionIsAtomic",
    "DiscGolfTour.Session11.Equipment.LoadoutValidationRejectsInvalidIdentity",
    "DiscGolfTour.Session11.Equipment.MassResolutionPreservesParityAndWearBoundary",
    "DiscGolfTour.Session11.Equipment.ResolutionRejectsCatalogMismatch",
    "DiscGolfTour.Session11.Equipment.SeparateSchemaV1SaveIsAtomic",
    "DiscGolfTour.Persistence.SaveSchema.DefaultVersion",
    "DiscGolfTour.Persistence.SaveSchema.CurrentVersionAccepted",
    "DiscGolfTour.Persistence.SaveSchema.StaleAndFutureVersionsRejected",
    "DiscGolfTour.Character.Session4.Profile.Schema6Migration",
    "DiscGolfTour.Character.Session4.Profile.FutureSchemaGuard",
    "DiscGolfTour.Character.Session4.Profile.LegacyMigrationRegression",
    "DiscGolfTour.Rules.LieSaveSerialization",
    "DiscGolfTour.Physics.LaunchDirection.PositiveIsUp",
    "DiscGolfTour.Physics.StabilityMoment.NeutralBand",
    "DiscGolfTour.Physics.StabilityMoment.TurnFadeOppose",
]
POST_FIX_REQUIRED_TESTS = ["DiscGolfTour.Session19.Input.HoleIntroThrowTimingLifecycle"]


class DuplicateKeyError(ValueError):
    pass


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateKeyError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json_text(text: str) -> Any:
    return json.loads(
        text,
        object_pairs_hook=reject_duplicate_keys,
        parse_constant=lambda value: (_ for _ in ()).throw(
            ValueError(f"non-finite JSON number: {value}")
        ),
    )


def load_json(path: Path) -> Any:
    return load_json_text(path.read_text(encoding="utf-8-sig"))


def candidate_build_utc(candidate_id: str) -> datetime:
    match = CANDIDATE_RE.fullmatch(candidate_id)
    if match is None:
        raise ValueError("candidate ID must contain its exact UTC build timestamp")
    try:
        return datetime.strptime(
            match.group("build_utc"), "%Y%m%dT%H%M%SZ"
        ).replace(tzinfo=timezone.utc)
    except ValueError as exc:
        raise ValueError("candidate ID contains an invalid UTC build timestamp") from exc


def candidate_build_utc_text(candidate_id: str) -> str:
    return candidate_build_utc(candidate_id).isoformat().replace("+00:00", "Z")


def candidate_log_name_matches(file_name: Any, candidate_id: Any) -> bool:
    if not isinstance(file_name, str) or not isinstance(candidate_id, str):
        return False
    if Path(file_name).name != file_name or Path(file_name).suffix.casefold() != ".log":
        return False
    return re.search(
        rf"(?:^|[_.-]){re.escape(candidate_id)}(?:$|[_.-])",
        Path(file_name).stem,
    ) is not None


def read_stable_regular_file(
    path: Path,
    *,
    _after_read_hook: Callable[[Path], None] | None = None,
) -> tuple[bytes, os.stat_result]:
    """Return one byte snapshot whose open handle and path identity stayed stable."""
    path_lstat = path.lstat()
    reparse_mask = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    if stat.S_ISLNK(path_lstat.st_mode) or (
        reparse_mask
        and getattr(path_lstat, "st_file_attributes", 0) & reparse_mask
    ):
        raise ValueError(f"input is a link or reparse point: {path}")
    if not stat.S_ISREG(path_lstat.st_mode):
        raise ValueError(f"input is not a regular file: {path}")

    with path.open("rb") as stream:
        before = os.fstat(stream.fileno())
        data = stream.read()
        if _after_read_hook is not None:
            _after_read_hook(path)
        after = os.fstat(stream.fileno())
    path_after_lstat = path.lstat()
    path_after = path.stat()

    def identity(value: os.stat_result) -> tuple[int, int, int, int]:
        return (
            value.st_dev,
            value.st_ino,
            value.st_size,
            value.st_mtime_ns,
        )

    if (
        identity(before) != identity(after)
        or identity(after) != identity(path_after)
        or stat.S_ISLNK(path_after_lstat.st_mode)
        or (
            reparse_mask
            and getattr(path_after_lstat, "st_file_attributes", 0) & reparse_mask
        )
        or len(data) != after.st_size
    ):
        raise ValueError(f"input changed while it was being read: {path}")
    return data, path_after


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest().upper()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def file_binding(path: Path, artifact: str) -> dict[str, Any]:
    return {
        "artifact": artifact,
        "fileName": path.name,
        "bytes": path.stat().st_size,
        "sha256": sha256_file(path),
        "hostPathRecorded": False,
    }


def file_binding_from_bytes(
    path: Path, artifact: str, data: bytes,
) -> dict[str, Any]:
    return {
        "artifact": artifact,
        "fileName": path.name,
        "bytes": len(data),
        "sha256": sha256_bytes(data),
        "hostPathRecorded": False,
    }


def canonical_relative(value: str) -> str:
    candidate = value.replace("\\", "/")
    pure = PurePosixPath(candidate)
    if (
        not candidate
        or candidate.startswith("/")
        or pure.is_absolute()
        or pure.as_posix() != candidate
        or any(part in ("", ".", "..") for part in pure.parts)
        or ":" in candidate
        or "\x00" in candidate
    ):
        raise ValueError("non-canonical relative identity")
    return candidate


def canonical_staged_identity(value: str) -> str:
    """Normalize UnrealPak mount prefixes, then apply strict relative checks."""
    candidate = value.replace("\\", "/")
    while candidate.startswith("../../../"):
        candidate = candidate[9:]
    candidate = candidate.lstrip("/")
    return canonical_relative(candidate)


def is_reparse_point(path: Path) -> bool:
    try:
        info = os.lstat(path)
    except OSError:
        return True
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return path.is_symlink() or bool(
        getattr(info, "st_file_attributes", 0) & reparse_flag
    )


def collect_archive(archive: Path) -> list[dict[str, Any]]:
    root = archive.resolve(strict=True)
    if not root.is_dir() or is_reparse_point(root):
        raise ValueError("archive root is not a regular directory")
    result: list[dict[str, Any]] = []
    folded: set[str] = set()
    for current_root, directory_names, file_names in os.walk(
        root, topdown=True, followlinks=False
    ):
        current = Path(current_root)
        for name in directory_names:
            if is_reparse_point(current / name):
                raise ValueError("archive contains a reparse point")
        for name in file_names:
            candidate = current / name
            if is_reparse_point(candidate):
                raise ValueError("archive contains a reparse point")
            identity = canonical_relative(candidate.relative_to(root).as_posix())
            key = identity.casefold()
            if key in folded:
                raise ValueError("archive contains a case-insensitive path collision")
            folded.add(key)
            info = candidate.stat()
            if not stat.S_ISREG(info.st_mode):
                raise ValueError("archive contains a non-regular file")
            result.append(
                {"path": identity, "bytes": info.st_size, "sha256": sha256_file(candidate)}
            )
    result.sort(key=lambda item: item["path"].casefold())
    return result


def canonical_archive_sha256(files: Iterable[dict[str, Any]]) -> str:
    material = "".join(
        f"{item['path']}\t{item['bytes']}\t{item['sha256']}\n" for item in files
    )
    return sha256_bytes(material.encode("utf-8"))


def run_tool(command: list[str], timeout: int = 300) -> str:
    completed = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"container inventory failed with exit {completed.returncode}: "
            f"{completed.stdout[-2000:]}"
        )
    return completed.stdout


def inventory_pak(unrealpak: Path, path: Path) -> tuple[list[str], dict[str, Any]]:
    output = run_tool([str(unrealpak), str(path), "-List"])
    identities: list[str] = []
    summary_count: int | None = None
    for line in output.splitlines():
        match = PAK_ENTRY_RE.match(line)
        if match:
            identities.append(canonical_staged_identity(match.group("path")))
        summary = PAK_SUMMARY_RE.match(line)
        if summary:
            summary_count = int(summary.group(1))
    if summary_count is None or summary_count != len(identities):
        raise ValueError(
            f"pak list summary differs: summary={summary_count} parsed={len(identities)}"
        )
    if len({item.casefold() for item in identities}) != len(identities):
        raise ValueError("pak inventory contains duplicate identities")
    canonical_hash = sha256_bytes(
        "".join(f"{item}\n" for item in identities).encode("utf-8")
    )
    return identities, {
        "method": "UnrealPak positional -List",
        "entryCount": len(identities),
        "canonicalIdentitySha256": canonical_hash,
    }


def inventory_utoc(unrealpak: Path, path: Path) -> tuple[list[str], dict[str, Any]]:
    with tempfile.TemporaryDirectory(prefix="dgt-s19-v05-equipment-") as temp_name:
        csv_path = Path(temp_name) / "inventory.csv"
        run_tool([str(unrealpak), f"-ListContainer={path}", f"-CSV={csv_path}"])
        if not csv_path.is_file():
            raise ValueError("UnrealPak did not emit IoStore CSV")
        with csv_path.open("r", encoding="utf-8-sig", newline="") as stream:
            rows = list(csv.DictReader(stream, skipinitialspace=True))
    if not rows or "OrderInContainer" not in rows[0] or "Filename" not in rows[0]:
        raise ValueError("IoStore CSV is empty or malformed")
    named: list[str] = []
    anonymous: list[str] = []
    for expected_order, row in enumerate(rows):
        if int(row["OrderInContainer"]) != expected_order:
            raise ValueError("IoStore order is not contiguous")
        filename = (row.get("Filename") or "").strip()
        if filename and not (filename.startswith("<") and filename.endswith(">")):
            named.append(canonical_staged_identity(filename))
        else:
            chunk_type = (row.get("ChunkType") or "Unknown").strip() or "Unknown"
            chunk_id = (row.get("ChunkId") or "UNKNOWN").strip() or "UNKNOWN"
            offset = (row.get("Offset") or "0").strip() or "0"
            size = (row.get("Size") or "0").strip() or "0"
            digest = (row.get("Hash") or "UNKNOWN").strip().removeprefix("0x") or "UNKNOWN"
            anonymous.append(f"<{chunk_type}>#{chunk_id}@{offset}:{size}:{digest}")
    if len({item.casefold() for item in named}) != len(named):
        raise ValueError("IoStore named inventory contains duplicates")
    canonical_hash = sha256_bytes(
        "".join(f"{item}\n" for item in named + anonymous).encode("utf-8")
    )
    return named, {
        "method": "UnrealPak -ListContainer CSV",
        "chunkCount": len(rows),
        "namedEntryCount": len(named),
        "anonymousChunkCount": len(anonymous),
        "canonicalIdentitySha256": canonical_hash,
    }


def find_unrealpak(explicit: Path | None) -> Path:
    candidates = [
        explicit,
        Path(r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealPak.exe"),
        Path(r"C:\Program Files\Epic Games\UE_5.8.1\Engine\Binaries\Win64\UnrealPak.exe"),
    ]
    for candidate in candidates:
        if candidate is not None and candidate.is_file():
            return candidate.resolve()
    raise FileNotFoundError("UnrealPak.exe was not found")


def validate_policy(policy: Any, require_candidate_bindings: bool = False) -> list[str]:
    problems: list[str] = []
    if not isinstance(policy, dict):
        return ["POLICY_NOT_OBJECT"]
    if policy.get("schema") != POLICY_SCHEMA:
        problems.append("POLICY_SCHEMA")
    if policy.get("schemaVersion") != 1 or policy.get("session") != SESSION:
        problems.append("POLICY_VERSION_OR_SESSION")
    if policy.get("target") != {
        "platform": "Win64", "configuration": "Shipping", "milestone": "v0.5"
    }:
        problems.append("POLICY_TARGET")
    binding = policy.get("candidateBinding")
    if not isinstance(binding, dict) or binding.get("hostPathsMayBeRecorded") is not False:
        problems.append("POLICY_CANDIDATE_BINDING")
    elif any(value is not True for key, value in binding.items() if key != "hostPathsMayBeRecorded"):
        problems.append("POLICY_CANDIDATE_BINDING")

    catalog = policy.get("catalog")
    if not isinstance(catalog, dict):
        problems.append("POLICY_CATALOG")
    else:
        molds = catalog.get("molds")
        plastics = catalog.get("plastics")
        if molds != EXPECTED_MOLDS:
            problems.append("POLICY_MOLDS")
        if plastics != EXPECTED_PLASTICS:
            problems.append("POLICY_PLASTICS")
        if catalog.get("calibrationRepeatCount") != 5:
            problems.append("POLICY_CALIBRATION_REPEATS")
        tolerance = catalog.get("absoluteTolerance")
        if tolerance != 1e-9:
            problems.append("POLICY_TOLERANCE")
        expected_hash = catalog.get("expectedResolvedVectorSha256")
        if expected_hash != EXPECTED_VECTOR_SHA256:
            problems.append("POLICY_VECTOR_HASH")

    bag = policy.get("bag")
    if not isinstance(bag, dict) or bag.get("bagEquipmentId") != "dg_generic_bag_default":
        problems.append("POLICY_BAG")
    elif (
        bag.get("capacity") != 24
        or bag.get("stampId") != "dg_generic_default"
        or bag.get("defaultPlastic") != "Tour"
        or bag.get("selectedMold") != "Apex"
        or bag.get("selectedInstanceIndex") != 0
        or len(bag.get("instances", [])) != 5
        or [item.get("mold") for item in bag.get("instances", [])] != [
            "Apex", "Vector", "Line", "Compass", "Touch"
        ]
        or [item.get("guidWords") for item in bag.get("instances", [])] != EXPECTED_GUID_WORDS
        or any(item.get("massGrams") != 175.0 for item in bag.get("instances", []))
    ):
        problems.append("POLICY_BAG_IDENTITY")

    content = policy.get("candidateContent")
    assets = content.get("requiredExactEquipmentAssets") if isinstance(content, dict) else None
    if assets != EXPECTED_ASSETS:
        problems.append("POLICY_CONTENT_ASSETS")
    else:
        try:
            for identity in assets:
                canonical_relative(identity)
        except (TypeError, ValueError):
            problems.append("POLICY_CONTENT_ASSETS")

    binary = policy.get("binaryScan")
    if not isinstance(binary, dict) or not binary.get("requiredMarkers"):
        problems.append("POLICY_BINARY_SCAN")
    elif binary.get("forbiddenMarkers") != ["DGT_Equipment_Dev_v1"]:
        problems.append("POLICY_BINARY_FORBIDDEN")

    source_bindings = policy.get("sourceTechnicalBindings")
    if not isinstance(source_bindings, list) or [
        item.get("path") for item in source_bindings if isinstance(item, dict)
    ] != EXPECTED_SOURCE_PATHS:
        problems.append("POLICY_SOURCE_BINDINGS")
    else:
        seen_paths: set[str] = set()
        for item in source_bindings:
            if not isinstance(item, dict) or not isinstance(item.get("requiredFragments"), list):
                problems.append("POLICY_SOURCE_BINDING")
                continue
            try:
                identity = canonical_relative(item.get("path", ""))
            except (TypeError, ValueError):
                problems.append("POLICY_SOURCE_PATH")
                continue
            if identity.casefold() in seen_paths:
                problems.append("POLICY_SOURCE_DUPLICATE")
            seen_paths.add(identity.casefold())
            if not item["requiredFragments"] or not all(
                isinstance(value, str) and value for value in item["requiredFragments"]
            ):
                problems.append("POLICY_SOURCE_FRAGMENTS")

    automation = policy.get("automation")
    if not isinstance(automation, dict) or automation.get("repeatLogCount") != 2:
        problems.append("POLICY_AUTOMATION")
    elif (
        automation.get("minimumSuccessCountPerLog") != 255
        or automation.get("requireDistinctLogHashes") is not True
        or automation.get("requireZeroNonSuccessCompletions") is not True
        or automation.get("requiredTests") != EXPECTED_AUTOMATION_TESTS
    ):
        problems.append("POLICY_AUTOMATION")

    candidate_id = policy.get("candidateId")
    log_bindings = automation.get("candidateLogBindings") if isinstance(automation, dict) else None
    not_before = automation.get("candidateLogsNotBeforeUtc") if isinstance(automation, dict) else None
    freshness_tests = automation.get("candidateFreshnessRequiredTests") if isinstance(automation, dict) else None
    candidate_contract_present = any(
        value is not None
        for value in (candidate_id, log_bindings, not_before, freshness_tests)
    )
    candidate_contract_required = require_candidate_bindings or candidate_contract_present
    expected_not_before: str | None = None
    if candidate_contract_required:
        try:
            expected_not_before = candidate_build_utc_text(candidate_id)
        except (TypeError, ValueError):
            problems.append("POLICY_AUTOMATION_CANDIDATE_ID")
        if (
            not isinstance(log_bindings, list)
            or len(log_bindings) != 2
            or freshness_tests != POST_FIX_REQUIRED_TESTS
        ):
            problems.append("POLICY_AUTOMATION_CANDIDATE_BINDINGS")
        if not isinstance(not_before, str) or not_before != expected_not_before:
            problems.append("POLICY_AUTOMATION_CANDIDATE_BUILD_TIME")

    if isinstance(log_bindings, list):
        names: list[str] = []
        hashes: list[str] = []
        for item in log_bindings:
            if not isinstance(item, dict):
                problems.append("POLICY_AUTOMATION_CANDIDATE_BINDING")
                continue
            name = item.get("fileName")
            digest = item.get("sha256")
            if (
                not isinstance(name, str)
                or not candidate_log_name_matches(name, candidate_id)
                or not isinstance(item.get("bytes"), int)
                or isinstance(item.get("bytes"), bool)
                or item["bytes"] <= 0
                or not isinstance(digest, str)
                or not SHA256_RE.fullmatch(digest)
            ):
                problems.append("POLICY_AUTOMATION_CANDIDATE_BINDING")
                continue
            names.append(name.casefold())
            hashes.append(digest)
        if len(names) != 2 or len(set(names)) != 2 or len(set(hashes)) != 2:
            problems.append("POLICY_AUTOMATION_CANDIDATE_BINDINGS")

    save = policy.get("saveContract")
    if not isinstance(save, dict) or (
        save.get("productionProfileSchemaVersion") != 10
        or save.get("isolatedEquipmentSchemaVersion") != 1
        or save.get("shippingIsolatedEquipmentPersistenceAllowed") is not False
        or save.get("isolatedEquipmentPersistenceScope") != "DEVELOPMENT_AUTOMATION_ONLY"
    ):
        problems.append("POLICY_SAVE_CONTRACT")

    exclusion = policy.get("throwLabExclusionReceipt")
    if not isinstance(exclusion, dict) or set(exclusion.get("requiredTrueAssertions", [])) != {
        "throwLabShippingEntrypointAbsent",
        "throwLabShippingSaveAbsent",
        "throwLabModuleSymbolAndContentIdentityAbsent",
    }:
        problems.append("POLICY_THROW_LAB_EXCLUSION")

    assertions = policy.get("technicalAssertions")
    if not isinstance(assertions, dict) or not assertions or not all(
        value is True for value in assertions.values()
    ):
        problems.append("POLICY_TECHNICAL_ASSERTIONS")

    claim = policy.get("claimBoundary")
    false_claims = (
        "equipmentNameClearance",
        "publicEquipmentNameClaimAllowed",
        "manualPlayFeelApproval",
        "manualGameplayReviewComplete",
        "legalApproval",
        "provenanceApproval",
        "distributionClearance",
        "blockerClosureClaimed",
        "releaseReady",
    )
    if not isinstance(claim, dict) or any(claim.get(key) is not False for key in false_claims):
        problems.append("POLICY_CLAIM_BOUNDARY")
    evidence = policy.get("evidenceContract")
    if not isinstance(evidence, dict) or (
        evidence.get("immutableOutput") is not True
        or evidence.get("writeOnlyWithExplicitOutputArgument") is not True
        or evidence.get("releaseReady") is not False
    ):
        problems.append("POLICY_EVIDENCE_CONTRACT")
    return sorted(set(problems))


def source_bindings(policy: dict[str, Any]) -> tuple[list[dict[str, Any]], list[str]]:
    result: list[dict[str, Any]] = []
    problems: list[str] = []
    root = ROOT.resolve()
    for item in policy["sourceTechnicalBindings"]:
        identity = canonical_relative(item["path"])
        path = (root / identity).resolve(strict=True)
        if root not in path.parents or not path.is_file() or is_reparse_point(path):
            problems.append(f"SOURCE_PATH_INVALID:{identity}")
            continue
        text = path.read_text(encoding="utf-8-sig")
        missing = [fragment for fragment in item["requiredFragments"] if fragment not in text]
        if missing:
            problems.append(f"SOURCE_REQUIRED_FRAGMENT_MISSING:{identity}:{len(missing)}")
        result.append(
            {
                "path": identity,
                "role": item["role"],
                "bytes": path.stat().st_size,
                "sha256": sha256_file(path),
                "requiredFragmentCount": len(item["requiredFragments"]),
                "missingRequiredFragmentCount": len(missing),
                "hostPathRecorded": False,
            }
        )
    return result, problems


def lerp(start: float, end: float, alpha: float) -> float:
    return start + (end - start) * alpha


def resolved_vectors(policy: dict[str, Any]) -> list[dict[str, Any]]:
    catalog = policy["catalog"]
    base = catalog["baseAero"]
    vectors: list[dict[str, Any]] = []
    for mold in catalog["molds"]:
        speed_norm = max(0.0, min(1.0, (mold["speed"] - 2.0) / 10.0))
        for plastic in catalog["plastics"]:
            vector = {
                "mold": mold["id"],
                "plastic": plastic["id"],
                "massKg": base["massKg"],
                "diameterM": base["diameterM"],
                "areaM2": base["areaM2"],
                "inertiaAxialKgM2": base["inertiaAxialKgM2"],
                "inertiaPlanarKgM2": base["inertiaPlanarKgM2"],
                "cd0": lerp(0.135, 0.080, speed_norm),
                "cDa": base["cDa"],
                "cLa": 1.72 + 0.05 * mold["glide"],
                "cl0": 0.22 + 0.018 * mold["glide"],
                "cM0": base["cM0"],
                "cMa": base["cMa"],
                "highSpeedTurnMomentNm": max(0.0, -mold["turn"]) * 0.0035 * plastic["turnScale"],
                "lowSpeedFadeMomentNm": max(0.0, mold["fade"]) * 0.0025 * plastic["fadeScale"],
                "turnStartsAboveMps": lerp(13.5, 20.0, speed_norm),
                "fadeStartsBelowMps": lerp(12.0, 17.0, speed_norm),
                "spinDecayPerSec": base["spinDecayPerSec"],
                "groundRestitution": base["groundRestitution"] * plastic["restitutionScale"],
                "groundFriction": base["groundFriction"] * plastic["frictionScale"],
            }
            vectors.append(
                {
                    key: round(value, 9) if isinstance(value, float) else value
                    for key, value in vector.items()
                }
            )
    return vectors


def canonical_vector_sha256(vectors: list[dict[str, Any]]) -> str:
    encoded = json.dumps(
        vectors, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("ascii")
    return sha256_bytes(encoded)


def validate_calibration(policy: dict[str, Any]) -> tuple[dict[str, Any], list[str]]:
    catalog = policy["catalog"]
    tolerance = float(catalog["absoluteTolerance"])
    repeats = [resolved_vectors(policy) for _ in range(catalog["calibrationRepeatCount"])]
    hashes = [canonical_vector_sha256(value) for value in repeats]
    problems: list[str] = []
    if len(set(hashes)) != 1:
        problems.append("CALIBRATION_REPEAT_MISMATCH")
    if hashes[0] != catalog["expectedResolvedVectorSha256"]:
        problems.append("CALIBRATION_VECTOR_HASH_MISMATCH")
    by_identity = {(item["mold"], item["plastic"]): item for item in repeats[0]}
    anchor_results: list[dict[str, Any]] = []
    for anchor in catalog["anchors"]:
        key = (anchor["mold"], anchor["plastic"])
        vector = by_identity.get(key)
        mismatches: list[str] = []
        if vector is None:
            mismatches.append("IDENTITY")
        else:
            for field, expected in anchor["expected"].items():
                actual = vector.get(field)
                if not isinstance(actual, (int, float)) or not math.isclose(
                    actual, expected, rel_tol=0.0, abs_tol=tolerance
                ):
                    mismatches.append(field)
        if mismatches:
            problems.append(f"CALIBRATION_ANCHOR_MISMATCH:{anchor['mold']}:{anchor['plastic']}")
        anchor_results.append(
            {
                "mold": anchor["mold"],
                "plastic": anchor["plastic"],
                "passed": not mismatches,
                "mismatches": mismatches,
            }
        )

    mass = catalog["massCalibration"]
    base = catalog["baseAero"]
    mass_kg = mass["inputMassGrams"] / 1000.0
    scale = mass_kg / base["massKg"]
    mass_actual = {
        "massKg": mass_kg,
        "massScale": scale,
        "inertiaAxialKgM2": base["inertiaAxialKgM2"] * scale,
        "inertiaPlanarKgM2": base["inertiaPlanarKgM2"] * scale,
        "wearAffectsPhysics": False,
    }
    mass_expected = {
        "massKg": mass["expectedMassKg"],
        "massScale": mass["expectedMassScale"],
        "inertiaAxialKgM2": mass["expectedInertiaAxialKgM2"],
        "inertiaPlanarKgM2": mass["expectedInertiaPlanarKgM2"],
    }
    mass_mismatches = [
        field for field, expected in mass_expected.items()
        if not math.isclose(mass_actual[field], expected, rel_tol=0.0, abs_tol=tolerance)
    ]
    if mass_actual["wearAffectsPhysics"] is not mass["wearAffectsPhysics"]:
        mass_mismatches.append("wearAffectsPhysics")
    if mass.get("coefficientFamilyMustRemainUnchanged") is not True:
        mass_mismatches.append("coefficientFamilyMustRemainUnchanged")
    if mass_mismatches:
        problems.append("MASS_CALIBRATION_MISMATCH")
    return {
        "resolvedVectorCount": len(repeats[0]),
        "repeatCount": len(repeats),
        "repeatSha256": hashes,
        "allRepeatsIdentical": len(set(hashes)) == 1,
        "expectedResolvedVectorSha256": catalog["expectedResolvedVectorSha256"],
        "absoluteTolerance": tolerance,
        "anchors": anchor_results,
        "massCalibration": {
            "inputMassGrams": mass["inputMassGrams"],
            "actual": {key: round(value, 9) if isinstance(value, float) else value for key, value in mass_actual.items()},
            "passed": not mass_mismatches,
            "mismatches": mass_mismatches,
            "coefficientFamilyUnchanged": True,
        },
    }, problems


def parse_automation_text(
    text: str, required_tests: list[str], minimum_success: int
) -> tuple[dict[str, Any], list[str]]:
    completions = [match.groupdict() for match in COMPLETION_RE.finditer(text)]
    successes = [item for item in completions if item["result"] == "Success"]
    non_success = [item for item in completions if item["result"] != "Success"]
    counts: dict[str, int] = {}
    for item in successes:
        counts[item["path"]] = counts.get(item["path"], 0) + 1
    missing = [path for path in required_tests if counts.get(path, 0) == 0]
    duplicates = [path for path in required_tests if counts.get(path, 0) > 1]
    problems: list[str] = []
    if len(successes) < minimum_success:
        problems.append("AUTOMATION_SUCCESS_COUNT_LOW")
    if non_success:
        problems.append("AUTOMATION_NON_SUCCESS_COMPLETION")
    if missing:
        problems.append("AUTOMATION_REQUIRED_TEST_MISSING")
    if duplicates:
        problems.append("AUTOMATION_REQUIRED_TEST_DUPLICATE")
    return {
        "completedCount": len(completions),
        "successCount": len(successes),
        "nonSuccessCount": len(non_success),
        "nonSuccessResults": sorted({item["result"] for item in non_success}),
        "requiredTestCount": len(required_tests),
        "requiredSuccessCount": len(required_tests) - len(missing),
        "missingRequiredTests": missing,
        "duplicateRequiredTests": duplicates,
    }, problems


def validate_automation_logs(
    paths: list[Path], policy: dict[str, Any]
) -> tuple[list[dict[str, Any]], list[str]]:
    automation = policy["automation"]
    problems: list[str] = []
    if len(paths) != automation["repeatLogCount"]:
        problems.append("AUTOMATION_LOG_COUNT")
    result: list[dict[str, Any]] = []
    hashes: list[str] = []
    expected = automation.get("candidateLogBindings")
    candidate_id = policy.get("candidateId")
    not_before: float | None = None
    if isinstance(expected, list):
        try:
            not_before = candidate_build_utc(candidate_id).timestamp()
        except (TypeError, ValueError):
            problems.append("AUTOMATION_CANDIDATE_ID_INVALID")
    for index, path in enumerate(paths, 1):
        data, observed_stat = read_stable_regular_file(path)
        text = data.decode("utf-8-sig", errors="replace")
        summary, parse_problems = parse_automation_text(
            text,
            automation["requiredTests"],
            automation["minimumSuccessCountPerLog"],
        )
        log_problems = list(parse_problems)
        binding = file_binding_from_bytes(
            path, f"automation-repeat-{index}", data,
        )
        hashes.append(binding["sha256"])
        if isinstance(expected, list) and index <= len(expected):
            expected_binding = expected[index - 1]
            if (
                binding["fileName"] != expected_binding.get("fileName")
                or binding["bytes"] != expected_binding.get("bytes")
                or binding["sha256"] != expected_binding.get("sha256")
            ):
                log_problems.append("AUTOMATION_LOG_BINDING_MISMATCH")
            if not candidate_log_name_matches(path.name, candidate_id):
                log_problems.append("AUTOMATION_LOG_NOT_CANDIDATE_SCOPED")
            if not_before is None or observed_stat.st_mtime < not_before:
                log_problems.append("AUTOMATION_LOG_STALE")
            freshness_summary, freshness_problems = parse_automation_text(
                text,
                automation["candidateFreshnessRequiredTests"],
                0,
            )
            log_problems.extend(freshness_problems)
            if freshness_summary["requiredSuccessCount"] != len(
                automation["candidateFreshnessRequiredTests"]
            ):
                log_problems.append("AUTOMATION_LOG_POST_FIX_TEST_MISSING")
        problems.extend(f"LOG_{index}:{value}" for value in log_problems)
        result.append({"binding": binding, **summary, "passed": not log_problems})
    if automation["requireDistinctLogHashes"] and len(set(hashes)) != len(hashes):
        problems.append("AUTOMATION_LOG_HASHES_NOT_DISTINCT")
    return result, problems


def scan_markers(data: bytes, required: list[str], forbidden: list[str]) -> tuple[dict[str, Any], list[str]]:
    lowered = data.lower()
    required_rows: list[dict[str, Any]] = []
    missing: list[str] = []
    for marker in required:
        ascii_found = marker.casefold().encode("ascii") in lowered
        utf16_found = marker.casefold().encode("utf-16le") in lowered
        found = ascii_found or utf16_found
        if not found:
            missing.append(marker)
        required_rows.append(
            {"marker": marker, "foundAscii": ascii_found, "foundUtf16Le": utf16_found, "passed": found}
        )
    forbidden_rows: list[dict[str, Any]] = []
    matches: list[str] = []
    for marker in forbidden:
        ascii_found = marker.casefold().encode("ascii") in lowered
        utf16_found = marker.casefold().encode("utf-16le") in lowered
        found = ascii_found or utf16_found
        if found:
            matches.append(marker)
        forbidden_rows.append(
            {"marker": marker, "foundAscii": ascii_found, "foundUtf16Le": utf16_found, "passed": not found}
        )
    problems = []
    if missing:
        problems.append("BINARY_REQUIRED_MARKER_MISSING")
    if matches:
        problems.append("BINARY_FORBIDDEN_MARKER_PRESENT")
    return {
        "caseInsensitive": True,
        "encodings": ["ascii", "utf-16le"],
        "required": required_rows,
        "missingRequiredMarkers": missing,
        "forbidden": forbidden_rows,
        "forbiddenMatches": matches,
    }, problems


def validate_equipment_asset_set(
    identities: Iterable[str], expected: list[str], prefixes: list[str]
) -> tuple[dict[str, Any], list[str]]:
    matched = [
        identity for identity in identities if any(identity.startswith(prefix) for prefix in prefixes)
    ]
    counts: dict[str, int] = {}
    for identity in matched:
        counts[identity] = counts.get(identity, 0) + 1
    actual = sorted(counts, key=str.casefold)
    expected_sorted = sorted(expected, key=str.casefold)
    missing = sorted(set(expected_sorted) - set(actual), key=str.casefold)
    extra = sorted(set(actual) - set(expected_sorted), key=str.casefold)
    duplicates = sorted(
        [identity for identity, count in counts.items() if count != 1], key=str.casefold
    )
    problems: list[str] = []
    if missing:
        problems.append("EQUIPMENT_ASSET_MISSING")
    if extra:
        problems.append("EQUIPMENT_ASSET_EXTRA")
    if duplicates:
        problems.append("EQUIPMENT_ASSET_DUPLICATE")
    return {
        "expectedCount": len(expected_sorted),
        "actualCount": len(actual),
        "actual": actual,
        "missing": missing,
        "extra": extra,
        "duplicates": duplicates,
        "exactSet": not missing and not extra and not duplicates,
    }, problems


def validate_fresh_manifest(data: Any, candidate_id: str, policy: dict[str, Any]) -> tuple[dict[str, Any], list[str]]:
    problems: list[str] = []
    save = policy["saveContract"]
    if not isinstance(data, dict):
        return {}, ["FRESH_MANIFEST_NOT_OBJECT"]
    if data.get("schema") != "DiscGolfTour.Session19FreshUserDirValidation.v1":
        problems.append("FRESH_MANIFEST_SCHEMA")
    if data.get("candidateId") != candidate_id:
        problems.append("FRESH_MANIFEST_CANDIDATE")
    if data.get("state") != "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST" or data.get("failures") != []:
        problems.append("FRESH_MANIFEST_STATE")
    counts = data.get("counts") if isinstance(data.get("counts"), dict) else {}
    if counts.get("unknownFiles") != 0:
        problems.append("FRESH_MANIFEST_UNKNOWN_FILES")
    if counts.get("forbiddenArtifacts") != 0:
        problems.append("FRESH_MANIFEST_FORBIDDEN_ARTIFACTS")
    observed = data.get("observedFiles") if isinstance(data.get("observedFiles"), list) else []
    profiles = [
        item for item in observed
        if isinstance(item, dict)
        and item.get("relativePath") == save["productionProfilePath"]
        and item.get("classification") == "production_profile"
    ]
    if len(profiles) != 1:
        problems.append("FRESH_MANIFEST_PROFILE")
    forbidden_equipment = [
        item.get("relativePath", "") for item in observed
        if isinstance(item, dict)
        and (
            item.get("classification") == save["freshUserDirForbiddenArtifactClass"]
            or "equipment" in str(item.get("relativePath", "")).casefold()
        )
    ]
    if forbidden_equipment:
        problems.append("FRESH_MANIFEST_EQUIPMENT_SAVE")
    profile = profiles[0] if len(profiles) == 1 else None
    return {
        "candidateUserDirBindingSha256": data.get("candidateUserDirBindingSha256"),
        "userDirToken": data.get("userDirToken"),
        "observedFileCount": counts.get("observedFiles"),
        "unknownFileCount": counts.get("unknownFiles"),
        "forbiddenArtifactCount": counts.get("forbiddenArtifacts"),
        "productionProfile": profile,
        "isolatedEquipmentSaveCount": len(forbidden_equipment),
        "passed": not problems,
    }, problems


def validate_exclusion_receipt(data: Any, candidate_id: str, policy: dict[str, Any]) -> tuple[dict[str, Any], list[str]]:
    problems: list[str] = []
    contract = policy["throwLabExclusionReceipt"]
    if not isinstance(data, dict):
        return {}, ["EXCLUSION_RECEIPT_NOT_OBJECT"]
    if data.get("schema") != contract["schema"]:
        problems.append("EXCLUSION_RECEIPT_SCHEMA")
    if data.get("candidateId") != candidate_id:
        problems.append("EXCLUSION_RECEIPT_CANDIDATE")
    if data.get("state") != contract["passState"] or data.get("passed") is not True:
        problems.append("EXCLUSION_RECEIPT_STATE")
    if data.get("failures") != []:
        problems.append("EXCLUSION_RECEIPT_FAILURES")
    assertions = data.get("technicalAssertions") if isinstance(data.get("technicalAssertions"), dict) else {}
    for key in contract["requiredTrueAssertions"]:
        if assertions.get(key) is not True:
            problems.append(f"EXCLUSION_ASSERTION_FALSE:{key}")
    claim = data.get("claimBoundary") if isinstance(data.get("claimBoundary"), dict) else {}
    for key in (
        "publicThrowLabClaimAllowed",
        "manualGameplayReviewComplete",
        "humanPlayFeelApproval",
        "legalApproval",
        "distributionClearance",
        "blockerClosureClaimed",
        "releaseReady",
    ):
        if claim.get(key) is not False:
            problems.append(f"EXCLUSION_CLAIM_NOT_FALSE:{key}")
    return {
        "sourceReceiptSchema": data.get("schema"),
        "sourceReceiptState": data.get("state"),
        "requiredAssertions": {key: assertions.get(key) for key in contract["requiredTrueAssertions"]},
        "manualPublicLegalReleaseClaimsRemainFalse": not any(
            problem.startswith("EXCLUSION_CLAIM_NOT_FALSE") for problem in problems
        ),
        "passed": not problems,
    }, problems


def validate_upstream_receipts(
    candidate_id: str,
    archive_summary: dict[str, Any],
    exe_entry: dict[str, Any],
    verification: Any,
    content: Any,
    binary: Any,
) -> list[str]:
    problems: list[str] = []
    if not isinstance(verification, dict) or (
        verification.get("schema") != "DiscGolfTour.Session19ShippingCandidateVerificationReceipt.v2"
        or verification.get("runId") != candidate_id
        or not str(verification.get("state", "")).startswith("PASS_")
    ):
        problems.append("VERIFICATION_RECEIPT")
    else:
        inner = verification.get("innerShippingExecutable", {})
        if (
            inner.get("relativePath") != INNER_EXE
            or str(inner.get("sha256", "")).upper() != exe_entry["sha256"]
            or inner.get("bytes") != exe_entry["bytes"]
        ):
            problems.append("VERIFICATION_EXECUTABLE_BINDING")
    if not isinstance(content, dict) or (
        content.get("schema") != "DiscGolfTour.Session19CandidateContentAuditReceipt.v1"
        or content.get("runId") != candidate_id
        or content.get("state") != "PASS_BOUNDED_STAGED_CONTENT_AUDIT"
        or content.get("issues") != []
    ):
        problems.append("CONTENT_RECEIPT")
    else:
        archived = content.get("archive", {})
        if any(archived.get(key) != archive_summary[key] for key in ("fileCount", "bytes", "canonicalManifestSha256")):
            problems.append("CONTENT_ARCHIVE_BINDING")
    if not isinstance(binary, dict) or (
        binary.get("schema") != "DiscGolfTour.Session19ShippingBinaryEvidence.v1"
        or binary.get("state") != "PASS_BINARY_MARKER_POLICY_ONLY"
        or binary.get("passed") is not True
        or binary.get("errors") != []
        or binary.get("scan", {}).get("forbiddenMatchCount") != 0
    ):
        problems.append("BINARY_RECEIPT")
    else:
        binary_input = binary.get("input", {})
        if (
            str(binary_input.get("sha256", "")).upper() != exe_entry["sha256"]
            or binary_input.get("bytes") != exe_entry["bytes"]
        ):
            problems.append("BINARY_EXECUTABLE_BINDING")
    return problems


def validate_container_receipt_binding(
    actual_rows: list[dict[str, Any]], content: dict[str, Any]
) -> list[str]:
    problems: list[str] = []
    receipt_rows = content.get("inventory", {}).get("containers", [])
    by_path = {
        row.get("path"): row for row in receipt_rows if isinstance(row, dict) and isinstance(row.get("path"), str)
    }
    for actual in actual_rows:
        expected = by_path.get(actual["path"])
        if expected is None:
            problems.append(f"CONTENT_CONTAINER_RECEIPT_MISSING:{actual['path']}")
            continue
        for key in ("bytes", "sha256", "method", "canonicalIdentitySha256"):
            if str(expected.get(key)).upper() != str(actual.get(key)).upper():
                problems.append(f"CONTENT_CONTAINER_BINDING:{actual['path']}:{key}")
        for key in ("entryCount", "chunkCount", "namedEntryCount", "anonymousChunkCount"):
            if key in actual and expected.get(key) != actual[key]:
                problems.append(f"CONTENT_CONTAINER_BINDING:{actual['path']}:{key}")
    return problems


def build_receipt(args: argparse.Namespace, policy: dict[str, Any]) -> dict[str, Any]:
    failures: list[str] = []
    archive_files = collect_archive(args.archive)
    by_path = {item["path"]: item for item in archive_files}
    required_files = policy["requiredArchiveFiles"]
    missing_archive = [identity for identity in required_files if identity not in by_path]
    if missing_archive:
        failures.append("ARCHIVE_REQUIRED_FILE_MISSING")
    archive_summary = {
        "fileCount": len(archive_files),
        "bytes": sum(item["bytes"] for item in archive_files),
        "canonicalManifestSha256": canonical_archive_sha256(archive_files),
        "missingRequiredFiles": missing_archive,
        "hostPathRecorded": False,
        "recoveryLocationToken": f"DGTOUR_PACKAGES/{args.candidate_id}/Windows",
    }
    if INNER_EXE not in by_path:
        raise ValueError("inner Shipping executable is absent")
    exe_path = args.archive / Path(INNER_EXE)

    verification = load_json(args.verification)
    content = load_json(args.content_receipt)
    binary = load_json(args.binary_receipt)
    fresh = load_json(args.fresh_userdir_manifest)
    exclusion = load_json(args.feature_exclusion_receipt)
    failures.extend(
        validate_upstream_receipts(
            args.candidate_id, archive_summary, by_path[INNER_EXE], verification, content, binary
        )
    )

    unrealpak = find_unrealpak(args.unrealpak)
    all_named: list[str] = []
    container_rows: list[dict[str, Any]] = []
    for identity, inventory_function in (
        ("DiscGolfTour/Content/Paks/DiscGolfTour-Windows.pak", inventory_pak),
        ("DiscGolfTour/Content/Paks/DiscGolfTour-Windows.utoc", inventory_utoc),
    ):
        path = args.archive / Path(identity)
        names, summary = inventory_function(unrealpak, path)
        all_named.extend(names)
        entry = by_path[identity]
        container_rows.append(
            {"path": identity, "bytes": entry["bytes"], "sha256": entry["sha256"], **summary}
        )
    failures.extend(validate_container_receipt_binding(container_rows, content))
    equipment_assets, asset_problems = validate_equipment_asset_set(
        all_named,
        policy["candidateContent"]["requiredExactEquipmentAssets"],
        policy["candidateContent"]["equipmentAssetPrefixes"],
    )
    failures.extend(asset_problems)

    binary_scan, binary_problems = scan_markers(
        exe_path.read_bytes(),
        policy["binaryScan"]["requiredMarkers"],
        policy["binaryScan"]["forbiddenMarkers"],
    )
    failures.extend(binary_problems)

    sources, source_problems = source_bindings(policy)
    failures.extend(source_problems)
    calibration, calibration_problems = validate_calibration(policy)
    failures.extend(calibration_problems)
    automation, automation_problems = validate_automation_logs(args.automation_log, policy)
    failures.extend(automation_problems)
    fresh_summary, fresh_problems = validate_fresh_manifest(fresh, args.candidate_id, policy)
    failures.extend(fresh_problems)
    exclusion_summary, exclusion_problems = validate_exclusion_receipt(
        exclusion, args.candidate_id, policy
    )
    failures.extend(exclusion_problems)

    unique_failures = sorted(set(failures))
    passed = not unique_failures
    evidence = policy["evidenceContract"]
    return {
        "schema": SCHEMA,
        "schemaVersion": 1,
        "session": SESSION,
        "candidateId": args.candidate_id,
        "generatedUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "state": evidence["passState"] if passed else evidence["failState"],
        "passed": passed,
        "failures": unique_failures,
        "archive": archive_summary,
        "candidateEquipmentContent": {
            "inventoryTool": file_binding(unrealpak, "container-inventory-tool"),
            "containers": container_rows,
            "equipmentAssets": equipment_assets,
        },
        "candidateBinary": {
            "relativePath": INNER_EXE,
            "bytes": by_path[INNER_EXE]["bytes"],
            "sha256": by_path[INNER_EXE]["sha256"],
            "scan": binary_scan,
        },
        "catalogAndBag": {
            "catalogAuthority": policy["catalog"]["authority"],
            "molds": policy["catalog"]["molds"],
            "plastics": policy["catalog"]["plastics"],
            "bag": policy["bag"],
            "sourceTechnicalBindings": sources,
            "candidateSemanticInference": policy["claimBoundary"]["candidateSemanticInference"],
        },
        "flightPhysicsCalibration": calibration,
        "automationRepeats": automation,
        "saveSchemaAndAtomicity": {
            "productionProfileSchemaVersion": policy["saveContract"]["productionProfileSchemaVersion"],
            "selectedMoldField": policy["saveContract"]["selectedMoldField"],
            "selectedPlasticField": policy["saveContract"]["selectedPlasticField"],
            "legacyEquipmentMigrationThreshold": policy["saveContract"]["legacyEquipmentMigrationThreshold"],
            "legacyDefaults": {
                "mold": policy["saveContract"]["legacyDefaultMold"],
                "plastic": policy["saveContract"]["legacyDefaultPlastic"],
            },
            "isolatedEquipmentSchemaVersion": policy["saveContract"]["isolatedEquipmentSchemaVersion"],
            "isolatedEquipmentPersistenceScope": policy["saveContract"]["isolatedEquipmentPersistenceScope"],
            "shippingIsolatedEquipmentPersistenceAllowed": False,
            "freshUserDir": fresh_summary,
            "migrationAndAtomicityAutomationPaths": [
                path for path in policy["automation"]["requiredTests"]
                if "SaveSchema" in path or "Profile." in path or "LieSaveSerialization" in path or "Equipment." in path
            ],
        },
        "throwLabExclusion": exclusion_summary,
        "technicalAssertions": policy["technicalAssertions"] if passed else {
            key: False for key in policy["technicalAssertions"]
        },
        "bindings": {
            "policy": file_binding(args.policy, "policy"),
            "validator": file_binding(Path(__file__), "validator"),
            "candidateVerification": file_binding(args.verification, "candidate-verification"),
            "candidateContent": file_binding(args.content_receipt, "candidate-content"),
            "shippingBinary": file_binding(args.binary_receipt, "shipping-binary"),
            "freshUserDirManifest": file_binding(args.fresh_userdir_manifest, "fresh-userdir-manifest"),
            "featureExclusionReceipt": file_binding(args.feature_exclusion_receipt, "feature-exclusion-receipt"),
        },
        "claimBoundary": {
            "technicalEvidenceOnly": True,
            "equipmentNameClearance": False,
            "publicEquipmentNameClaimAllowed": False,
            "manualPlayFeelApproval": False,
            "manualGameplayReviewComplete": False,
            "legalApproval": False,
            "provenanceApproval": False,
            "distributionClearance": False,
            "blockerClosureClaimed": False,
            "releaseReady": False,
        },
        "disclaimer": evidence["disclaimer"],
    }


def write_immutable(path: Path, data: dict[str, Any]) -> None:
    if path.exists() or path.is_symlink():
        raise FileExistsError(f"refusing to overwrite immutable output: {path}")
    parent = path.parent.resolve(strict=True)
    if not parent.is_dir() or is_reparse_point(parent):
        raise ValueError("output parent is not a regular directory")
    encoded = (json.dumps(data, indent=2, sort_keys=True) + "\n").encode("utf-8")
    with path.open("xb") as stream:
        stream.write(encoded)
        stream.flush()
        os.fsync(stream.fileno())


def make_automation_fixture(required: list[str], total: int = 255) -> str:
    paths = list(required) + [f"Fixture.Pass.{index}" for index in range(total - len(required))]
    return "\n".join(
        f"Test Completed. Result={{Success}} Name={{T}} Path={{{path}}}" for path in paths
    )


def run_self_tests(policy: dict[str, Any]) -> tuple[int, int, list[str]]:
    passed = 0
    failures: list[str] = []

    def check(name: str, condition: bool) -> None:
        nonlocal passed
        if condition:
            passed += 1
        else:
            failures.append(name)

    check("valid-policy", validate_policy(policy) == [])
    mutated_policy = json.loads(json.dumps(policy))
    mutated_policy["bag"]["instances"][0]["guidWords"][0] += 1
    check("policy-bag-identity-mutation", "POLICY_BAG_IDENTITY" in validate_policy(mutated_policy))
    mutated_policy = json.loads(json.dumps(policy))
    mutated_policy["candidateContent"]["requiredExactEquipmentAssets"][0] += ".unexpected"
    check("policy-asset-identity-mutation", "POLICY_CONTENT_ASSETS" in validate_policy(mutated_policy))
    mutated_policy = json.loads(json.dumps(policy))
    mutated_policy["claimBoundary"]["releaseReady"] = True
    check("policy-release-claim-mutation", "POLICY_CLAIM_BOUNDARY" in validate_policy(mutated_policy))
    mutated_policy = json.loads(json.dumps(policy))
    mutated_policy["automation"]["requiredTests"].pop()
    check("policy-automation-removal", "POLICY_AUTOMATION" in validate_policy(mutated_policy))
    mutated_policy = json.loads(json.dumps(policy))
    mutated_policy["sourceTechnicalBindings"].pop(0)
    check("policy-source-binding-removal", "POLICY_SOURCE_BINDINGS" in validate_policy(mutated_policy))

    candidate_policy_id = "S19_WindowsShipping_20000101T000000Z_000000000001"
    candidate_policy = json.loads(json.dumps(policy))
    candidate_policy["candidateId"] = candidate_policy_id
    candidate_policy["automation"]["candidateLogsNotBeforeUtc"] = (
        candidate_build_utc_text(candidate_policy_id)
    )
    candidate_policy["automation"]["candidateFreshnessRequiredTests"] = (
        POST_FIX_REQUIRED_TESTS
    )
    candidate_policy["automation"]["candidateLogBindings"] = [
        {
            "fileName": f"Automation_{candidate_policy_id}_A.log",
            "bytes": 1,
            "sha256": "A" * 64,
        },
        {
            "fileName": f"Automation_{candidate_policy_id}_B.log",
            "bytes": 2,
            "sha256": "B" * 64,
        },
    ]
    check(
        "candidate-policy-valid",
        validate_policy(candidate_policy, require_candidate_bindings=True) == [],
    )
    backdated_policy = json.loads(json.dumps(candidate_policy))
    backdated_policy["automation"]["candidateLogsNotBeforeUtc"] = (
        "1970-01-01T00:00:00Z"
    )
    check(
        "candidate-policy-backdated-epoch",
        "POLICY_AUTOMATION_CANDIDATE_BUILD_TIME"
        in validate_policy(backdated_policy, require_candidate_bindings=True),
    )
    unscoped_policy = json.loads(json.dumps(candidate_policy))
    unscoped_policy["automation"]["candidateLogBindings"][0]["fileName"] = (
        "Automation_Unscoped_A.log"
    )
    check(
        "candidate-policy-unscoped-name",
        "POLICY_AUTOMATION_CANDIDATE_BINDING"
        in validate_policy(unscoped_policy, require_candidate_bindings=True),
    )
    try:
        load_json_text('{"a":1,"a":2}')
        check("duplicate-json", False)
    except DuplicateKeyError:
        check("duplicate-json", True)
    try:
        load_json_text('{"a":NaN}')
        check("nonfinite-json", False)
    except ValueError:
        check("nonfinite-json", True)
    for name, value in (("traversal", "../x"), ("absolute", "/x"), ("drive", "C:/x")):
        try:
            canonical_relative(value)
            check(name, False)
        except ValueError:
            check(name, True)
    check(
        "unreal-mount-prefix-normalization",
        canonical_staged_identity("../../../DiscGolfTour/Content/A.uasset")
        == "DiscGolfTour/Content/A.uasset",
    )

    calibration, calibration_problems = validate_calibration(policy)
    check("calibration-valid", not calibration_problems)
    check("calibration-repeat", calibration["allRepeatsIdentical"])
    mutated = json.loads(json.dumps(policy))
    mutated["catalog"]["molds"][0]["turn"] = -1.01
    check("calibration-mutation", "CALIBRATION_VECTOR_HASH_MISMATCH" in validate_calibration(mutated)[1])
    mutated = json.loads(json.dumps(policy))
    mutated["catalog"]["anchors"][0]["expected"]["cd0"] = 0.081
    check("anchor-mutation", any(value.startswith("CALIBRATION_ANCHOR_MISMATCH") for value in validate_calibration(mutated)[1]))
    mutated = json.loads(json.dumps(policy))
    mutated["catalog"]["massCalibration"]["expectedMassScale"] = 0.81
    check("mass-mutation", "MASS_CALIBRATION_MISMATCH" in validate_calibration(mutated)[1])

    required = policy["automation"]["requiredTests"]
    fixture = make_automation_fixture(required)
    check("automation-valid", parse_automation_text(fixture, required, 255)[1] == [])
    check("automation-missing", "AUTOMATION_REQUIRED_TEST_MISSING" in parse_automation_text(fixture.replace(required[0], "Fixture.Replaced", 1), required, 255)[1])
    check("automation-duplicate", "AUTOMATION_REQUIRED_TEST_DUPLICATE" in parse_automation_text(fixture + "\n" + make_automation_fixture([required[0]], 1), required, 255)[1])
    check("automation-failure", "AUTOMATION_NON_SUCCESS_COMPLETION" in parse_automation_text(fixture + "\nTest Completed. Result={Fail} Name={T} Path={Fixture.Fail}", required, 255)[1])
    check("automation-low-count", "AUTOMATION_SUCCESS_COUNT_LOW" in parse_automation_text(make_automation_fixture(required, len(required)), required, 255)[1])

    expected = policy["candidateContent"]["requiredExactEquipmentAssets"]
    prefixes = policy["candidateContent"]["equipmentAssetPrefixes"]
    check("assets-valid", validate_equipment_asset_set(expected, expected, prefixes)[1] == [])
    check("assets-missing", "EQUIPMENT_ASSET_MISSING" in validate_equipment_asset_set(expected[1:], expected, prefixes)[1])
    check("assets-extra", "EQUIPMENT_ASSET_EXTRA" in validate_equipment_asset_set(expected + [prefixes[0] + "Unexpected.uasset"], expected, prefixes)[1])
    check("assets-duplicate", "EQUIPMENT_ASSET_DUPLICATE" in validate_equipment_asset_set(expected + [expected[0]], expected, prefixes)[1])

    marker_data = b"required\x00m\x00a\x00r\x00k\x00e\x00r\x00"
    check("markers-valid", scan_markers(marker_data, ["required", "marker"], ["forbidden"])[1] == [])
    check("markers-missing", "BINARY_REQUIRED_MARKER_MISSING" in scan_markers(marker_data, ["absent"], [])[1])
    check("markers-forbidden", "BINARY_FORBIDDEN_MARKER_PRESENT" in scan_markers(marker_data + b"forbidden", [], ["forbidden"])[1])

    candidate = "S19_WindowsShipping_FIXTURE"
    fresh = {
        "schema": "DiscGolfTour.Session19FreshUserDirValidation.v1",
        "candidateId": candidate,
        "state": "PASS_FRESH_SHIPPING_USERDIR_ALLOWLIST",
        "failures": [],
        "counts": {"observedFiles": 1, "unknownFiles": 0, "forbiddenArtifacts": 0},
        "observedFiles": [{"relativePath": policy["saveContract"]["productionProfilePath"], "classification": "production_profile", "bytes": 1, "sha256": "A" * 64}],
    }
    check("fresh-valid", validate_fresh_manifest(fresh, candidate, policy)[1] == [])
    bad_fresh = json.loads(json.dumps(fresh))
    bad_fresh["observedFiles"].append({"relativePath": "Saved/SaveGames/DGT_Equipment_Dev_v1.sav", "classification": "DEVELOPMENT_EQUIPMENT_SAVE"})
    check("fresh-equipment-save", "FRESH_MANIFEST_EQUIPMENT_SAVE" in validate_fresh_manifest(bad_fresh, candidate, policy)[1])

    exclusion = {
        "schema": policy["throwLabExclusionReceipt"]["schema"],
        "candidateId": candidate,
        "state": policy["throwLabExclusionReceipt"]["passState"],
        "passed": True,
        "failures": [],
        "technicalAssertions": {key: True for key in policy["throwLabExclusionReceipt"]["requiredTrueAssertions"]},
        "claimBoundary": {
            "publicThrowLabClaimAllowed": False, "manualGameplayReviewComplete": False,
            "humanPlayFeelApproval": False, "legalApproval": False,
            "distributionClearance": False, "blockerClosureClaimed": False, "releaseReady": False,
        },
    }
    check("exclusion-valid", validate_exclusion_receipt(exclusion, candidate, policy)[1] == [])
    bad_exclusion = json.loads(json.dumps(exclusion))
    bad_exclusion["technicalAssertions"]["throwLabShippingSaveAbsent"] = False
    check("exclusion-assertion", any(value.startswith("EXCLUSION_ASSERTION_FALSE") for value in validate_exclusion_receipt(bad_exclusion, candidate, policy)[1]))
    bad_exclusion = json.loads(json.dumps(exclusion))
    bad_exclusion["candidateId"] = "S19_WindowsShipping_WRONG"
    check("exclusion-candidate", "EXCLUSION_RECEIPT_CANDIDATE" in validate_exclusion_receipt(bad_exclusion, candidate, policy)[1])
    bad_exclusion = json.loads(json.dumps(exclusion))
    bad_exclusion["claimBoundary"]["publicThrowLabClaimAllowed"] = True
    check("exclusion-public-claim", any(value.startswith("EXCLUSION_CLAIM_NOT_FALSE") for value in validate_exclusion_receipt(bad_exclusion, candidate, policy)[1]))

    with tempfile.TemporaryDirectory(prefix="dgt-equipment-selftest-") as temp_name:
        temp_root = Path(temp_name)
        output = temp_root / "receipt.json"
        write_immutable(output, {"ok": True})
        immutable_rejected = False
        try:
            write_immutable(output, {"ok": False})
        except FileExistsError:
            immutable_rejected = True
        check("immutable-output", immutable_rejected)

        mutation_input = temp_root / "mutation.log"
        mutation_input.write_bytes(b"stable automation bytes")

        def mutate_after_read(path: Path) -> None:
            with path.open("ab") as stream:
                stream.write(b" mutated")
                stream.flush()
                os.fsync(stream.fileno())

        try:
            read_stable_regular_file(
                mutation_input, _after_read_hook=mutate_after_read,
            )
        except ValueError as exc:
            check("automation-stable-read-mutation", "changed while" in str(exc))
        else:
            check("automation-stable-read-mutation", False)

        stale_policy = json.loads(json.dumps(candidate_policy))
        stale_paths: list[Path] = []
        stale_bindings: list[dict[str, Any]] = []
        combined_fixture = make_automation_fixture(
            required + POST_FIX_REQUIRED_TESTS
        )
        stale_timestamp = candidate_build_utc(candidate_policy_id).timestamp() - 1.0
        for suffix in ("A", "B"):
            stale_path = temp_root / f"Automation_{candidate_policy_id}_{suffix}.log"
            stale_data = (combined_fixture + f"\nCopiedPreCandidate.{suffix}").encode(
                "utf-8"
            )
            stale_path.write_bytes(stale_data)
            os.utime(stale_path, (stale_timestamp, stale_timestamp))
            stale_paths.append(stale_path)
            stale_bindings.append({
                "fileName": stale_path.name,
                "bytes": len(stale_data),
                "sha256": sha256_bytes(stale_data),
            })
        stale_policy["automation"]["candidateLogBindings"] = stale_bindings
        check(
            "copied-stale-policy-valid",
            validate_policy(stale_policy, require_candidate_bindings=True) == [],
        )
        stale_rows, stale_problems = validate_automation_logs(
            stale_paths, stale_policy,
        )
        check(
            "copied-renamed-stale-logs",
            all(
                f"LOG_{index}:AUTOMATION_LOG_STALE" in stale_problems
                for index in (1, 2)
            )
            and all(row["passed"] is False for row in stale_rows),
        )

    total = passed + len(failures)
    return passed, total, failures


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policy", type=Path, default=DEFAULT_POLICY)
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--verification", type=Path)
    parser.add_argument("--content-receipt", type=Path)
    parser.add_argument("--binary-receipt", type=Path)
    parser.add_argument("--fresh-userdir-manifest", type=Path)
    parser.add_argument("--feature-exclusion-receipt", type=Path)
    parser.add_argument("--automation-log", action="append", type=Path, default=[])
    parser.add_argument("--unrealpak", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        policy = load_json(args.policy)
        policy_problems = validate_policy(policy, require_candidate_bindings=not args.self_test)
        if policy_problems:
            raise ValueError("invalid policy: " + ", ".join(policy_problems))
        if args.self_test:
            passed, total, failures = run_self_tests(policy)
            if failures:
                print(f"SELF_TEST FAIL {passed}/{total}: {', '.join(failures)}", file=sys.stderr)
                return 1
            print(f"SELF_TEST PASS {passed}/{total}")
            return 0
        required = {
            "candidate-id": args.candidate_id,
            "archive": args.archive,
            "verification": args.verification,
            "content-receipt": args.content_receipt,
            "binary-receipt": args.binary_receipt,
            "fresh-userdir-manifest": args.fresh_userdir_manifest,
            "feature-exclusion-receipt": args.feature_exclusion_receipt,
            "output": args.output,
        }
        missing = [name for name, value in required.items() if value is None]
        if missing:
            raise ValueError("missing required arguments: " + ", ".join(missing))
        if not CANDIDATE_RE.fullmatch(args.candidate_id):
            raise ValueError("invalid candidate ID")
        if policy.get("candidateId") != args.candidate_id:
            raise ValueError("policy candidate ID does not match --candidate-id")
        receipt = build_receipt(args, policy)
        write_immutable(args.output, receipt)
        print(f"{receipt['state']} failures={len(receipt['failures'])} output={args.output}")
        return 0 if receipt["passed"] else 1
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
        print(f"FAIL: {type(exc).__name__}: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
