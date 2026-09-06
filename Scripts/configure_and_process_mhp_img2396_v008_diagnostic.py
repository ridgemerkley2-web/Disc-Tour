"""Create, configure, and process the authentic IMG_2396 mono capture.

This is an append-only diagnostic solve. It intentionally does not promote the
result into the active animation set or assert any rights/release approval.
"""

from __future__ import annotations

import datetime as _datetime
import json as _json
import os as _os
import traceback as _traceback
import uuid as _uuid

import unreal


CAPTURE_FOLDER = "/Game/CaptureManager/Imports/Mono_Video_Ingest/2-IMG_2396_1"
CAPTURE_PATH = f"{CAPTURE_FOLDER}/CD_2-IMG_2396_1"
PERFORMANCE_NAME = "MHP_DG_RHBH_IMG2396_v008_DIAGNOSTIC"
PERFORMANCE_PATH = f"{CAPTURE_FOLDER}/{PERFORMANCE_NAME}"
RECEIPT_ROOT = r"C:\DGTour_TestRuns\AnimationFluidity"


def _enum_text(value: object) -> str:
    return getattr(value, "name", None) or str(value)


def _write_receipt(receipt_dir: str, payload: dict[str, object]) -> str:
    _os.makedirs(receipt_dir, exist_ok=True)
    receipt_path = _os.path.join(receipt_dir, "metahuman_mono_process_receipt.json")
    with open(receipt_path, "w", encoding="utf-8") as handle:
        _json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")
    return receipt_path


def _configure_performance() -> unreal.MetaHumanPerformance:
    capture = unreal.load_asset(CAPTURE_PATH)
    if capture is None:
        raise RuntimeError(f"Missing capture asset: {CAPTURE_PATH}")

    performance = unreal.load_asset(PERFORMANCE_PATH)
    if performance is None:
        performance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            PERFORMANCE_NAME,
            CAPTURE_FOLDER,
            unreal.MetaHumanPerformance,
            unreal.MetaHumanPerformanceFactoryNew(),
        )

    if performance is None or not isinstance(performance, unreal.MetaHumanPerformance):
        raise RuntimeError(f"Wrong or missing performance asset: {PERFORMANCE_PATH}")

    performance.modify()
    # Set input mode first so the footage setter runs with the correct mono policy.
    performance.set_editor_property("input_type", unreal.DataInputType.MONO_FOOTAGE)
    performance.set_editor_property("footage_capture_data", capture)
    performance.set_editor_property("face_tracking", False)
    performance.set_editor_property(
        "head_movement_mode", unreal.PerformanceHeadMovementMode.DISABLED
    )
    # UE 5.8's generated setter is not exposed in every Python build. The
    # reflected property remains the supported fallback in those builds.
    set_body_tracking = getattr(performance, "set_body_tracking", None)
    if callable(set_body_tracking):
        set_body_tracking(True)
    else:
        performance.set_editor_property("body_tracking", True)

    expected = {
        "input_type": unreal.DataInputType.MONO_FOOTAGE,
        "footage_capture_data": capture,
        "face_tracking": False,
        "head_movement_mode": unreal.PerformanceHeadMovementMode.DISABLED,
        "body_tracking": True,
    }
    for key, expected_value in expected.items():
        actual_value = performance.get_editor_property(key)
        if actual_value != expected_value:
            raise RuntimeError(
                f"{key}: expected {expected_value!r}, got {actual_value!r}"
            )

    if not unreal.EditorAssetLibrary.save_loaded_asset(performance, False):
        raise RuntimeError(f"Save failed: {PERFORMANCE_PATH}")

    unreal.log(f"CONFIGURED_AND_SAVED {performance.get_path_name()}")
    return performance


def main() -> None:
    run_id = (
        "MetaHumanMonoDiagnostic-v008-"
        f"{_datetime.datetime.now(_datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')}-"
        f"{_uuid.uuid4()}"
    )
    receipt_dir = _os.path.join(RECEIPT_ROOT, run_id)
    started_at = _datetime.datetime.now(_datetime.timezone.utc).isoformat()
    receipt: dict[str, object] = {
        "schema": "DiscGolfTour.MetaHumanMonoDiagnosticReceipt.v1",
        "run_id": run_id,
        "started_at_utc": started_at,
        "capture_asset": CAPTURE_PATH,
        "performance_asset": PERFORMANCE_PATH,
        "settings": {
            "input_type": "MONO_FOOTAGE",
            "body_tracking": True,
            "face_tracking": False,
            "head_movement_mode": "DISABLED",
            "blocking_processing": True,
        },
        "promotion_authorized": False,
        "rights_release_approved": False,
        "status": "started",
    }

    try:
        performance = _configure_performance()
        performance.set_blocking_processing(True)
        unreal.log(f"STARTING_MONO_BODY_PIPELINE {performance.get_path_name()}")
        start_error = performance.start_pipeline()
        receipt["start_pipeline_result"] = _enum_text(start_error)

        if start_error is not unreal.StartPipelineErrorType.NONE:
            receipt["status"] = "blocked_start_pipeline"
            raise RuntimeError(f"MetaHuman pipeline did not start: {start_error!r}")

        receipt["status"] = "pipeline_call_completed"
        if not unreal.EditorAssetLibrary.save_loaded_asset(performance, False):
            raise RuntimeError(f"Post-process save failed: {PERFORMANCE_PATH}")
        unreal.log(f"FINISHED_MONO_BODY_PIPELINE {performance.get_path_name()}")
    except Exception as exc:
        receipt["status"] = "failed"
        receipt["error_type"] = type(exc).__name__
        receipt["error"] = str(exc)
        receipt["traceback"] = _traceback.format_exc()
        receipt["finished_at_utc"] = _datetime.datetime.now(
            _datetime.timezone.utc
        ).isoformat()
        receipt_path = _write_receipt(receipt_dir, receipt)
        unreal.log_error(f"MONO_BODY_PIPELINE_FAILED receipt={receipt_path}: {exc}")
        raise

    receipt["finished_at_utc"] = _datetime.datetime.now(
        _datetime.timezone.utc
    ).isoformat()
    receipt_path = _write_receipt(receipt_dir, receipt)
    unreal.log(f"MONO_BODY_PIPELINE_RECEIPT {receipt_path}")


if __name__ == "__main__":
    main()
