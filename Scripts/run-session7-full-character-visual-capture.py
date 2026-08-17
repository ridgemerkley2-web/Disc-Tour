#!/usr/bin/env python3
"""Launch and independently gate the exact 24-shot Session 7 evidence set.

This launcher is intentionally stricter than a screenshot-presence check.  It
protects production saves and packages, validates the runtime manifest, checks
PNG structure/contrast/hashes, and requires pixel differences in the projected
head region for face, hair, appearance, and hat transitions.  Passing these
machine gates never substitutes for the retained manual visual review.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time
import uuid
import zlib


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "DiscGolfTour.uproject"
DEFAULT_ENGINE = Path(
    r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
)
OUTPUT_DIR = (
    ROOT / "Saved/CharacterFramework/Screenshots/Session7_FullCharacter"
)
MANIFEST = OUTPUT_DIR / "Session7_FullCharacter_CaptureManifest.json"
LAUNCH_REPORT = OUTPUT_DIR / "Session7_FullCharacter_LaunchValidation.json"
LOG = ROOT / "Saved/Logs/CharacterFramework_Session7_FullCharacterVisual.log"

PASS = "DG_SESSION7_FULL_CHARACTER_VISUAL_CAPTURE: PASS"
FAIL = "DG_SESSION7_FULL_CHARACTER_VISUAL_CAPTURE: FAIL"
FULL_BODY_FOV_DEG = 64.0
HEAD_CLOSEUP_FOV_DEG = 50.0
# The original 0.20 face-edge threshold was calibrated at 34 degrees. The
# accepted 50-degree closeup scales projected face motion by
# tan(17 degrees)/tan(25 degrees)=0.6556, so 0.13 preserves that strength.
FACE_PRESET_ROI_DELTA_MIN = 0.13
FILENAMES = (
    "01_Full_Creator_Overview.png",
    "02_Identity_Tab.png",
    "03_Body_Tab.png",
    "04_Face_Tab.png",
    "05_Hair_Tab.png",
    "06_Appearance_Tab.png",
    "07_Throw_Style_Tab.png",
    "08_Outfit_Tab.png",
    "09_Face_Default.png",
    "10_Face_Square.png",
    "11_Face_Narrow.png",
    "12_Face_Round.png",
    "13_Hair_Short.png",
    "14_Hair_Medium.png",
    "15_Facial_Hair.png",
    "16_Multiple_Skin_Tones.png",
    "17_Multiple_Eye_Colors.png",
    "18_Hat_Hides_Hair.png",
    "19_Hat_Removed_Hair_Restored.png",
    "20_ShortCompact_Complete.png",
    "21_Baseline_Complete.png",
    "22_TallLongArms_Complete.png",
    "23_Complete_RHBH_Release.png",
    "24_Complete_RHBH_FollowThrough.png",
)
EXPECTED_TABS = (
    "Identity", "Identity", "Body", "Face", "Hair", "Appearance",
    "Throw Style", "Outfit",
)
EXPECTED_TAB_INDICES = (0, 0, 1, 2, 3, 4, 5, 6)
EXPECTED_CONTROLS = (
    ("DisplayName", "Handedness", "Voice", "Pronouns"),
    ("DisplayName", "Handedness", "Voice", "Pronouns"),
    ("Height", "Wingspan", "ShoulderWidth", "TorsoLength", "LegLength",
     "HandScale", "Mass", "Muscularity", "BodyFat", "Chest", "Waist",
     "Hips", "Arms", "Legs", "BodyPresets"),
    ("FacePresets", "HeadWidth", "HeadHeight", "BrowHeight", "BrowDepth",
     "EyeSize", "EyeSpacing", "EyeDepth", "NoseWidth", "NoseLength",
     "NoseBridge", "CheekWidth", "CheekFullness", "JawWidth", "JawHeight",
     "ChinWidth", "ChinLength", "MouthWidth", "LipFullness", "EarSize",
     "EarAngle"),
    ("HairStyle", "FacialHair", "Eyebrow", "HairColor",
     "FacialHairColor", "EyebrowColor"),
    ("SkinTone", "EyeColor", "Complexion", "Freckles", "SunExposure",
     "Scar", "Tattoo"),
    ("RunUp", "ReachBack", "TorsoRotation", "Brace", "Explosiveness",
     "FollowThrough"),
    ("Headwear", "Eyewear", "Top", "Outerwear", "Bottom", "Socks",
     "Footwear", "Glove", "Wrist", "Bag", "Accessory", "Item", "Variant"),
)
VISIBLE_MORPHS = (
    "head_width", "head_height", "cheek_fullness", "jaw_width", "chin_length",
)
STABLE_IDS = (
    "hair_none", "hair_short", "hair_medium", "hair_mohawk",
    "facialhair_none", "facialhair_stubble", "facialhair_beard",
    "brow_default", "brow_alt", "scar_none", "scar_proxy",
    "tattoo_none", "tattoo_proxy", "voice_default", "voice_alt",
    "pronouns_default", "pronouns_they_them",
)
MANUAL_CRITERIA = (
    "all_seven_creator_tabs_are_legible_and_the_preview_remains_in_the_right_pane",
    "five_supported_proxy_face_morphs_and_four_presets_are_visibly_distinct",
    "hair_facial_hair_brows_scar_and_tattoo_read_as_intentional_proxy_choices",
    "skin_eye_complexion_freckles_and_sun_exposure_are_visible_on_the_modular_head",
    "hat_hides_hair_and_removal_restores_the_same_selection_without_popping_or_detachment",
    "short_compact_baseline_and_tall_long_arms_profiles_are_visibly_distinct",
    "complete_character_release_and_follow_through_are_correctly_framed",
    "no_visible_clipping_detachment_duplicate_body_or_duplicate_customization_authority",
    "proxy_assets_are_clearly_marked_do_not_ship_and_not_mistaken_for_final_art",
)
PACKAGE_SUFFIXES = {".uasset", ".umap", ".uexp", ".ubulk", ".uptnl"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", default=os.environ.get("UE_EDITOR", str(DEFAULT_ENGINE)))
    parser.add_argument("--project", default=str(PROJECT))
    parser.add_argument("--manifest", default=str(MANIFEST))
    parser.add_argument("--log", default=str(LOG))
    parser.add_argument("--timeout-seconds", type=float, default=420.0)
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


def sha256(path: Path) -> str | None:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def sha1(path: Path) -> str:
    return hashlib.sha1(path.read_bytes()).hexdigest().upper()


def snapshot_tree(root: Path, suffixes: set[str] | None = None) -> dict[str, dict]:
    result: dict[str, dict] = {}
    if not root.is_dir():
        return result
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        if suffixes is not None and path.suffix.casefold() not in suffixes:
            continue
        result[str(path.absolute())] = {
            "bytes": path.stat().st_size,
            "sha256": sha256(path),
        }
    return result


def snapshot_diff(before: dict[str, dict], after: dict[str, dict]) -> list[str]:
    return sorted(path for path in before.keys() | after.keys()
                  if before.get(path) != after.get(path))


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


def paeth(left: int, above: int, upper_left: int) -> int:
    prediction = left + above - upper_left
    return min(
        ((abs(prediction - left), left),
         (abs(prediction - above), above),
         (abs(prediction - upper_left), upper_left)),
        key=lambda item: item[0],
    )[1]


def decode_png(path: Path) -> tuple[int, int, bytearray]:
    encoded = path.read_bytes()
    if not encoded.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError(f"not a PNG: {path}")
    offset = 8
    width = height = bit_depth = color_type = interlace = None
    compressed = bytearray()
    while offset + 12 <= len(encoded):
        size = struct.unpack(">I", encoded[offset:offset + 4])[0]
        kind = encoded[offset + 4:offset + 8]
        payload = encoded[offset + 8:offset + 8 + size]
        offset += size + 12
        if kind == b"IHDR":
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break
    if (width is None or height is None or bit_depth != 8
            or color_type not in (2, 6) or interlace != 0):
        raise ValueError(f"unsupported PNG layout: {path}")
    channels = 3 if color_type == 2 else 4
    stride = width * channels
    raw = zlib.decompress(bytes(compressed))
    if len(raw) != (stride + 1) * height:
        raise ValueError(f"unexpected PNG scanline length: {path}")
    rgb = bytearray(width * height * 3)
    previous = bytearray(stride)
    source = destination = 0
    for _ in range(height):
        filter_type = raw[source]
        source += 1
        current = bytearray(raw[source:source + stride])
        source += stride
        for index in range(stride):
            left = current[index - channels] if index >= channels else 0
            above = previous[index]
            upper_left = previous[index - channels] if index >= channels else 0
            if filter_type == 1:
                current[index] = (current[index] + left) & 0xFF
            elif filter_type == 2:
                current[index] = (current[index] + above) & 0xFF
            elif filter_type == 3:
                current[index] = (current[index] + ((left + above) // 2)) & 0xFF
            elif filter_type == 4:
                current[index] = (current[index] + paeth(left, above, upper_left)) & 0xFF
            elif filter_type != 0:
                raise ValueError(f"unsupported PNG filter {filter_type}: {path}")
        for pixel in range(0, stride, channels):
            rgb[destination:destination + 3] = current[pixel:pixel + 3]
            destination += 3
        previous = current
    return width, height, rgb


def pixel_metrics(width: int, height: int, rgb: bytearray) -> dict[str, float | int]:
    luminance_sum = luminance_square_sum = bright = dark = sampled = 0
    for pixel in range(0, width * height, 4):
        offset = pixel * 3
        red, green, blue = rgb[offset:offset + 3]
        luminance = (54 * red + 183 * green + 19 * blue) // 256
        luminance_sum += luminance
        luminance_square_sum += luminance * luminance
        bright += luminance >= 80
        dark += luminance <= 24
        sampled += 1
    mean = luminance_sum / max(1, sampled)
    variance = max(0.0, luminance_square_sum / max(1, sampled) - mean * mean)
    return {
        "sampled_pixels": sampled,
        "bright_pixels": bright,
        "dark_pixels": dark,
        "mean_luminance": round(mean, 3),
        "luminance_stddev": round(variance ** 0.5, 3),
    }


def valid_roi(value: object, width: int, height: int) -> bool:
    return (isinstance(value, list) and len(value) == 4
            and all(isinstance(item, (int, float)) for item in value)
            and 0 <= value[0] < value[2] <= width
            and 0 <= value[1] < value[3] <= height
            and value[2] - value[0] >= 32 and value[3] - value[1] >= 32)


def roi_difference(
    first: tuple[int, int, bytearray],
    second: tuple[int, int, bytearray],
    roi: list[int | float],
) -> float:
    width, height, first_rgb = first
    other_width, other_height, second_rgb = second
    if (width, height) != (other_width, other_height):
        return -1.0
    x0, y0, x1, y1 = [int(value) for value in roi]
    difference = samples = 0
    # A two-pixel stride makes this independent check inexpensive while still
    # sampling tens of thousands of projected-head pixels.
    for y in range(y0, y1, 2):
        for x in range(x0, x1, 2):
            offset = (y * width + x) * 3
            difference += sum(abs(first_rgb[offset + channel] - second_rgb[offset + channel])
                              for channel in range(3))
            samples += 3
    return round(difference / max(1, samples), 4)


def check_manifest(
    data: dict,
    manifest: Path,
    decoded: list[tuple[int, int, bytearray]],
    pixel_stats: list[dict[str, float | int]],
    temp_deleted: bool,
    production_unchanged: bool,
    changed_packages: list[str],
    changed_saves: list[str],
) -> tuple[dict[str, bool], dict[str, float]]:
    captures = data.get("captures", [])
    exact_rows = len(captures) == 24
    head_rois_ok = exact_rows and all(
        valid_roi(row.get("head_roi_pixels"), 1920, 1080) for row in captures[8:19]
    )
    pixel_differences: dict[str, float] = {}
    if head_rois_ok and len(decoded) == 24:
        # Presets use the same close camera and default is the comparison base.
        for first, second, label in (
            (8, 9, "face_default_vs_square"),
            (8, 10, "face_default_vs_narrow"),
            (8, 11, "face_default_vs_round"),
            (12, 13, "hair_short_vs_medium"),
            (14, 15, "skin_tone_a_vs_b"),
            (15, 16, "eye_color_a_vs_b"),
            (17, 18, "hat_hidden_vs_restored"),
        ):
            pixel_differences[label] = roi_difference(
                decoded[first], decoded[second], captures[first]["head_roi_pixels"]
            )
    face_values = [
        row.get("visible_proxy_morph_values", {}) for row in captures[8:12]
    ] if exact_rows else []
    face_value_distinct = len(face_values) == 4 and len({
        json.dumps(value, sort_keys=True) for value in face_values
    }) == 4 and all(set(value) == set(VISIBLE_MORPHS) for value in face_values)
    tab_rows = captures[:8] if exact_rows else []
    head_rows = captures[8:19] if exact_rows else []
    complete_rows = captures[19:] if exact_rows else []
    camera_fov_exact = exact_rows and all(
        row.get("camera_fov_valid") is True
        and abs(float(row.get("camera_fov_deg", -999.0)) - (
            HEAD_CLOSEUP_FOV_DEG if 8 <= index <= 18 else FULL_BODY_FOV_DEG
        )) <= 0.1
        for index, row in enumerate(captures)
    )
    checks = {
        "runtime_manifest_pass": data.get("status") == "PASS",
        "schema_exact": data.get("schema") == "DiscGolfTour.Session7FullCharacterVisualEvidence.v1",
        "manual_review_retained": (
            data.get("manual_visual_review_required") is True
            and data.get("manual_review_status") in {"PENDING", "APPROVED"}
            and tuple(data.get("manual_review_criteria", [])) == MANUAL_CRITERIA
        ),
        "proxy_art_disclosure": data.get("proxy_art_status") == "DO_NOT_SHIP",
        "exact_24_order": exact_rows and [row.get("filename") for row in captures] == list(FILENAMES),
        "canonical_paths": (
            data.get("cosmetic_catalog") == "/Game/DiscGolf/Characters/Customization/Data/DA_DG_CosmeticCatalog.DA_DG_CosmeticCatalog"
            and data.get("modular_head_mesh") == "/Game/DiscGolf/Characters/Customization/Head/SK_DG_Head_Proxy.SK_DG_Head_Proxy"
            and data.get("head_material") == "/Game/DiscGolf/Materials/CharacterCustomization/M_DG_HeadProxy.M_DG_HeadProxy"
            and data.get("hair_material") == "/Game/DiscGolf/Materials/CharacterCustomization/M_DG_HairProxy.M_DG_HairProxy"
            and data.get("proxy_source_root") == "SourceArt/DiscGolf/Characters/Customization/Proxy"
        ),
        "frozen_visible_morphs_and_ids": (
            tuple(data.get("visible_proxy_face_morphs", [])) == VISIBLE_MORPHS
            and tuple(data.get("stable_cosmetic_ids", [])) == STABLE_IDS
            and len(data.get("visual_deferred_face_morphs", [])) == 15
        ),
        "all_files_hash_and_dimensions": exact_rows and len(decoded) == 24 and all(
            row.get("filename") == filename
            and (manifest.parent / filename).is_file()
            and row.get("width") == 1920 and row.get("height") == 1080
            and row.get("bytes", 0) >= 32768
            and row.get("sha1") == sha1(manifest.parent / filename)
            for row, filename in zip(captures, FILENAMES)
        ),
        "independent_brightness_and_contrast": len(pixel_stats) == 24 and all(
            item["bright_pixels"] >= 2000 and item["dark_pixels"] >= 1000
            and item["mean_luminance"] >= 8.0 and item["luminance_stddev"] >= 10.0
            for item in pixel_stats
        ),
        "seven_tabs_exact_and_controls_visible": len(tab_rows) == 8 and all(
            row.get("active_tab_index") == index
            and row.get("active_tab") == tab
            and row.get("creator_open") is True
            and row.get("preview_in_right_pane") is True
            and row.get("view_target_is_golfer") is True
            and row.get("expected_controls_visible") is True
            and tuple(row.get("visible_control_ids", ())) == controls
            and row.get("minimum_visible_control_count") == len(controls)
            for row, index, tab, controls in zip(
                tab_rows, EXPECTED_TAB_INDICES, EXPECTED_TABS,
                EXPECTED_CONTROLS)
        ),
        "creator_contracts_proven": all(data.get(name) is True for name in (
            "draft_preserved_across_tabs", "current_tab_reset_scoped",
            "reset_all_restored_defaults", "randomize_locks_respected",
            "randomize_catalog_valid", "creator_apply_saved",
            "creator_apply_reloaded", "creator_cancel_restored_applied",
            "schema8_migration_reloaded", "missing_cosmetic_fallbacks_resolved",
        )),
        "presentation_authority_all_24": exact_rows and all(
            row.get("finite_pose") is True
            and row.get("camera_fov_valid") is True
            and row.get("one_customization_component") is True
            and row.get("one_outfit_component") is True
            and row.get("head_leader_pose") is True
            and row.get("visible_proxy_morphs_applied") is True
            and row.get("cosmetics_attached") is True
            and row.get("cosmetics_collision_free") is True
            and row.get("presentation_materials_valid") is True
            and row.get("head_material_is_canonical_mid") is True
            and row.get("head_material_has_morph_target_usage") is True
            and row.get("outfit_attachments_and_materials_valid") is True
            and row.get("world_disc_delta") == 0
            and row.get("stroke_delta") == 0
            for row in captures
        ),
        "camera_fov_exact_all_24": camera_fov_exact,
        "head_closeup_framing_and_authority": len(head_rows) == 11 and head_rois_ok and all(
            row.get("head_closeup") is True and row.get("head_framed") is True
            and row.get("finite_pose") is True and row.get("creator_open") is True
            and row.get("one_customization_component") is True
            and row.get("one_outfit_component") is True
            and row.get("head_leader_pose") is True
            and row.get("visible_proxy_morphs_applied") is True
            and row.get("cosmetics_attached") is True
            and row.get("cosmetics_collision_free") is True
            and row.get("presentation_materials_valid") is True
            and row.get("head_material_is_canonical_mid") is True
            and row.get("head_material_has_morph_target_usage") is True
            and row.get("outfit_attachments_and_materials_valid") is True
            and row.get("throw_phase") == "Idle"
            and row.get("world_disc_delta") == 0 and row.get("stroke_delta") == 0
            for row in head_rows
        ),
        "four_face_presets_have_distinct_visible_values": face_value_distinct,
        "face_hair_appearance_hat_pixels_change": (
            len(pixel_differences) == 7
            and all(pixel_differences.get(name, -1.0) >= threshold for name, threshold in {
                "face_default_vs_square": FACE_PRESET_ROI_DELTA_MIN,
                "face_default_vs_narrow": FACE_PRESET_ROI_DELTA_MIN,
                "face_default_vs_round": FACE_PRESET_ROI_DELTA_MIN,
                "hair_short_vs_medium": 0.75,
                "skin_tone_a_vs_b": 0.35,
                "eye_color_a_vs_b": 0.02,
                "hat_hidden_vs_restored": 0.75,
            }.items())
        ),
        "appearance_head_only_truthful_scope": (
            data.get("appearance_visual_authority") == "MODULAR_HEAD_ONLY"
            and data.get("body_surface_appearance_deferred") is True
        ),
        "head_material_morph_target_usage_all_24": (
            data.get("head_material_has_morph_target_usage_all_24") is True
            and exact_rows
            and all(row.get("head_material_has_morph_target_usage") is True
                    for row in captures)
        ),
        "hat_hide_restore_preserves_selection": exact_rows and (
            captures[17].get("hair_style_id") == "hair_medium"
            and captures[18].get("hair_style_id") == "hair_medium"
            and captures[17].get("headwear_id") == "proxy_s6_headwear_cap_01"
            and captures[18].get("headwear_id") in {"", "none"}
            and captures[17].get("hair_hidden_by_outfit_coverage") is True
            and captures[17].get("hair_component_visible") is False
            and captures[18].get("hair_hidden_by_outfit_coverage") is False
            and captures[18].get("hair_component_visible") is True
            and captures[17].get("hair_color") == captures[18].get("hair_color")
        ),
        "three_complete_profiles_in_order": len(complete_rows) == 5 and (
            [row.get("profile_id") for row in complete_rows[:3]]
            == ["ShortCompact", "Baseline", "TallLongArms"]
            and complete_rows[0].get("body_height_cm", 0.0)
            < complete_rows[1].get("body_height_cm", 0.0)
            < complete_rows[2].get("body_height_cm", 0.0)
        ),
        "release_and_followthrough_exact": len(complete_rows) == 5 and (
            complete_rows[3].get("throw_phase") == "Release"
            and 1.52 <= complete_rows[3].get("montage_position_seconds", -1.0) <= 1.68
            and complete_rows[4].get("throw_phase") == "FollowThrough"
            and 1.95 <= complete_rows[4].get("montage_position_seconds", -1.0) <= 2.12
            and data.get("validation_release_callbacks") == 1
            and data.get("paused_throw_capture_count") == 2
        ),
        "no_gameplay_or_persistent_writes": (
            data.get("world_disc_delta") == 0 and data.get("stroke_delta") == 0
            and data.get("no_persistent_writes") is True
            and data.get("changed_packages") == [] and data.get("changed_save_games") == []
            and data.get("validation_temp_slot_deleted") is True and temp_deleted
            and production_unchanged and not changed_packages and not changed_saves
        ),
        "production_save_sha256_unchanged": production_unchanged,
        "package_files_unchanged": not changed_packages,
        "save_files_unchanged": not changed_saves,
    }
    return checks, pixel_differences


def main() -> int:
    options = parse_args()
    engine = resolve_engine(options.engine)
    project = Path(options.project).expanduser().absolute()
    manifest = Path(options.manifest).expanduser().absolute()
    log = Path(options.log).expanduser().absolute()
    if not engine.is_file() or not project.is_file():
        raise FileNotFoundError(f"engine/project missing: {engine} / {project}")
    if not 45.0 <= options.timeout_seconds <= 1800.0:
        raise ValueError("timeout must be between 45 and 1800 seconds")
    reserved = (
        "-session7fullcharactervisualcapture", "-session7fullcharacterthrowsmoketest",
        "-session7fullcharactervalidationnosave", "-session7fullcharactervalidationsaveslot",
        "-resx", "-resy", "-abslog", "-nullrhi",
    )
    if any(arg.casefold().startswith(reserved) for arg in options.extra_arg):
        raise ValueError("extra arguments may not override visual harness authority")

    manifest.parent.mkdir(parents=True, exist_ok=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    log.unlink(missing_ok=True)
    for filename in (*FILENAMES, manifest.name, LAUNCH_REPORT.name):
        (manifest.parent / filename).unlink(missing_ok=True)

    slot = "DiscGolfTour_Automation_Session7FullCharacter_" + uuid.uuid4().hex
    slot_path = project.parent / "Saved/SaveGames" / f"{slot}.sav"
    if slot_path.exists():
        raise RuntimeError(f"fresh GUID validation slot unexpectedly exists: {slot_path}")
    packages_before = snapshot_tree(project.parent / "Content", PACKAGE_SUFFIXES)
    saves_before = snapshot_tree(project.parent / "Saved/SaveGames")
    production_save = (
        project.parent / "Saved/SaveGames/DiscGolfTour_Profile_0.sav"
    )
    production_before = sha256(production_save)
    command = [
        str(engine), str(project), "-game", "-Course=PineRidge",
        "-Session7FullCharacterVisualCapture",
        "-Session7FullCharacterValidationNoSave",
        f"-Session7FullCharacterValidationSaveSlot={slot}",
        "-windowed", "-ResX=1920", "-ResY=1080", "-ForceRes",
        "-unattended", "-nop4", "-nosplash", "-NoSound", "-UTF8Output",
        "-stdout", "-FullStdOutLogOutput", f"-abslog={log}",
        *options.extra_arg,
    ]
    started = time.monotonic()
    process = subprocess.Popen(
        command, cwd=project.parent, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
        creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0,
    )
    timed_out = False
    console = ""
    try:
        console, _ = process.communicate(timeout=options.timeout_seconds)
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        partial = exc.output or ""
        if isinstance(partial, bytes):
            partial = partial.decode("utf-8", errors="replace")
        terminate(process)
        tail, _ = process.communicate()
        console = partial + (tail or "")

    log_text = log.read_text(encoding="utf-8-sig", errors="replace") if log.is_file() else console
    temp_deleted = not slot_path.exists()
    production_after = sha256(production_save)
    packages_after = snapshot_tree(project.parent / "Content", PACKAGE_SUFFIXES)
    saves_after = snapshot_tree(project.parent / "Saved/SaveGames")
    changed_packages = snapshot_diff(packages_before, packages_after)
    changed_saves = snapshot_diff(saves_before, saves_after)
    if not temp_deleted:
        # Defensive cleanup does not turn this run into a pass.
        slot_path.unlink(missing_ok=True)

    decoded: list[tuple[int, int, bytearray]] = []
    pixel_stats: list[dict[str, float | int]] = []
    data: dict = {}
    decode_error = ""
    try:
        if manifest.is_file():
            data = json.loads(manifest.read_text(encoding="utf-8"))
        for filename in FILENAMES:
            image = decode_png(manifest.parent / filename)
            decoded.append(image)
            pixel_stats.append(pixel_metrics(*image))
    except (OSError, ValueError, KeyError, json.JSONDecodeError, zlib.error) as exc:
        decode_error = f"{type(exc).__name__}: {exc}"

    checks, pixel_differences = check_manifest(
        data, manifest, decoded, pixel_stats, temp_deleted,
        production_before == production_after, changed_packages, changed_saves,
    ) if data else ({"manifest_readable": False}, {})
    checks.update({
        "completed_before_timeout": not timed_out,
        "process_return_code_zero": process.returncode == 0,
        "runtime_pass_exactly_once": log_text.count(PASS) == 1,
        "runtime_fail_absent": FAIL not in log_text,
        "png_decode_clean": not decode_error and len(decoded) == 24,
        "material_usage_fallback_warnings_absent": all(
            token not in log_text.casefold() for token in (
                "missing usage flag morphtargets",
                "default material will be used in game",
                "material will recompile every editor launch until resaved",
            )
        ),
    })
    failures = [name for name, passed in checks.items() if not passed]
    report = {
        "schema": "DiscGolfTour.Session7FullCharacterLaunchValidation.v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS" if not failures else "FAIL",
        "runtime_seconds": round(time.monotonic() - started, 3),
        "manifest": str(manifest),
        "manual_visual_review_required": True,
        "checks": checks,
        "failures": failures,
        "decode_error": decode_error,
        "pixel_metrics": pixel_stats,
        "projected_head_roi_pixel_differences": pixel_differences,
        "changed_packages": changed_packages,
        "changed_save_games": changed_saves,
        "validation_slot": slot,
        "production_save": str(production_save),
        "production_save_sha256_before": production_before,
        "production_save_sha256_after": production_after,
        "production_save_sha256_unchanged": production_before == production_after,
        "validation_temp_slot_deleted_by_runner": temp_deleted,
    }
    LAUNCH_REPORT.parent.mkdir(parents=True, exist_ok=True)
    LAUNCH_REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if failures:
        print(
            "SESSION 7 FULL CHARACTER VISUAL CAPTURE FAIL: "
            + ", ".join(failures)
            + (f"; {decode_error}" if decode_error else ""),
            file=sys.stderr,
        )
        return 1
    print(
        "SESSION 7 FULL CHARACTER VISUAL CAPTURE MACHINE GATES PASS: "
        f"captures=24 manifest={manifest}; MANUAL VISUAL REVIEW REQUIRED"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
