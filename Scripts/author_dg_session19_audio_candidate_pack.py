"""Guarded Unreal Editor authoring for the project-owned Session 19 WAV candidate pack.

Run only after generating and verifying the source masters. Example:
  DG_AUDIO_CANDIDATE_AUTHORING=1 UnrealEditor-Cmd.exe DiscGolfTour.uproject \
    -run=pythonscript -script=Scripts/author_dg_session19_audio_candidate_pack.py --apply
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re

import unreal


PROJECT_ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
MANIFEST_PATH = PROJECT_ROOT / "Config/DG_Session19OriginalAudioCandidatePack.json"
RECEIPT_PATH = PROJECT_ROOT / "Evidence/Session19/OriginalAudioCandidateImport.json"
DESTINATION = "/Game/Presentation/Audio/Generated"


def fail(message: str) -> None:
    raise RuntimeError(message)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def main() -> None:
    command_line = unreal.SystemLibrary.get_command_line()
    apply_requested = re.search(r"(?i)(?:^|\s)--apply(?:$|\s)", command_line) is not None
    allow_existing = re.search(r"(?i)(?:^|\s)--allow-existing(?:$|\s)", command_line) is not None
    if not apply_requested or os.environ.get("DG_AUDIO_CANDIDATE_AUTHORING") != "1":
        fail("authoring requires --apply and DG_AUDIO_CANDIDATE_AUTHORING=1")
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    if manifest.get("schema") != "DiscGolfTour.Session19OriginalAudioCandidatePack.v1":
        fail("audio candidate manifest schema differs")
    categories = manifest.get("categories")
    if not isinstance(categories, list) or len(categories) != 12:
        fail("audio candidate manifest must contain exactly twelve categories")

    imported: list[dict[str, object]] = []
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    for record in categories:
        source = PROJECT_ROOT / record["sourcePath"]
        if (not source.is_file() or source.stat().st_size != record["bytes"]
                or sha256(source) != record["sha256"]):
            fail(f"source WAV identity differs: {record['category']}")
        package = record["unrealPackage"]
        object_path = record["unrealObject"]
        exists = unreal.EditorAssetLibrary.does_asset_exist(package)
        if exists and not allow_existing:
            fail(f"target already exists; refusing replacement: {package}")
        if not exists:
            task = unreal.AssetImportTask()
            task.filename = str(source)
            task.destination_path = DESTINATION
            task.destination_name = f"SW_{record['category']}"
            task.automated = True
            task.replace_existing = False
            task.replace_existing_settings = False
            task.save = True
            asset_tools.import_asset_tasks([task])
            if not task.imported_object_paths:
                fail(f"Unreal did not import: {record['category']}")

        asset = unreal.EditorAssetLibrary.load_asset(object_path)
        if asset is None or not isinstance(asset, unreal.SoundWave):
            fail(f"imported object is not a SoundWave: {object_path}")
        unreal.EditorAssetLibrary.set_metadata_tag(asset, "DG.AudioCategory", record["category"])
        unreal.EditorAssetLibrary.set_metadata_tag(asset, "DG.SourceSha256", record["sha256"])
        unreal.EditorAssetLibrary.set_metadata_tag(
            asset, "DG.Provenance", "PROJECT_OWNED_ORIGINAL_PROCEDURAL_SYNTHESIS")
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

        relative_uasset = "Content/Presentation/Audio/Generated/SW_{}.uasset".format(
            record["category"])
        disk_path = PROJECT_ROOT / relative_uasset
        if not disk_path.is_file():
            fail(f"saved uasset is missing: {relative_uasset}")
        imported.append({
            "category": record["category"],
            "object": object_path,
            "sourceSha256": record["sha256"],
            "uassetPath": relative_uasset,
            "uassetBytes": disk_path.stat().st_size,
            "uassetSha256": sha256(disk_path),
            "metadataBound": True,
        })

    RECEIPT_PATH.parent.mkdir(parents=True, exist_ok=True)
    receipt = {
        "schema": "DiscGolfTour.Session19OriginalAudioCandidateImportReceipt.v1",
        "schemaVersion": 1,
        "state": "PASS_ORIGINAL_AUDIO_CANDIDATE_IMPORTED_PENDING_BUILD_COOK_AND_HUMAN_REVIEW",
        "manifest": {
            "path": "Config/DG_Session19OriginalAudioCandidatePack.json",
            "bytes": MANIFEST_PATH.stat().st_size,
            "sha256": sha256(MANIFEST_PATH),
        },
        "assetCount": len(imported),
        "assets": imported,
        "cookRoot": DESTINATION,
        "claimBoundary": {
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
        },
    }
    RECEIPT_PATH.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    unreal.log(f"PASS_ORIGINAL_AUDIO_CANDIDATE_IMPORT ({len(imported)} SoundWave assets)")


main()
