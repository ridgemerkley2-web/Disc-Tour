"""Export the solved IMG_2396 diagnostic performance to an AnimSequence."""

from __future__ import annotations

import datetime as _datetime
import json as _json
import os as _os
import traceback as _traceback
import uuid as _uuid

import unreal


PERFORMANCE_PATH = (
    "/Game/CaptureManager/Imports/Mono_Video_Ingest/2-IMG_2396_1/"
    "MHP_DG_RHBH_IMG2396_v008_DIAGNOSTIC"
)
EXPORT_FOLDER = "/Game/DiscGolf/Animation/Authentic/v008/Diagnostic"
EXPORT_NAME = "AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_Performer"
EXPORT_PATH = f"{EXPORT_FOLDER}/{EXPORT_NAME}"
RECEIPT_ROOT = r"C:\DGTour_TestRuns\AnimationFluidity"


def _safe_call(obj: object, method_name: str) -> object | None:
    method = getattr(obj, method_name, None)
    if not callable(method):
        return None
    try:
        return method()
    except Exception:
        return None


def _write_receipt(receipt_dir: str, payload: dict[str, object]) -> str:
    _os.makedirs(receipt_dir, exist_ok=True)
    receipt_path = _os.path.join(receipt_dir, "metahuman_anim_export_receipt.json")
    with open(receipt_path, "w", encoding="utf-8") as handle:
        _json.dump(payload, handle, indent=2, sort_keys=True, default=str)
        handle.write("\n")
    return receipt_path


def main() -> None:
    run_id = (
        "MetaHumanAnimExport-v008-"
        f"{_datetime.datetime.now(_datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')}-"
        f"{_uuid.uuid4()}"
    )
    receipt_dir = _os.path.join(RECEIPT_ROOT, run_id)
    receipt: dict[str, object] = {
        "schema": "DiscGolfTour.MetaHumanAnimExportReceipt.v1",
        "run_id": run_id,
        "started_at_utc": _datetime.datetime.now(
            _datetime.timezone.utc
        ).isoformat(),
        "performance_asset": PERFORMANCE_PATH,
        "animation_asset": EXPORT_PATH,
        "settings": {
            "show_export_dialog": False,
            "auto_save_anim_sequence": True,
            "export_range": "WHOLE_SEQUENCE",
            "export_face": False,
            "export_body": True,
            "export_skeleton": "PERFORMER_SKELETON",
            "enable_head_movement": False,
            "body_unsolved_behavior": "LAST_VALID_FRAME",
        },
        "promotion_authorized": False,
        "rights_release_approved": False,
        "status": "started",
    }

    try:
        performance = unreal.load_asset(PERFORMANCE_PATH)
        if performance is None or not isinstance(
            performance, unreal.MetaHumanPerformance
        ):
            raise RuntimeError(f"Missing solved performance: {PERFORMANCE_PATH}")

        if unreal.EditorAssetLibrary.does_asset_exist(EXPORT_PATH):
            raise RuntimeError(f"Append-only export target already exists: {EXPORT_PATH}")

        settings = (
            unreal.MetaHumanPerformanceExportUtils.get_export_animation_sequence_settings(
                performance
            )
        )
        if settings is None:
            raise RuntimeError("Could not create body-aware export settings")

        settings.show_export_dialog = False
        settings.auto_save_anim_sequence = True
        settings.package_path = EXPORT_FOLDER
        settings.asset_name = EXPORT_NAME
        settings.enable_head_movement = False
        settings.export_range = unreal.PerformanceExportRange.WHOLE_SEQUENCE
        settings.export_face = False
        settings.export_body = True
        settings.export_skeleton = (
            unreal.PerformanceExportSkeleton.PERFORMER_SKELETON
        )
        settings.body_unsolved_behavior = (
            unreal.BodyUnsolvedFrameBehavior.LAST_VALID_FRAME
        )

        animation = unreal.MetaHumanPerformanceExportUtils.export_animation_sequence(
            performance, settings
        )
        if animation is None or not isinstance(animation, unreal.AnimSequence):
            raise RuntimeError("MetaHuman animation export returned no AnimSequence")

        if not unreal.EditorAssetLibrary.save_loaded_asset(animation, False):
            raise RuntimeError(f"Failed to save exported animation: {EXPORT_PATH}")

        skeleton = _safe_call(animation, "get_skeleton")
        receipt["animation_object_path"] = animation.get_path_name()
        receipt["skeleton_object_path"] = (
            skeleton.get_path_name() if skeleton is not None else None
        )
        receipt["play_length_seconds"] = _safe_call(animation, "get_play_length")
        receipt["sampled_keys"] = _safe_call(
            animation, "get_number_of_sampled_keys"
        )
        receipt["status"] = "export_completed"
        unreal.log(f"EXPORTED_DIAGNOSTIC_ANIMATION {animation.get_path_name()}")
    except Exception as exc:
        receipt["status"] = "failed"
        receipt["error_type"] = type(exc).__name__
        receipt["error"] = str(exc)
        receipt["traceback"] = _traceback.format_exc()
        receipt["finished_at_utc"] = _datetime.datetime.now(
            _datetime.timezone.utc
        ).isoformat()
        receipt_path = _write_receipt(receipt_dir, receipt)
        unreal.log_error(f"DIAGNOSTIC_ANIMATION_EXPORT_FAILED receipt={receipt_path}: {exc}")
        raise

    receipt["finished_at_utc"] = _datetime.datetime.now(
        _datetime.timezone.utc
    ).isoformat()
    receipt_path = _write_receipt(receipt_dir, receipt)
    unreal.log(f"DIAGNOSTIC_ANIMATION_EXPORT_RECEIPT {receipt_path}")


if __name__ == "__main__":
    main()
