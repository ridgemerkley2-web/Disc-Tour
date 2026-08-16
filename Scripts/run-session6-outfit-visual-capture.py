#!/usr/bin/env python3
"""Launch and independently verify the 12-frame Session 6 visual pass."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import struct
import sys
import time
import uuid
import zlib


ROOT = Path(__file__).absolute().parents[1]
DEFAULT_ENGINE = Path(
    r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
)
PROJECT = ROOT / "DiscGolfTour.uproject"
LOG = ROOT / "Saved/Logs/CharacterFramework_Session6_OutfitVisualCapture.log"
MANIFEST = ROOT / (
    "Saved/CharacterFramework/Screenshots/Session6_OutfitCustomization/"
    "Session6_Outfit_CaptureManifest.json"
)
PASS = "DG_SESSION6_OUTFIT_VISUAL_CAPTURE: PASS"
FAIL = "DG_SESSION6_OUTFIT_VISUAL_CAPTURE: FAIL"
FILENAMES = (
    "01_Outfit_Creator_Tab.png",
    "02_Top_Choices_And_Variants.png",
    "03_Outerwear.png",
    "04_Bottoms_And_Shoes.png",
    "05_Hats_And_Eyewear.png",
    "06_Gloves_And_Wrist.png",
    "07_Disc_Bag.png",
    "08_ShortCompact_Full_Outfit.png",
    "09_Baseline_Full_Outfit.png",
    "10_TallLongArms_Full_Outfit.png",
    "11_Outfitted_Release_Frame.png",
    "12_Outfitted_FollowThrough.png",
)
EXPECTED_EQUIPPED_COUNTS = (11, 1, 2, 3, 2, 2, 2, 11, 11, 11, 11, 11)
EXPECTED_SKELETAL_COUNTS = (6, 1, 2, 3, 0, 1, 0, 6, 6, 6, 6, 6)
MATERIAL_FALLBACK_LOG_TOKENS = (
    "missing usage flag",
    "Default Material will be used in game",
)


def file_sha256_or_absent(path: Path) -> str | None:
    if not path.is_file():
        return None
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def paeth(left: int, above: int, upper_left: int) -> int:
    prediction = left + above - upper_left
    candidates = (
        (abs(prediction - left), left),
        (abs(prediction - above), above),
        (abs(prediction - upper_left), upper_left),
    )
    return min(candidates, key=lambda item: item[0])[1]


def png_pixel_metrics(path: Path) -> dict[str, float | int]:
    encoded = path.read_bytes()
    if not encoded.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError(f"not a PNG: {path}")
    offset = 8
    width = height = bit_depth = color_type = interlace = None
    compressed = bytearray()
    while offset + 12 <= len(encoded):
        size = struct.unpack(">I", encoded[offset : offset + 4])[0]
        kind = encoded[offset + 4 : offset + 8]
        data = encoded[offset + 8 : offset + 8 + size]
        offset += size + 12
        if kind == b"IHDR":
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(
                ">IIBBBBB", data
            )
        elif kind == b"IDAT":
            compressed.extend(data)
        elif kind == b"IEND":
            break
    if (
        width is None or height is None or bit_depth != 8
        or color_type not in (2, 6) or interlace != 0
    ):
        raise ValueError(f"unsupported PNG layout: {path}")
    channels = 3 if color_type == 2 else 4
    stride = width * channels
    decoded = zlib.decompress(bytes(compressed))
    if len(decoded) != (stride + 1) * height:
        raise ValueError(f"unexpected PNG scanline length: {path}")

    previous = bytearray(stride)
    source = 0
    bright = 0
    dark = 0
    luminance_sum = 0
    luminance_square_sum = 0
    sampled = 0
    for _row in range(height):
        filter_type = decoded[source]
        source += 1
        current = bytearray(decoded[source : source + stride])
        source += stride
        for byte_index in range(stride):
            left = current[byte_index - channels] if byte_index >= channels else 0
            above = previous[byte_index]
            upper_left = previous[byte_index - channels] if byte_index >= channels else 0
            if filter_type == 1:
                current[byte_index] = (current[byte_index] + left) & 0xFF
            elif filter_type == 2:
                current[byte_index] = (current[byte_index] + above) & 0xFF
            elif filter_type == 3:
                current[byte_index] = (current[byte_index] + ((left + above) // 2)) & 0xFF
            elif filter_type == 4:
                current[byte_index] = (
                    current[byte_index] + paeth(left, above, upper_left)
                ) & 0xFF
            elif filter_type != 0:
                raise ValueError(f"unsupported PNG filter {filter_type}: {path}")
        # Sample every fourth pixel. This is independent of the runtime body
        # projection gate and catches black/blank/flat screenshots cheaply.
        for pixel in range(0, stride, channels * 4):
            red, green, blue = current[pixel : pixel + 3]
            luminance = (54 * red + 183 * green + 19 * blue) // 256
            luminance_sum += luminance
            luminance_square_sum += luminance * luminance
            sampled += 1
            bright += luminance >= 80
            dark += luminance <= 24
        previous = current
    mean = luminance_sum / max(1, sampled)
    variance = max(0.0, luminance_square_sum / max(1, sampled) - mean * mean)
    return {
        "sampled_pixels": sampled,
        "bright_pixels": bright,
        "dark_pixels": dark,
        "mean_luminance": round(mean, 3),
        "luminance_stddev": round(variance**0.5, 3),
    }


def args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", default=os.environ.get("UE_EDITOR", str(DEFAULT_ENGINE)))
    parser.add_argument("--project", default=str(PROJECT))
    parser.add_argument("--log", default=str(LOG))
    parser.add_argument("--manifest", default=str(MANIFEST))
    parser.add_argument("--timeout-seconds", type=float, default=300.0)
    parser.add_argument("--extra-arg", action="append", default=[])
    return parser.parse_args()


def resolve_engine(value: str) -> Path:
    path = Path(value).expanduser().absolute()
    if path.is_dir():
        for candidate in (
            path / "Engine/Binaries/Win64/UnrealEditor.exe",
            path / "Binaries/Win64/UnrealEditor.exe",
        ):
            if candidate.is_file():
                return candidate
    return path


def terminate(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(
            ["taskkill", "/PID", str(process.pid), "/T", "/F"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False,
        )
    else:
        process.kill()


def main() -> int:
    options = args()
    engine = resolve_engine(options.engine)
    project = Path(options.project).expanduser().absolute()
    log = Path(options.log).expanduser().absolute()
    manifest = Path(options.manifest).expanduser().absolute()
    if not engine.is_file() or not project.is_file():
        raise FileNotFoundError(f"engine/project missing: {engine} / {project}")
    if not 30 <= options.timeout_seconds <= 1800:
        raise ValueError("timeout must be between 30 and 1800 seconds")
    reserved = (
        "-session6outfitvisualcapture", "-session6outfitvalidationnosave",
        "-session6outfitvalidationsaveslot",
        "-resx", "-resy", "-abslog", "-nullrhi",
    )
    if any(arg.lower().startswith(reserved) for arg in options.extra_arg):
        raise ValueError("extra arguments may not override visual harness arguments")
    log.parent.mkdir(parents=True, exist_ok=True)
    log.unlink(missing_ok=True)
    if manifest.parent.is_dir():
        manifest.unlink(missing_ok=True)
        for filename in FILENAMES:
            (manifest.parent / filename).unlink(missing_ok=True)
    validation_slot = (
        "DiscGolfTour_Automation_Session6Outfit_" + uuid.uuid4().hex
    )
    validation_save = project.parent / "Saved/SaveGames" / f"{validation_slot}.sav"
    production_save = project.parent / "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
    production_save_sha256_before = file_sha256_or_absent(production_save)
    validation_save.unlink(missing_ok=True)
    command = [
        str(engine), str(project), "-game", "-Course=PineRidge",
        "-Session6OutfitVisualCapture", "-Session6OutfitValidationNoSave",
        f"-Session6OutfitValidationSaveSlot={validation_slot}",
        "-windowed", "-ResX=1920", "-ResY=1080",
        "-ForceRes", "-unattended", "-nop4", "-nosplash", "-NoSound",
        "-UTF8Output", "-stdout", "-FullStdOutLogOutput", f"-abslog={log}",
        *options.extra_arg,
    ]
    process = subprocess.Popen(
        command, cwd=project.parent, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
        creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0,
    )
    try:
        console, _ = process.communicate(timeout=options.timeout_seconds)
    except subprocess.TimeoutExpired:
        terminate(process)
        console, _ = process.communicate()
        validation_save.unlink(missing_ok=True)
        print("SESSION 6 OUTFIT VISUAL CAPTURE FAIL: timed out", file=sys.stderr)
        return 1
    log_text = log.read_text(encoding="utf-8-sig", errors="replace") if log.is_file() else console
    folded_log_text = log_text.casefold()
    material_fallback_log_clean = {
        token: token.casefold() not in folded_log_text
        for token in MATERIAL_FALLBACK_LOG_TOKENS
    }
    temp_slot_deleted_by_runner = not validation_save.exists()
    production_save_sha256_after = file_sha256_or_absent(production_save)
    production_save_sha256_unchanged = (
        production_save_sha256_before == production_save_sha256_after
    )
    if not temp_slot_deleted_by_runner:
        validation_save.unlink(missing_ok=True)
    if (process.returncode != 0 or log_text.count(PASS) != 1
            or FAIL in log_text or not temp_slot_deleted_by_runner
            or not production_save_sha256_unchanged):
        print(f"SESSION 6 OUTFIT VISUAL CAPTURE FAIL: rc={process.returncode}\n{log_text[-4000:]}", file=sys.stderr)
        return 1
    if not manifest.is_file():
        print("SESSION 6 OUTFIT VISUAL CAPTURE FAIL: manifest missing", file=sys.stderr)
        return 1
    data = json.loads(manifest.read_text(encoding="utf-8"))
    captures = data.get("captures", [])
    capture_paths = [manifest.parent / name for name in FILENAMES]
    pixel_metrics: list[dict[str, float | int]] = []
    pixel_error = ""
    try:
        pixel_metrics = [png_pixel_metrics(path) for path in capture_paths]
    except (OSError, ValueError, zlib.error) as exc:
        pixel_error = f"{type(exc).__name__}: {exc}"
    hashes_match = len(captures) == len(capture_paths) and all(
        hashlib.sha1(path.read_bytes()).hexdigest().upper() == row.get("sha1")
        for path, row in zip(capture_paths, captures)
        if path.is_file()
    ) and all(path.is_file() for path in capture_paths)
    scene_gates = len(captures) == 12 and all(
        row.get("body_framed") is True
        and row.get("finite_pose") is True
        and row.get("outfit_components_visible") is True
        and row.get("expected_equipped_count") == expected
        and row.get("visible_equipped_component_count") == expected
        and row.get("projected_key_bone_count") == 6
        and row.get("key_bones_inside_viewport") == 6
        and row.get("key_bones_inside_safe_margin", 0) >= 4
        and len(row.get("key_bone_projections", [])) == 6
        and all(
            bone.get("projected") is True
            and bone.get("inside_viewport") is True
            for bone in row.get("key_bone_projections", [])
        )
        and 0.20 <= row.get("subject_screen_height_fraction", 0.0) <= 0.96
        and 0.04 <= row.get("subject_screen_width_fraction", 0.0) <= 0.96
        and abs(row.get("camera_fov_deg", 0.0) - 64.0) <= 0.1
        for row, expected in zip(captures, EXPECTED_EQUIPPED_COUNTS)
    )
    skeletal_material_gates = len(captures) == 12 and all(
        row.get("expected_skeletal_outfit_component_count") == expected
        and row.get("visible_skeletal_outfit_component_count") == expected
        and row.get("visible_skeletal_outfit_materials_are_canonical_mids") is True
        and row.get("canonical_material_has_skeletal_mesh_usage") is True
        and row.get("selected_variant_material_parameters_match") is True
        and row.get("expected_skeletal_material_slot_count", -1) >= expected
        and row.get("canonical_material_mid_count")
            == row.get("expected_skeletal_material_slot_count")
        and row.get("skeletal_usage_ready_mid_count")
            == row.get("expected_skeletal_material_slot_count")
        and row.get("variant_parameter_matched_mid_count")
            == row.get("expected_skeletal_material_slot_count")
        for row, expected in zip(captures, EXPECTED_SKELETAL_COUNTS)
    )
    required_variant_evidence = len(captures) == 12 and (
        "TOP=Graphite" in captures[1].get("selected_skeletal_variants", [])
        and "TOP=Teal" in captures[2].get("selected_skeletal_variants", [])
        and "OUTERWEAR=Teal" in captures[2].get("selected_skeletal_variants", [])
    )
    phase_gates = len(captures) == 12 and all(
        (index < 10 and row.get("throw_phase") == "Idle")
        or (
            index == 10
            and row.get("throw_phase") == "Release"
            and 1.52 <= row.get("montage_position_seconds", -1.0) <= 1.68
        )
        or (
            index == 11
            and row.get("throw_phase") == "FollowThrough"
            and 1.68 <= row.get("montage_position_seconds", -1.0) <= 2.20
        )
        for index, row in enumerate(captures)
    )
    creator_tab_gates = len(captures) == 12 and all(
        row.get("creator_outfit_tab_required") is True
        and row.get("creator_outfit_tab_prepared") is True
        for row in captures[:7]
    )
    creator_right_pane_gates = len(captures) == 12 and all(
        row.get("creator_preview_composed_in_right_pane") is True
        for row in captures[:7]
    )
    creator_view_target_gates = len(captures) == 12 and all(
        row.get("creator_view_target_is_golfer") is True
        for row in captures[:7]
    )
    profile_ordering = len(captures) == 12 and (
        captures[7].get("body_height_cm", 0.0)
        < captures[8].get("body_height_cm", 0.0)
        < captures[9].get("body_height_cm", 0.0)
    )
    pixel_gates = not pixel_error and len(pixel_metrics) == 12 and all(
        metrics["bright_pixels"] >= 2_000
        and metrics["dark_pixels"] >= 1_000
        and metrics["mean_luminance"] >= 8.0
        and metrics["luminance_stddev"] >= 10.0
        for metrics in pixel_metrics
    )
    checks = {
        "manifest_pass": data.get("status") == "PASS",
        "manual_review_explicitly_retained": data.get("manual_visual_review_required") is True,
        "exact_capture_count": len(captures) == 12,
        "exact_order": [row.get("filename") for row in captures] == list(FILENAMES),
        "all_1920x1080": all(row.get("width") == 1920 and row.get("height") == 1080 for row in captures),
        "all_readable": all(row.get("bytes", 0) >= 32768 and len(row.get("sha1", "")) == 40 for row in captures),
        "png_sha1_matches_manifest": hashes_match,
        "runtime_projection_component_and_visibility_gates": scene_gates,
        "runtime_canonical_skeletal_mids_and_variant_parameters": (
            data.get("canonical_outfit_material")
                == "/Game/DiscGolf/Materials/Outfits/"
                   "M_DG_OutfitProxy.M_DG_OutfitProxy"
            and data.get(
                "all_visible_skeletal_outfit_materials_are_canonical_mids") is True
            and data.get(
                "canonical_outfit_material_has_skeletal_mesh_usage") is True
            and data.get("all_selected_variant_material_parameters_match") is True
            and skeletal_material_gates
        ),
        "frame_2_graphite_and_frame_3_teal_runtime_parameters": (
            required_variant_evidence
        ),
        "no_missing_usage_flag_log_token": material_fallback_log_clean[
            "missing usage flag"
        ],
        "no_default_material_fallback_log_token": material_fallback_log_clean[
            "Default Material will be used in game"
        ],
        "exact_release_and_followthrough_phase_positions": phase_gates,
        "creator_outfit_tab_prepared_for_frames_1_to_7": creator_tab_gates,
        "creator_preview_composed_in_right_pane_for_frames_1_to_7": (
            creator_right_pane_gates
        ),
        "creator_view_target_is_golfer_for_frames_1_to_7": (
            data.get("creator_view_target_is_golfer") is True
            and creator_view_target_gates
        ),
        "creator_apply_saved": data.get("creator_apply_saved") is True,
        "creator_apply_reloaded": data.get("creator_apply_reloaded") is True,
        "creator_reload_reconstructed_outfit": (
            data.get("creator_reload_reconstructed_outfit") is True
        ),
        "validation_temp_slot_deleted": (
            data.get("validation_temp_slot_deleted") is True
            and temp_slot_deleted_by_runner
        ),
        "creator_cancel_restored_applied": (
            data.get("creator_cancel_restored_applied") is True
        ),
        "short_baseline_tall_runtime_height_order": profile_ordering,
        "independent_png_brightness_and_contrast": pixel_gates,
        "release_once": data.get("validation_release_callbacks") == 1,
        "watchdog_clock_paused_for_two_throw_frames": (
            data.get("paused_throw_capture_count") == 2
        ),
        "no_gameplay_disc": data.get("world_disc_delta") == 0,
        "no_stroke": data.get("stroke_delta") == 0,
        "no_persistent_writes": data.get("no_persistent_writes") is True,
        "production_save_sha256_unchanged": production_save_sha256_unchanged,
        "files_present": all((manifest.parent / name).is_file() for name in FILENAMES),
    }
    failed = [name for name, passed in checks.items() if not passed]
    if failed:
        detail = f" pixel_error={pixel_error}" if pixel_error else ""
        print(
            "SESSION 6 OUTFIT VISUAL CAPTURE FAIL: "
            + ", ".join(failed) + detail,
            file=sys.stderr,
        )
        return 1
    print(f"SESSION 6 OUTFIT VISUAL CAPTURE PASS: captures=12 manifest={manifest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
