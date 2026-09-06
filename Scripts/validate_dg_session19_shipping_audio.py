#!/usr/bin/env python3
"""Prove the twelve Session 19 generated SoundWaves exist in one fresh Shipping cook.

This validator is deliberately technical. It binds source/import identities, the
final Windows archive, IoStore package entries, the packaged AssetRegistry class
inventory, and extracted cooked-object parts. UE 5.8 class identity is proven by
the packaged AssetRegistry rather than an invalid raw-export ASCII marker
assumption. It does not prove audible playback, mix quality, accessibility, legal
clearance, product-owner approval, or release readiness.
"""

from __future__ import annotations

import argparse
import copy
import csv
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "Config/DG_Session19ShippingAudioPolicy.json"
RUN_ID_RE = re.compile(r"^S19_WindowsShipping_[A-Za-z0-9_-]+$")
SHA256_RE = re.compile(r"^[0-9A-F]{64}$")
IOSTORE_HASH_RE = re.compile(r"^0x[0-9a-fA-F]{40}$")
STATE = (
    "PASS_CANDIDATE_BOUND_ALL_TWELVE_GENERATED_SOUNDWAVES_COOKED_"
    "AUDIBLE_PLAYBACK_AND_HUMAN_APPROVALS_PENDING"
)


class ValidationError(ValueError):
    pass


def reject_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValidationError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json_bytes(data: bytes, label: str) -> Any:
    try:
        return json.loads(
            data.decode("utf-8-sig"),
            object_pairs_hook=reject_duplicates,
            parse_constant=lambda value: (_ for _ in ()).throw(
                ValidationError(f"non-finite JSON value in {label}: {value}")
            ),
        )
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ValidationError(f"invalid JSON {label}: {exc}") from exc


def read_stable(path: Path) -> bytes:
    before = path.stat()
    data = path.read_bytes()
    after = path.stat()
    if before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns:
        raise ValidationError(f"file changed while read: {path}")
    return data


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def identity(path: Path, data: bytes | None = None) -> dict[str, Any]:
    payload = read_stable(path) if data is None else data
    return {"bytes": len(payload), "sha256": sha256(payload)}


def canonical_relative(value: str) -> str:
    if not isinstance(value, str) or not value or "\\" in value or "\x00" in value:
        raise ValidationError(f"non-canonical relative path: {value!r}")
    pure = PurePosixPath(value)
    if pure.is_absolute() or any(part in ("", ".", "..") for part in pure.parts):
        raise ValidationError(f"unsafe relative path: {value!r}")
    if pure.as_posix() != value or ":" in value:
        raise ValidationError(f"non-canonical relative path: {value!r}")
    return value


def is_reparse(path: Path) -> bool:
    try:
        info = os.stat(path, follow_symlinks=False)
    except OSError:
        return False
    attributes = getattr(info, "st_file_attributes", 0)
    return bool(attributes & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0))


def exact_file(path: Path, label: str) -> Path:
    if not path.is_file() or path.is_symlink() or is_reparse(path):
        raise ValidationError(f"{label} must be a regular non-reparse file")
    return path.resolve(strict=True)


def exact_binding(path: Path, expected: dict[str, Any], label: str) -> bytes:
    data = read_stable(path)
    if len(data) != expected.get("bytes") or sha256(data) != expected.get("sha256"):
        raise ValidationError(f"exact source identity differs: {label}")
    return data


def expected_categories(policy: dict[str, Any]) -> list[str]:
    return list(policy["orderedCategories"])


def expected_object(category: str) -> str:
    return f"/Game/Presentation/Audio/Generated/SW_{category}.SW_{category}"


def expected_container_filename(category: str) -> str:
    return (
        "../../../DiscGolfTour/Content/Presentation/Audio/Generated/"
        f"SW_{category}.uasset"
    )


def validate_policy(policy: Any) -> None:
    if not isinstance(policy, dict):
        raise ValidationError("Shipping audio policy root must be an object")
    expected_keys = {
        "schema", "schemaVersion", "session", "policyId", "state",
        "receiptPattern", "selfTestMinimumMutationCount", "sourceBindings",
        "cookRoot", "orderedCategories", "archive", "proofRequirements",
        "resultBoundary",
    }
    if set(policy) != expected_keys:
        raise ValidationError("Shipping audio policy keys differ")
    expected_head = {
        "schema": "DiscGolfTour.Session19ShippingAudioCookPolicy.v1",
        "schemaVersion": 1,
        "session": 19,
        "policyId": "session19_original_audio_twelve_soundwave_shipping_cook_v1",
        "state": "SOURCE_AND_IMPORTED_SOUNDWAVES_BOUND_FRESH_SHIPPING_COOK_PROOF_PENDING",
        "receiptPattern": "Evidence/Session19/ShippingAudioCook-{candidateId}.json",
        "selfTestMinimumMutationCount": 45,
        "cookRoot": "/Game/Presentation/Audio/Generated",
    }
    for key, expected in expected_head.items():
        if policy.get(key) != expected:
            raise ValidationError(f"Shipping audio policy differs: {key}")
    categories = policy.get("orderedCategories")
    expected = [
        "ThrowRelease", "AirborneFlight", "GroundContact", "GroundState",
        "BasketOutcome", "Penalty", "HoleStart", "HoleCompletion",
        "HoleTransition", "RoundCompletion", "Replay", "Flyover",
    ]
    if categories != expected or len(categories) != len(set(categories)):
        raise ValidationError("Shipping audio category order or uniqueness differs")
    bindings = policy.get("sourceBindings")
    if not isinstance(bindings, list) or [item.get("role") for item in bindings] != [
        "ORIGINAL_AUDIO_CANDIDATE_PACK", "ORIGINAL_AUDIO_IMPORT_RECEIPT"
    ]:
        raise ValidationError("Shipping audio source bindings differ")
    if [item.get("path") for item in bindings] != [
        "Config/DG_Session19OriginalAudioCandidatePack.json",
        "Evidence/Session19/OriginalAudioCandidateImport.json",
    ]:
        raise ValidationError("Shipping audio source binding paths differ")
    if bindings != [
        {
            "role": "ORIGINAL_AUDIO_CANDIDATE_PACK",
            "path": "Config/DG_Session19OriginalAudioCandidatePack.json",
            "bytes": 9875,
            "sha256": "A21C72AB08E13D99CAA93731E3AFE4BDFDBF9E79A0FD4CE924A33189E7AD6B1A",
        },
        {
            "role": "ORIGINAL_AUDIO_IMPORT_RECEIPT",
            "path": "Evidence/Session19/OriginalAudioCandidateImport.json",
            "bytes": 6363,
            "sha256": "3FD33651C3CD0C660800E68548E6634C4A1E2160980778391C23D8FA4CBA55FA",
        },
    ]:
        raise ValidationError("Shipping audio source binding identities differ")
    for item in bindings:
        if (
            set(item) != {"role", "path", "bytes", "sha256"}
            or not isinstance(item.get("bytes"), int)
            or item["bytes"] <= 0
            or not SHA256_RE.fullmatch(str(item.get("sha256", "")))
        ):
            raise ValidationError("Shipping audio source binding shape differs")
    if policy.get("archive") != {
        "leaf": "Windows",
        "pak": "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.pak",
        "utoc": "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.utoc",
        "ucas": "DiscGolfTour/Content/Paks/DiscGolfTour-Windows.ucas",
        "assetRegistry": "DiscGolfTour/AssetRegistry.bin",
        "ioStoreContainerName": "DiscGolfTour-Windows",
        "platform": "Windows",
    }:
        raise ValidationError("Shipping audio archive contract differs")
    if policy.get("proofRequirements") != {
        "categoryCount": 12,
        "exactGeneratedFolderInventory": True,
        "ioStoreExportBundleRequired": True,
        "packagedAssetRegistryClass": "/Script/Engine.SoundWave",
        "extractedHeaderExportAndBulkRequired": True,
        "extractedObjectNameMarkerRequired": True,
        "extractedExportBundleSizeAgreementRequired": True,
        "soundWaveClassProofSource": "PACKAGED_ASSET_REGISTRY_ONLY",
        "sourceImportIdentityMustMatch": True,
    }:
        raise ValidationError("Shipping audio proof requirements differ")
    if policy.get("resultBoundary") != {
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
    }:
        raise ValidationError("Shipping audio result boundary differs")


def source_audit(policy: dict[str, Any]) -> dict[str, Any]:
    loaded: dict[str, Any] = {}
    bindings: list[dict[str, Any]] = []
    for expected in policy["sourceBindings"]:
        relative = canonical_relative(expected["path"])
        path = ROOT / relative
        data = exact_binding(path, expected, expected["role"])
        loaded[expected["role"]] = load_json_bytes(data, expected["role"])
        bindings.append({"role": expected["role"], "path": relative, **identity(path, data)})
    manifest = loaded["ORIGINAL_AUDIO_CANDIDATE_PACK"]
    receipt = loaded["ORIGINAL_AUDIO_IMPORT_RECEIPT"]
    categories = expected_categories(policy)
    if (
        not isinstance(manifest, dict)
        or manifest.get("schema") != "DiscGolfTour.Session19OriginalAudioCandidatePack.v1"
        or manifest.get("categoryCount") != 12
        or [item.get("category") for item in manifest.get("categories", [])] != categories
    ):
        raise ValidationError("original audio source manifest differs")
    if (
        not isinstance(receipt, dict)
        or receipt.get("schema")
        != "DiscGolfTour.Session19OriginalAudioCandidateImportReceipt.v1"
        or receipt.get("state")
        != "PASS_ORIGINAL_AUDIO_CANDIDATE_IMPORTED_PENDING_BUILD_COOK_AND_HUMAN_REVIEW"
        or receipt.get("assetCount") != 12
        or receipt.get("cookRoot") != policy["cookRoot"]
        or [item.get("category") for item in receipt.get("assets", [])] != categories
    ):
        raise ValidationError("original audio import receipt differs")
    expected_claims = {
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
    if receipt.get("claimBoundary") != expected_claims:
        raise ValidationError("original audio import receipt overclaims")
    records: list[dict[str, Any]] = []
    for source, imported, category in zip(
        manifest["categories"], receipt["assets"], categories
    ):
        uasset_relative = f"Content/Presentation/Audio/Generated/SW_{category}.uasset"
        object_path = expected_object(category)
        if (
            source.get("category") != category
            or source.get("unrealObject") != object_path
            or imported.get("category") != category
            or imported.get("object") != object_path
            or imported.get("sourceSha256") != source.get("sha256")
            or imported.get("uassetPath") != uasset_relative
            or imported.get("metadataBound") is not True
        ):
            raise ValidationError(f"source/import binding differs: {category}")
        uasset = ROOT / uasset_relative
        data = read_stable(uasset)
        if len(data) != imported.get("uassetBytes") or sha256(data) != imported.get("uassetSha256"):
            raise ValidationError(f"serialized source SoundWave identity differs: {category}")
        records.append({
            "category": category,
            "object": object_path,
            "sourceSha256": source["sha256"],
            "serializedSource": {"path": uasset_relative, **identity(uasset, data)},
        })
    configured = read_stable(ROOT / "Config/DefaultGame.ini").decode("utf-8-sig")
    cook_line = '+DirectoriesToAlwaysCook=(Path="/Game/Presentation/Audio/Generated")'
    if configured.count(cook_line) != 1:
        raise ValidationError("generated audio cook root must be configured exactly once")
    return {"bindings": bindings, "records": records, "cookRootConfigured": True}


def collect_archive(archive: Path, candidate_id: str, leaf: str) -> dict[str, Any]:
    if not archive.is_dir() or archive.is_symlink() or is_reparse(archive):
        raise ValidationError("archive must be a regular non-reparse directory")
    if archive.name != leaf or archive.parent.name != candidate_id:
        raise ValidationError("archive is not the requested candidate's Windows leaf")
    root = archive.resolve(strict=True)
    seen: set[str] = set()
    records: list[dict[str, Any]] = []
    for current, directories, files in os.walk(archive, followlinks=False):
        current_path = Path(current)
        for name in directories:
            child = current_path / name
            if child.is_symlink() or is_reparse(child):
                raise ValidationError("archive contains a reparse directory")
        for name in files:
            child = current_path / name
            relative = canonical_relative(child.relative_to(archive).as_posix())
            folded = relative.casefold()
            if folded in seen:
                raise ValidationError("archive contains a case-insensitive duplicate")
            seen.add(folded)
            resolved = exact_file(child, f"archive entry {relative}")
            if root not in resolved.parents:
                raise ValidationError("archive entry resolves outside archive")
            data = read_stable(child)
            records.append({"path": relative, "bytes": len(data), "sha256": sha256(data)})
    records.sort(key=lambda item: (item["path"].casefold(), item["path"]))
    canonical = "".join(
        f"{item['path']}\t{item['bytes']}\t{item['sha256']}\n" for item in records
    ).encode("utf-8")
    return {
        "fileCount": len(records),
        "bytes": sum(item["bytes"] for item in records),
        "canonicalManifestSha256": sha256(canonical),
        "records": records,
    }


def run_tool(arguments: list[str], context: str, timeout: int = 180) -> str:
    try:
        completed = subprocess.run(
            arguments, capture_output=True, check=False, timeout=timeout
        )
    except subprocess.TimeoutExpired as exc:
        raise ValidationError(f"{context} timed out") from exc
    output = (completed.stdout + b"\n" + completed.stderr).decode(
        "utf-8", errors="replace"
    )
    if completed.returncode != 0:
        raise ValidationError(
            f"{context} failed ({completed.returncode}): {output[-2000:]}"
        )
    return output


def parse_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle, skipinitialspace=True)
        required = {
            "ChunkId", "PackageId", "Filename", "ContainerName", "Size",
            "CompressedSize", "Hash", "ChunkType", "Platform",
        }
        if reader.fieldnames is None or not required.issubset(set(reader.fieldnames)):
            raise ValidationError("IoStore CSV header differs")
        return [dict(row) for row in reader]


def validate_iostore_rows(
    policy: dict[str, Any], rows: list[dict[str, str]]
) -> tuple[dict[str, dict[str, Any]], dict[str, Any]]:
    categories = expected_categories(policy)
    expected_names = {category: expected_container_filename(category) for category in categories}
    folder = "../../../DiscGolfTour/Content/Presentation/Audio/Generated/"
    generated_uassets = [
        row for row in rows
        if str(row.get("Filename", "")).startswith(folder)
        and str(row.get("Filename", "")).endswith(".uasset")
    ]
    observed = [str(row.get("Filename", "")) for row in generated_uassets]
    if sorted(observed) != sorted(expected_names.values()) or len(observed) != len(set(observed)):
        raise ValidationError("IoStore generated SoundWave package inventory differs")
    output: dict[str, dict[str, Any]] = {}
    for category, filename in expected_names.items():
        matches = [row for row in generated_uassets if row.get("Filename") == filename]
        if len(matches) != 1:
            raise ValidationError(f"IoStore package identity count differs: {category}")
        row = matches[0]
        try:
            size = int(row.get("Size", ""))
            compressed = int(row.get("CompressedSize", ""))
        except ValueError as exc:
            raise ValidationError(f"IoStore numeric field differs: {category}") from exc
        if (
            row.get("ChunkType") != "ExportBundleData"
            or row.get("ContainerName") != policy["archive"]["ioStoreContainerName"]
            or row.get("Platform") != policy["archive"]["platform"]
            or not row.get("ChunkId")
            or not row.get("PackageId")
            or size <= 0
            or compressed <= 0
            or compressed > size
            or not IOSTORE_HASH_RE.fullmatch(str(row.get("Hash", "")))
        ):
            raise ValidationError(f"IoStore export-bundle row differs: {category}")
        output[category] = {
            "filename": filename,
            "chunkId": row["ChunkId"],
            "packageId": row["PackageId"],
            "size": size,
            "compressedSize": compressed,
            "ioHash": row["Hash"].lower(),
            "chunkType": row["ChunkType"],
        }
    canonical_rows = sorted(
        "\t".join(str(row.get(key, "")) for key in (
            "ChunkId", "PackageId", "Filename", "ContainerName", "Size",
            "CompressedSize", "Hash", "ChunkType", "Platform",
        )) + "\n"
        for row in rows
    )
    return output, {
        "rowCount": len(rows),
        "generatedSoundWaveExportBundleCount": len(output),
        "canonicalInventorySha256": sha256("".join(canonical_rows).encode("utf-8")),
    }


def decode_dump_page(path: Path) -> str:
    data = read_stable(path)
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        return data.decode("utf-16")
    return data.decode("utf-8-sig")


def class_objects(registry_text: str, class_path: str) -> tuple[int, list[str]]:
    lines = registry_text.splitlines()
    header_re = re.compile(r"^\t" + re.escape(class_path) + r" : (\d+) item\(s\)$")
    locations = [index for index, line in enumerate(lines) if header_re.fullmatch(line)]
    if len(locations) != 1:
        raise ValidationError(f"packaged AssetRegistry class section differs: {class_path}")
    index = locations[0]
    declared = int(header_re.fullmatch(lines[index]).group(1))  # type: ignore[union-attr]
    objects: list[str] = []
    for line in lines[index + 1:]:
        if line.startswith("\t\t"):
            objects.append(line[2:])
            continue
        if line.startswith("\t"):
            break
        if line.startswith("--- "):
            break
    if declared != len(objects) or len(objects) != len(set(objects)):
        raise ValidationError(f"packaged AssetRegistry class count differs: {class_path}")
    return declared, objects


def validate_registry(
    policy: dict[str, Any], registry_text: str
) -> tuple[dict[str, bool], dict[str, Any]]:
    class_path = policy["proofRequirements"]["packagedAssetRegistryClass"]
    declared, objects = class_objects(registry_text, class_path)
    expected = {category: expected_object(category) for category in expected_categories(policy)}
    prefix = policy["cookRoot"] + "/"
    generated = [item for item in objects if item.startswith(prefix)]
    if sorted(generated) != sorted(expected.values()):
        raise ValidationError("packaged AssetRegistry generated SoundWave inventory differs")
    result: dict[str, bool] = {}
    for category, object_path in expected.items():
        if objects.count(object_path) != 1:
            raise ValidationError(f"packaged SoundWave object differs: {category}")
        result[category] = True
    return result, {
        "soundWaveClassObjectCount": declared,
        "generatedSoundWaveObjectCount": len(result),
        "dumpCanonicalSha256": sha256(registry_text.encode("utf-8")),
    }


def validate_extracted(
    policy: dict[str, Any], extracted_root: Path,
    iostore: dict[str, dict[str, Any]],
) -> dict[str, dict[str, Any]]:
    base = extracted_root / "DiscGolfTour/Content/Presentation/Audio/Generated"
    if not base.is_dir():
        raise ValidationError("extracted generated-audio directory is missing")
    files = sorted(path for path in base.rglob("*") if path.is_file())
    categories = expected_categories(policy)
    expected_stems = {f"SW_{category}": category for category in categories}
    allowed_suffixes = {".uheader", ".uexp", ".ubulk", ".uptnl", ".m.ubulk"}
    classified: dict[str, list[Path]] = {category: [] for category in categories}
    for path in files:
        if path.parent != base:
            raise ValidationError("cooked SoundWave extraction contains an unexpected subdirectory")
        matched = False
        for stem, category in expected_stems.items():
            if path.name.startswith(stem + "."):
                suffix = path.name[len(stem):]
                if suffix not in allowed_suffixes:
                    raise ValidationError(f"unexpected cooked SoundWave part: {path.name}")
                classified[category].append(path)
                matched = True
                break
        if not matched:
            raise ValidationError(f"unexpected generated-audio cooked file: {path.name}")
    output: dict[str, dict[str, Any]] = {}
    for category in categories:
        parts = classified[category]
        by_suffix = {path.name[len(f"SW_{category}"):]: path for path in parts}
        if (
            len(by_suffix) != len(parts)
            or ".uheader" not in by_suffix
            or ".uexp" not in by_suffix
            or ".ubulk" not in by_suffix
        ):
            raise ValidationError(f"required cooked SoundWave parts differ: {category}")
        export_parts = [by_suffix[".uheader"], by_suffix[".uexp"]]
        export_size = sum(path.stat().st_size for path in export_parts)
        if export_size != iostore[category]["size"]:
            raise ValidationError(f"extracted/export-bundle size differs: {category}")
        combined = b"".join(read_stable(path) for path in export_parts)
        if f"SW_{category}".encode("ascii") not in combined:
            raise ValidationError(f"cooked object-name marker missing: {category}")
        part_records = []
        for path in parts:
            data = read_stable(path)
            if not data:
                raise ValidationError(f"empty cooked SoundWave part: {path.name}")
            part_records.append({"name": path.name, **identity(path, data)})
        output[category] = {
            "exportBundleBytes": export_size,
            "bulkPayloadBytes": by_suffix[".ubulk"].stat().st_size,
            "partCount": len(parts),
            "parts": part_records,
            "objectNameMarkerPresent": True,
            "bulkPayloadPresent": True,
        }
    return output


def write_bytes_exclusive(path: Path, rendered: bytes) -> None:
    with path.open("xb") as handle:
        handle.write(rendered)
        handle.flush()
        os.fsync(handle.fileno())


def write_receipt(path: Path, value: dict[str, Any], candidate_id: str, policy: dict[str, Any]) -> None:
    expected_relative = policy["receiptPattern"].replace("{candidateId}", candidate_id)
    expected = (ROOT / expected_relative).resolve()
    if path.resolve() != expected:
        raise ValidationError(f"--output must be {expected_relative}")
    path.parent.mkdir(parents=True, exist_ok=True)
    rendered = (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    try:
        write_bytes_exclusive(path, rendered)
    except FileExistsError as exc:
        raise ValidationError(
            "Shipping audio receipt already exists; refusing overwrite"
        ) from exc


def audit(
    candidate_id: str, archive: Path, unrealpak: Path, editor: Path,
    output: Path,
) -> dict[str, Any]:
    policy_data = read_stable(POLICY_PATH)
    policy = load_json_bytes(policy_data, str(POLICY_PATH))
    validate_policy(policy)
    source = source_audit(policy)
    archive_facts = collect_archive(archive, candidate_id, policy["archive"]["leaf"])
    archive_by_path = {item["path"]: item for item in archive_facts.pop("records")}
    unrealpak = exact_file(unrealpak, "UnrealPak")
    editor = exact_file(editor, "UnrealEditor-Cmd")
    project = exact_file(ROOT / "DiscGolfTour.uproject", "project descriptor")
    required_containers: dict[str, Path] = {}
    for role in ("pak", "utoc", "ucas"):
        relative = policy["archive"][role]
        path = archive / PurePosixPath(relative)
        exact_file(path, f"Shipping {role}")
        if relative not in archive_by_path:
            raise ValidationError(f"archive manifest lacks required {role}")
        required_containers[role] = path

    scratch_parent = Path("C:\\") if os.name == "nt" else None
    scratch = Path(tempfile.mkdtemp(prefix="dgaudiocook_", dir=scratch_parent))
    try:
        inventory_csv = scratch / "container.csv"
        run_tool([
            str(unrealpak), f"-ListContainer={required_containers['utoc']}",
            f"-CSV={inventory_csv}",
        ], "IoStore inventory")
        if not inventory_csv.is_file():
            raise ValidationError("UnrealPak emitted no IoStore CSV")
        rows = parse_csv(inventory_csv)
        iostore, iostore_summary = validate_iostore_rows(policy, rows)

        extracted_root = scratch / "cooked"
        run_tool([
            str(unrealpak), str(required_containers["utoc"]), "-Extract",
            str(extracted_root),
            "-Filter=*DiscGolfTour/Content/Presentation/Audio/Generated/*",
        ], "generated SoundWave extraction")
        extracted = validate_extracted(policy, extracted_root, iostore)

        registry_root = scratch / "registry"
        run_tool([
            str(unrealpak), str(required_containers["pak"]), "-Extract",
            str(registry_root), "-Filter=*AssetRegistry.bin",
        ], "packaged AssetRegistry extraction")
        registry = registry_root / policy["archive"]["assetRegistry"]
        exact_file(registry, "packaged AssetRegistry")
        registry_data = read_stable(registry)
        dump_root = scratch / "dump"
        dump_log = scratch / "DumpAssetRegistry.log"
        run_tool([
            str(editor), str(project), "-run=DumpAssetRegistry",
            f"-Path={registry}", f"-OutDir={dump_root}", "-All",
            "-unattended", "-nop4", "-nosplash", "-nullrhi", "-NoSound",
            f"-abslog={dump_log}",
        ], "packaged AssetRegistry dump")
        pages = sorted(dump_root.glob("Page_*.txt"))
        if not pages:
            raise ValidationError("DumpAssetRegistry emitted no pages")
        registry_text = "\n".join(decode_dump_page(page) for page in pages)
        registry_presence, registry_summary = validate_registry(policy, registry_text)

        category_records = []
        source_by_category = {item["category"]: item for item in source["records"]}
        for category in expected_categories(policy):
            category_records.append({
                **source_by_category[category],
                "ioStore": iostore[category],
                "packagedAssetRegistrySoundWave": registry_presence[category],
                "cookedObject": extracted[category],
                "shippingCookProven": True,
                "audibleRuntimePlaybackProven": False,
            })
        receipt = {
            "schema": "DiscGolfTour.Session19ShippingAudioCookReceipt.v1",
            "schemaVersion": 1,
            "session": 19,
            "candidateId": candidate_id,
            "generatedUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
            "state": STATE,
            "policy": {
                "path": "Config/DG_Session19ShippingAudioPolicy.json",
                **identity(POLICY_PATH, policy_data),
            },
            "source": source,
            "archive": {
                "hostPathRecorded": False,
                "recoveryLocationToken": f"DGTOUR_PACKAGES/{candidate_id}/Windows",
                **archive_facts,
            },
            "tools": {
                "unrealPak": identity(unrealpak),
                "unrealEditorCmd": identity(editor),
            },
            "containers": {
                role: {"path": policy["archive"][role], **archive_by_path[policy["archive"][role]]}
                for role in ("pak", "utoc", "ucas")
            },
            "ioStore": iostore_summary,
            "packagedAssetRegistry": {
                "path": policy["archive"]["assetRegistry"],
                **identity(registry, registry_data),
                **registry_summary,
            },
            "categoryCount": len(category_records),
            "categories": category_records,
            "claimBoundary": copy.deepcopy(policy["resultBoundary"]),
        }
        write_receipt(output, receipt, candidate_id, policy)
        return receipt
    finally:
        resolved = scratch.resolve()
        if resolved.parent == Path("C:\\") and resolved.name.startswith("dgaudiocook_"):
            shutil.rmtree(resolved, ignore_errors=True)
        elif os.name != "nt" and resolved.name.startswith("dgaudiocook_"):
            shutil.rmtree(resolved, ignore_errors=True)


def fixture_rows(policy: dict[str, Any], size: int) -> list[dict[str, str]]:
    return [{
        "ChunkId": f"{index:024x}",
        "PackageId": f"0x{index:016X}",
        "Filename": expected_container_filename(category),
        "ContainerName": policy["archive"]["ioStoreContainerName"],
        "Size": str(size),
        "CompressedSize": str(max(1, size - 1)),
        "Hash": "0x" + f"{index:040x}",
        "ChunkType": "ExportBundleData",
        "Platform": policy["archive"]["platform"],
    } for index, category in enumerate(expected_categories(policy), 1)]


def fixture_registry(policy: dict[str, Any], extra: str | None = None) -> str:
    objects = [expected_object(category) for category in expected_categories(policy)]
    if extra is not None:
        objects.append(extra)
    lines = [
        "--- Begin CachedAssetsByClass ---",
        f"\t/Script/Engine.SoundWave : {len(objects)} item(s)",
        *(f"\t\t{item}" for item in objects),
        "\t/Script/Engine.Texture2D : 0 item(s)",
    ]
    return "\n".join(lines) + "\n"


def make_extracted(policy: dict[str, Any], root: Path) -> tuple[list[dict[str, str]], int]:
    base = root / "DiscGolfTour/Content/Presentation/Audio/Generated"
    base.mkdir(parents=True)
    size = 0
    for category in expected_categories(policy):
        header = f"SW_{category}".encode("ascii").ljust(96, b"_")
        export = b"X"
        bulk = b"procedural-audio-payload"
        (base / f"SW_{category}.uheader").write_bytes(header)
        (base / f"SW_{category}.uexp").write_bytes(export)
        (base / f"SW_{category}.ubulk").write_bytes(bulk)
        size = len(header) + len(export)
    return fixture_rows(policy, size), size


def self_test() -> tuple[int, list[str]]:
    policy = load_json_bytes(read_stable(POLICY_PATH), str(POLICY_PATH))
    validate_policy(policy)
    source_audit(policy)
    caught = 0
    failures: list[str] = []

    policy_mutations: list[tuple[str, Callable[[dict[str, Any]], None]]] = [
        ("schema", lambda v: v.__setitem__("schema", "bad")),
        ("version", lambda v: v.__setitem__("schemaVersion", 2)),
        ("session", lambda v: v.__setitem__("session", 18)),
        ("state", lambda v: v.__setitem__("state", "PASS")),
        ("receipt", lambda v: v.__setitem__("receiptPattern", "bad")),
        ("self-test minimum", lambda v: v.__setitem__("selfTestMinimumMutationCount", 44)),
        ("category removal", lambda v: v["orderedCategories"].pop()),
        ("category duplicate", lambda v: v["orderedCategories"].__setitem__(1, v["orderedCategories"][0])),
        ("cook root", lambda v: v.__setitem__("cookRoot", "/Game")),
        ("class", lambda v: v["proofRequirements"].__setitem__("packagedAssetRegistryClass", "bad")),
        ("source hash", lambda v: v["sourceBindings"][0].__setitem__("sha256", "0" * 64)),
        ("shipping cook", lambda v: v["resultBoundary"].__setitem__("shippingCookProven", False)),
        ("audible overclaim", lambda v: v["resultBoundary"].__setitem__("audibleRuntimePlaybackProven", True)),
        ("human overclaim", lambda v: v["resultBoundary"].__setitem__("humanMixApproved", True)),
        ("quality overclaim", lambda v: v["resultBoundary"].__setitem__("finalQualityApproved", True)),
        ("accessibility overclaim", lambda v: v["resultBoundary"].__setitem__("accessibilityApproved", True)),
        ("legal overclaim", lambda v: v["resultBoundary"].__setitem__("legalApproved", True)),
        ("owner overclaim", lambda v: v["resultBoundary"].__setitem__("productOwnerApproved", True)),
        ("blocker overclaim", lambda v: v["resultBoundary"].__setitem__("blockerClosed", True)),
        ("release overclaim", lambda v: v["resultBoundary"].__setitem__("releaseReady", True)),
    ]
    for name, mutation in policy_mutations:
        value = copy.deepcopy(policy)
        mutation(value)
        try:
            validate_policy(value)
        except ValidationError:
            caught += 1
        else:
            failures.append(f"policy mutation escaped: {name}")

    with tempfile.TemporaryDirectory(prefix="dgaudio_selftest_") as temp_name:
        root = Path(temp_name)
        rows, _ = make_extracted(policy, root)
        iostore, _ = validate_iostore_rows(policy, rows)
        validate_extracted(policy, root, iostore)
        validate_registry(policy, fixture_registry(policy))

        row_mutations: list[tuple[str, Callable[[list[dict[str, str]]], None]]] = [
            ("row removal", lambda v: v.pop()),
            ("row duplicate", lambda v: v.append(copy.deepcopy(v[0]))),
            ("row filename case", lambda v: v[0].__setitem__("Filename", v[0]["Filename"].lower())),
            ("row chunk type", lambda v: v[0].__setitem__("ChunkType", "BulkData")),
            ("row size", lambda v: v[0].__setitem__("Size", "0")),
            ("row compressed size", lambda v: v[0].__setitem__("CompressedSize", "999999")),
            ("row hash", lambda v: v[0].__setitem__("Hash", "bad")),
            ("row container", lambda v: v[0].__setitem__("ContainerName", "bad")),
            ("row platform", lambda v: v[0].__setitem__("Platform", "Linux")),
            ("row package id", lambda v: v[0].__setitem__("PackageId", "")),
            ("extra generated package", lambda v: v.append({**copy.deepcopy(v[0]), "Filename": "../../../DiscGolfTour/Content/Presentation/Audio/Generated/SW_Extra.uasset"})),
        ]
        for name, mutation in row_mutations:
            value = copy.deepcopy(rows)
            mutation(value)
            try:
                validate_iostore_rows(policy, value)
            except ValidationError:
                caught += 1
            else:
                failures.append(f"IoStore mutation escaped: {name}")

        registry_mutations = [
            ("registry missing object", fixture_registry(policy).replace(expected_object(expected_categories(policy)[0]) + "\n", "", 1)),
            ("registry wrong class", fixture_registry(policy).replace("/Script/Engine.SoundWave", "/Script/Engine.SoundBase", 1)),
            ("registry absent class", "--- Begin CachedAssetsByClass ---\n\t/Script/Engine.Texture2D : 0 item(s)\n"),
            ("registry extra generated object", fixture_registry(policy, "/Game/Presentation/Audio/Generated/SW_Extra.SW_Extra")),
            ("registry duplicate object", fixture_registry(policy).replace(
                "\t\t" + expected_object(expected_categories(policy)[0]) + "\n",
                ("\t\t" + expected_object(expected_categories(policy)[0]) + "\n") * 2,
                1,
            )),
            ("registry declared count", fixture_registry(policy).replace("SoundWave : 12", "SoundWave : 11", 1)),
        ]
        for name, text in registry_mutations:
            try:
                validate_registry(policy, text)
            except ValidationError:
                caught += 1
            else:
                failures.append(f"AssetRegistry mutation escaped: {name}")

    first_category = expected_categories(policy)[0]
    extracted_mutations: list[tuple[str, Callable[[Path, list[dict[str, str]]], None]]] = [
        ("missing cooked header", lambda root, rows: (
            root / "DiscGolfTour/Content/Presentation/Audio/Generated"
            / f"SW_{first_category}.uheader"
        ).unlink()),
        ("missing cooked export", lambda root, rows: (
            root / "DiscGolfTour/Content/Presentation/Audio/Generated"
            / f"SW_{first_category}.uexp"
        ).unlink()),
        ("missing cooked bulk", lambda root, rows: (
            root / "DiscGolfTour/Content/Presentation/Audio/Generated"
            / f"SW_{first_category}.ubulk"
        ).unlink()),
        ("empty cooked bulk", lambda root, rows: (
            root / "DiscGolfTour/Content/Presentation/Audio/Generated"
            / f"SW_{first_category}.ubulk"
        ).write_bytes(b"")),
        ("missing object-name marker", lambda root, rows: (
            root / "DiscGolfTour/Content/Presentation/Audio/Generated"
            / f"SW_{first_category}.uheader"
        ).write_bytes(b"NotTheObject".ljust(96, b"_"))),
        ("export size mismatch", lambda root, rows: rows[0].__setitem__(
            "Size", str(int(rows[0]["Size"]) + 1)
        )),
        ("extra cooked object", lambda root, rows: (
            root / "DiscGolfTour/Content/Presentation/Audio/Generated/SW_Extra.uheader"
        ).write_bytes(b"SW_Extra SoundWave")),
        ("unexpected cooked suffix", lambda root, rows: (
            root / "DiscGolfTour/Content/Presentation/Audio/Generated"
            / f"SW_{first_category}.invalid"
        ).write_bytes(b"bad")),
    ]
    for name, mutation in extracted_mutations:
        with tempfile.TemporaryDirectory(prefix="dgaudio_extract_selftest_") as temp_name:
            root = Path(temp_name)
            rows, _ = make_extracted(policy, root)
            mutation(root, rows)
            iostore = {
                category: value
                for category, value in validate_iostore_rows(policy, rows)[0].items()
            }
            try:
                validate_extracted(policy, root, iostore)
            except ValidationError:
                caught += 1
            else:
                failures.append(f"cooked-object mutation escaped: {name}")

    with tempfile.TemporaryDirectory(prefix="dgaudio_output_selftest_") as temp_name:
        collision = Path(temp_name) / "receipt.json"
        collision.write_bytes(b"sentinel")
        try:
            write_bytes_exclusive(collision, b"must-not-overwrite")
        except FileExistsError:
            caught += 1
            if collision.read_bytes() != b"sentinel":
                failures.append("exclusive output collision changed existing bytes")
        else:
            failures.append("exclusive output collision was accepted")

    expected_count = (
        len(policy_mutations) + len(row_mutations) + len(registry_mutations)
        + len(extracted_mutations) + 1
    )
    if caught != expected_count:
        failures.append(f"caught {caught} of {expected_count}")
    if expected_count < policy["selfTestMinimumMutationCount"]:
        failures.append("self-test mutation inventory below policy minimum")
    return caught, failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-id")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--unrealpak", type=Path)
    parser.add_argument("--unreal-editor-cmd", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            if any((args.candidate_id, args.archive, args.unrealpak, args.unreal_editor_cmd, args.output)):
                parser.error("--self-test does not accept live arguments")
            caught, failures = self_test()
            if failures:
                print("SESSION 19 SHIPPING AUDIO SELF-TEST FAILED", file=sys.stderr)
                for failure in failures:
                    print(" - " + failure, file=sys.stderr)
                return 1
            print(f"SESSION 19 SHIPPING AUDIO SELF-TEST PASS: {caught}/{caught}")
            return 0
        if any(value is None for value in (
            args.candidate_id, args.archive, args.unrealpak,
            args.unreal_editor_cmd, args.output,
        )):
            parser.error(
                "live validation requires --candidate-id --archive --unrealpak "
                "--unreal-editor-cmd --output"
            )
        if not RUN_ID_RE.fullmatch(args.candidate_id):
            parser.error("--candidate-id is not a sanitized Session 19 Shipping identifier")
        receipt = audit(
            args.candidate_id, args.archive, args.unrealpak,
            args.unreal_editor_cmd, args.output,
        )
        print(receipt["state"])
        print(f"Receipt: {args.output.resolve()}")
        print("Shipping cook proven: yes (12/12 generated SoundWave packages)")
        print("Audible playback and human approvals proven: no")
        return 0
    except (OSError, UnicodeError, ValidationError, subprocess.SubprocessError) as exc:
        print(f"SESSION 19 SHIPPING AUDIO FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
