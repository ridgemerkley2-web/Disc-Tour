#!/usr/bin/env python3
"""Launch and verify the rendered ten-shot Session 5 mocap evidence pass."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import struct
import sys
import time
import zlib


ROOT = Path(__file__).absolute().parents[1]
DEFAULT_ENGINE = Path(
    r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
)
PROJECT = ROOT / "DiscGolfTour.uproject"
LOG = ROOT / "Saved" / "Logs" / "CharacterFramework_Session5_MocapVisualCapture.log"
MANIFEST = (
    ROOT
    / "Saved"
    / "CharacterFramework"
    / "Screenshots"
    / "Session5_MocapPipeline"
    / "Session5_MocapPipeline_CaptureManifest.json"
)
PASS_PREFIX = "DG_SESSION5_MOCAP_VISUAL_CAPTURE: PASS"
FAIL_PREFIX = "DG_SESSION5_MOCAP_VISUAL_CAPTURE: FAIL"
RESERVED = (
    "-session5mocapvisualcapture",
    "-resx",
    "-resy",
    "-abslog",
    "-nullrhi",
)
MINIMUM_SUBJECT_BRIGHT_PIXELS = 5_000
EXPECTED_EVIDENCE_MATERIAL = (
    "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
)
EXPECTED_PROFILE_BONE_LENGTH_POLICY = (
    "ACCEPTED_DG_PROFILE_FACTORS_DERIVE_EXPECTED_SEGMENT_LENGTHS_"
    "THEN_REQUIRE_15_PERCENT_MAX_ERROR"
)
EXPECTED_PROFILE_AWARE_SUBJECTS = (0, 0, 0, 3, 3, 3, 3, 1, 1, 1)
EXPECTED_PROFILE_GROUP_CAMERA = {
    "profile_group_horizontal_frame_fill": 0.88,
    "profile_group_vertical_frame_fill": 0.85,
    "profile_group_camera_clearance_cm": 25,
}


def _paeth(left: int, above: int, upper_left: int) -> int:
    prediction = left + above - upper_left
    left_error = abs(prediction - left)
    above_error = abs(prediction - above)
    upper_left_error = abs(prediction - upper_left)
    if left_error <= above_error and left_error <= upper_left_error:
        return left
    if above_error <= upper_left_error:
        return above
    return upper_left


def _rendered_subject_bright_pixels(path: Path) -> int:
    """Count readable subject pixels below the bright evidence-stage horizon.

    UE writes non-interlaced 8-bit RGB/RGBA screenshots.  Decode that small PNG
    subset with the standard library so the runner has no Pillow dependency.
    This pixel gate complements (and cannot replace) the runtime key-bone gate.
    """
    encoded = path.read_bytes()
    if not encoded.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError("not a PNG")
    offset = 8
    width = height = bit_depth = color_type = interlace = None
    compressed = bytearray()
    while offset + 12 <= len(encoded):
        chunk_size = struct.unpack(">I", encoded[offset : offset + 4])[0]
        chunk_type = encoded[offset + 4 : offset + 8]
        chunk_data = encoded[offset + 8 : offset + 8 + chunk_size]
        offset += 12 + chunk_size
        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(
                ">IIBBBBB", chunk_data
            )
        elif chunk_type == b"IDAT":
            compressed.extend(chunk_data)
        elif chunk_type == b"IEND":
            break
    if (
        width is None
        or height is None
        or bit_depth != 8
        or color_type not in (2, 6)
        or interlace != 0
    ):
        raise ValueError("unsupported screenshot PNG layout")
    channels = 3 if color_type == 2 else 4
    stride = width * channels
    decoded = zlib.decompress(bytes(compressed))
    if len(decoded) != (stride + 1) * height:
        raise ValueError("unexpected PNG scanline length")

    # The transient stage camera is level with the evaluated body center, so the
    # bright sky can reach the middle of the image.  Inspect only the lower body
    # band beneath that horizon; the projection gate already proves head/hands.
    first_subject_row = int(height * 0.58)
    previous = bytearray(stride)
    bright_pixels = 0
    source_offset = 0
    for row_index in range(height):
        filter_type = decoded[source_offset]
        source_offset += 1
        current = bytearray(decoded[source_offset : source_offset + stride])
        source_offset += stride
        for byte_index in range(stride):
            left = current[byte_index - channels] if byte_index >= channels else 0
            above = previous[byte_index]
            upper_left = previous[byte_index - channels] if byte_index >= channels else 0
            if filter_type == 1:
                current[byte_index] = (current[byte_index] + left) & 0xFF
            elif filter_type == 2:
                current[byte_index] = (current[byte_index] + above) & 0xFF
            elif filter_type == 3:
                current[byte_index] = (
                    current[byte_index] + ((left + above) // 2)
                ) & 0xFF
            elif filter_type == 4:
                current[byte_index] = (
                    current[byte_index] + _paeth(left, above, upper_left)
                ) & 0xFF
            elif filter_type != 0:
                raise ValueError(f"unsupported PNG filter {filter_type}")
        if row_index >= first_subject_row:
            for pixel_offset in range(0, stride, channels):
                red, green, blue = current[pixel_offset : pixel_offset + 3]
                if max(red, green, blue) >= 112 and red + green + blue >= 345:
                    bright_pixels += 1
        previous = current
    return bright_pixels


def _args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Render and verify all ten Session 5 synthetic-pipeline evidence captures."
    )
    parser.add_argument(
        "--engine", default=os.environ.get("UE_EDITOR", str(DEFAULT_ENGINE))
    )
    parser.add_argument("--project", default=str(PROJECT))
    parser.add_argument("--log", default=str(LOG))
    parser.add_argument("--manifest", default=str(MANIFEST))
    parser.add_argument("--timeout-seconds", type=float, default=240.0)
    parser.add_argument("--extra-arg", action="append", default=[])
    return parser.parse_args()


def _resolve_engine(value: str) -> Path:
    candidate = Path(value).expanduser().absolute()
    if candidate.is_dir():
        choices = (
            candidate / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe",
            candidate / "Binaries" / "Win64" / "UnrealEditor.exe",
        )
        for choice in choices:
            if choice.is_file():
                return choice
    return candidate


def _terminate(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(
            ["taskkill", "/PID", str(process.pid), "/T", "/F"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    else:
        process.kill()
    try:
        process.wait(timeout=10.0)
    except subprocess.TimeoutExpired:
        process.kill()


def main() -> int:
    args = _args()
    engine = _resolve_engine(args.engine)
    project = Path(args.project).expanduser().absolute()
    log_path = Path(args.log).expanduser().absolute()
    manifest_path = Path(args.manifest).expanduser().absolute()
    if not 30.0 <= args.timeout_seconds <= 1800.0:
        raise ValueError("timeout must be between 30 and 1800 seconds")
    if not engine.is_file():
        raise FileNotFoundError(f"UnrealEditor.exe not found: {engine}")
    if not project.is_file():
        raise FileNotFoundError(f"project not found: {project}")
    for extra in args.extra_arg:
        folded = extra.strip().lower()
        if any(folded.startswith(prefix) for prefix in RESERVED):
            raise ValueError(f"extra argument overrides a required capture argument: {extra}")

    log_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    for path in (log_path, manifest_path):
        if path.exists():
            path.unlink()
    command = [
        str(engine),
        str(project),
        "-game",
        "-Course=PineRidge",
        "-Hole=1",
        "-Session5MocapVisualCapture",
        "-windowed",
        "-ResX=1920",
        "-ResY=1080",
        "-ForceRes",
        "-unattended",
        "-nop4",
        "-nosplash",
        "-NoSound",
        "-stdout",
        "-FullStdOutLogOutput",
        f"-abslog={log_path}",
        *args.extra_arg,
    ]
    creation_flags = subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0
    started = time.monotonic()
    process = subprocess.Popen(
        command,
        cwd=project.parent,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        creationflags=creation_flags,
    )
    try:
        output, _ = process.communicate(timeout=args.timeout_seconds)
    except subprocess.TimeoutExpired:
        _terminate(process)
        output, _ = process.communicate()
        print("SESSION 5 MOCAP VISUAL CAPTURE FAIL: timed out", file=sys.stderr)
        return 1

    log_text = (
        log_path.read_text(encoding="utf-8-sig", errors="replace")
        if log_path.is_file()
        else output
    )
    pass_count = log_text.count(PASS_PREFIX)
    fail_count = log_text.count(FAIL_PREFIX)
    errors: list[str] = []
    bright_pixel_counts: list[int] = []
    if process.returncode != 0:
        errors.append(f"return_code={process.returncode}")
    if pass_count != 1:
        errors.append(f"pass_banner_count={pass_count}")
    if fail_count:
        errors.append(f"fail_banner_count={fail_count}")
    if not manifest_path.is_file():
        errors.append("manifest_missing")
    else:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        captures = manifest.get("captures", [])
        recoveries = manifest.get("profile_recoveries", [])
        if manifest.get("status") != "PASS":
            errors.append("manifest_status_not_pass")
        if manifest.get("shipping_status") != "DO_NOT_SHIP":
            errors.append("shipping_status_not_DO_NOT_SHIP")
        if manifest.get("evidence_material") != EXPECTED_EVIDENCE_MATERIAL:
            errors.append("unexpected_evidence_material")
        if (
            manifest.get("profile_bone_length_policy")
            != EXPECTED_PROFILE_BONE_LENGTH_POLICY
        ):
            errors.append("unexpected_profile_bone_length_policy")
        if manifest.get("profile_fixture_spacing_cm") != 155:
            errors.append("unexpected_profile_fixture_spacing_cm")
        for field, expected in EXPECTED_PROFILE_GROUP_CAMERA.items():
            actual = manifest.get(field)
            if not isinstance(actual, (int, float)) or abs(actual - expected) > 1e-5:
                errors.append(f"unexpected_{field}")
        if manifest.get("gameplay_isolation", {}).get("world_paused_after_restore") is not False:
            errors.append("world_pause_not_restored")
        if len(captures) != 10:
            errors.append(f"capture_count={len(captures)}")
        if len(recoveries) != 3 or any(
            item.get("reason") != "ThrowFinished" for item in recoveries
        ):
            errors.append("profile_recovery_reasons_not_all_ThrowFinished")
        for index, capture in enumerate(captures, start=1):
            required = (
                capture.get("width") == 1920,
                capture.get("height") == 1080,
                len(capture.get("sha1", "")) == 40,
                capture.get("readable") is True,
                capture.get("finite_pose") is True,
                capture.get("all_subjects_projected_and_framed") is True,
                capture.get("projected_body_coverage_readable") is True,
                capture.get("bone_lengths_and_world_body_height_plausible") is True,
                capture.get("camera_outside_key_bone_bounds") is True,
                capture.get("framed_subjects") == capture.get("visible_subjects"),
                capture.get("profile_aware_subjects")
                == EXPECTED_PROFILE_AWARE_SUBJECTS[index - 1],
                0.28
                <= capture.get("min_subject_screen_height_fraction", 0.0)
                <= capture.get("max_subject_screen_height_fraction", 0.0)
                <= 0.86,
                0.025
                <= capture.get("min_subject_screen_width_fraction", 0.0)
                <= capture.get("max_subject_screen_width_fraction", 0.0)
                <= 0.90,
                capture.get("max_bone_length_ratio_error", 1.0) <= 0.15,
                capture.get("stage_asset_identity") is True,
                (
                    capture.get("held_disc_gate_required") is not True
                    or capture.get("held_disc_scale_alignment_or_hidden_state") is True
                ),
                capture.get("world_disc_delta") == 0,
                capture.get("stroke_delta") == 0,
            )
            if not all(required):
                errors.append(f"capture_{index}_gate_failed")
            capture_filename = capture.get("filename")
            if not isinstance(capture_filename, str) or not capture_filename:
                errors.append(f"capture_{index}_filename_missing")
                continue
            capture_path = manifest_path.parent / capture_filename
            try:
                bright_pixels = _rendered_subject_bright_pixels(capture_path)
            except (OSError, ValueError, zlib.error) as exc:
                errors.append(f"capture_{index}_pixel_decode_failed={exc}")
            else:
                bright_pixel_counts.append(bright_pixels)
                if bright_pixels < MINIMUM_SUBJECT_BRIGHT_PIXELS:
                    errors.append(
                        f"capture_{index}_rendered_subject_pixels={bright_pixels}"
                    )
    if errors:
        print("SESSION 5 MOCAP VISUAL CAPTURE FAIL: " + ", ".join(errors), file=sys.stderr)
        return 1
    print(
        "SESSION 5 MOCAP VISUAL CAPTURE PASS: "
        f"captures=10 min_rendered_subject_pixels={min(bright_pixel_counts)} "
        f"duration={time.monotonic() - started:.3f}s manifest={manifest_path}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
