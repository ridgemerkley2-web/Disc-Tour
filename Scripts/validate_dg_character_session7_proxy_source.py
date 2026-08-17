#!/usr/bin/env python3
"""Read-only ordinary-CPython validation of Session 7 proxy source.

Before Blender generation this proves the frozen inputs are source-ready.  Once
the generated manifest exists it additionally validates every recorded FBX and
the saved .blend by exact byte count/hash and Blender round-trip evidence.  It
never launches Blender, Unreal, UBT, a browser, or a downloader; its only write
is the report under Saved/CharacterFramework.
"""

from __future__ import annotations

import hashlib
import json
from collections import Counter
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "SourceArt/DiscGolf/Characters/Customization/Proxy"
SPEC_PATH = SOURCE_ROOT / "proxy_customization_catalog_spec.json"
GENERATOR_PATH = SOURCE_ROOT / "generate_session7_proxy_customization.py"
MANIFEST_PATH = SOURCE_ROOT / "proxy_customization_source_manifest.json"
REPORT_PATH = PROJECT_ROOT / "Saved/CharacterFramework/Session7ProxySourceValidation.json"
FROZEN_CATALOG_SPEC_SHA256 = (
    "27E6B2C74AA5F1F254CC499627FF8A2347F39727E823A4D0B4F769213D59FE45"
)

EXPECTED_IDS = [
    "hair_none", "hair_short", "hair_medium", "hair_mohawk",
    "facialhair_none", "facialhair_stubble", "facialhair_beard",
    "brow_default", "brow_alt", "scar_none", "scar_proxy",
    "tattoo_none", "tattoo_proxy", "voice_default", "voice_alt",
    "pronouns_default", "pronouns_they_them",
]
EXPECTED_VISIBLE = {
    "head_width": "DG_Face_HeadWidth",
    "head_height": "DG_Face_HeadHeight",
    "cheek_fullness": "DG_Face_CheekFullness",
    "jaw_width": "DG_Face_JawWidth",
    "chin_length": "DG_Face_ChinLength",
}
EXPECTED_DEFERRED = {
    "brow_height": "DG_Face_BrowHeight",
    "brow_depth": "DG_Face_BrowDepth",
    "eye_size": "DG_Face_EyeSize",
    "eye_spacing": "DG_Face_EyeSpacing",
    "eye_depth": "DG_Face_EyeDepth",
    "nose_width": "DG_Face_NoseWidth",
    "nose_length": "DG_Face_NoseLength",
    "nose_bridge": "DG_Face_NoseBridge",
    "cheek_width": "DG_Face_CheekWidth",
    "jaw_height": "DG_Face_JawHeight",
    "chin_width": "DG_Face_ChinWidth",
    "mouth_width": "DG_Face_MouthWidth",
    "lip_fullness": "DG_Face_LipFullness",
    "ear_size": "DG_Face_EarSize",
    "ear_angle": "DG_Face_EarAngle",
}
EXPECTED_HEAD_PARAMETERS = [
    "DG_SkinTone", "DG_EyeColor", "DG_Complexion", "DG_Freckles",
    "DG_SunExposure", "DG_ScarProxy", "DG_TattooProxy",
]
EXPECTED_MATERIAL_DEFAULTS = {
    "DG_SkinTone": [0.55, 0.35, 0.24, 1.0],
    "DG_EyeColor": [0.15, 0.24, 0.20, 1.0],
    "DG_Complexion": 0.35,
    "DG_Freckles": 0.0,
    "DG_SunExposure": 0.25,
    "DG_ScarProxy": 0.0,
    "DG_TattooProxy": 0.0,
    "DG_HairColor": [0.05, 0.03, 0.02, 1.0],
}
EXPECTED_MESH_ITEMS = [
    "hair_short", "hair_medium", "hair_mohawk", "facialhair_stubble",
    "facialhair_beard", "brow_default", "brow_alt",
]


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _require(condition: bool, message: str, errors: list[str]) -> None:
    if not condition:
        errors.append(message)


def _validate_frozen_input(spec: dict, path_key: str, hash_key: str,
                           errors: list[str]) -> None:
    relative = spec.get(path_key, "")
    path = PROJECT_ROOT / relative
    _require(path.is_file(), f"Frozen input missing: {relative}", errors)
    if path.is_file():
        _require(_sha256(path) == spec.get(hash_key),
                 f"Frozen input hash differs: {relative}", errors)


def _validate_spec(spec: dict, errors: list[str]) -> list[dict]:
    exact = {
        "schema": "DiscGolfTour.Session7ProxyCustomizationCatalogSpec.v1",
        "content_status": "NON_PRODUCTION_PROXY",
        "shipping_status": "DO_NOT_SHIP",
        "brand_id": "dg_generic",
        "brand_status": "GENERIC_UNBRANDED",
        "geometry_recipe_id": "SESSION7_GENERIC_CUSTOMIZATION_BLOCKOUT_V1",
        "rights_status":
            "PROJECT_LOCAL_GENERATED_TEST_FIXTURE_DERIVED_FROM_ACCEPTED_PROJECT_PROXY",
        "production_license_claim": "NONE",
        "head_attachment_policy": "LEADER_POSE_ACCEPTED_SKEL_DG_MASTER",
        "head_skinning_policy": "FULL_69_BONE_EXPORT_HEAD_BONE_RIGID_WEIGHTS",
        "static_attachment_policy":
            "REST_HEAD_BONE_LOCAL_IDENTITY_WITH_ABSOLUTE_RUNTIME_SCALE",
        "legacy_head_policy":
            "OPAQUE_ROUNDED_BOX_SHELL_ENCLOSES_FROZEN_HEAD_GEO_AT_ALL_SUPPORTED_EXTREMES",
        "catalog_asset":
            "/Game/DiscGolf/Characters/Customization/Data/DA_DG_CosmeticCatalog.DA_DG_CosmeticCatalog",
        "head_mesh_asset":
            "/Game/DiscGolf/Characters/Customization/Head/SK_DG_Head_Proxy",
        "head_source_fbx": "FBX/SK_DG_Head_Proxy.fbx",
        "head_material_asset":
            "/Game/DiscGolf/Materials/CharacterCustomization/M_DG_HeadProxy",
        "hair_material_asset":
            "/Game/DiscGolf/Materials/CharacterCustomization/M_DG_HairProxy",
        "tattoo_none_persistence": "CANONICAL_EMPTY_ARRAY",
    }
    for key, expected in exact.items():
        _require(spec.get(key) == expected, f"Frozen spec field differs: {key}", errors)
    _require(spec.get("external_sources") == [], "External sources must remain empty", errors)
    _require(spec.get("logos") == [], "Logo list must remain empty", errors)
    _require(spec.get("visible_morphs") == EXPECTED_VISIBLE,
             "Exactly five visible morphs/order differ", errors)
    _require(spec.get("deferred_visual_morphs") == EXPECTED_DEFERRED,
             "Explicit 15-morph deferred ledger differs", errors)
    _require(spec.get("head_material_parameters") == EXPECTED_HEAD_PARAMETERS,
             "Head material parameter contract differs", errors)
    _require(spec.get("required_appearance_parameters") == EXPECTED_HEAD_PARAMETERS[:5],
             "Required appearance parameter contract differs", errors)
    _require(spec.get("hair_material_parameters") == ["DG_HairColor"],
             "Hair material parameter contract differs", errors)
    _require(spec.get("material_defaults") == EXPECTED_MATERIAL_DEFAULTS,
             "Material defaults differ", errors)

    envelope = spec.get("legacy_head_envelope", {})
    _require(envelope.get("accepted_head_geo_half_extents_m") == [0.11, 0.10, 0.13]
             and envelope.get("proxy_shell_half_extents_m") == [0.145, 0.135, 0.175]
             and envelope.get("minimum_clearance_m") == 0.01
             and envelope.get("supported_extremes") == [-1.0, 1.0],
             "Frozen legacy-head enclosure contract differs", errors)

    items = spec.get("items", [])
    _require([item.get("item_id") for item in items] == EXPECTED_IDS,
             "Frozen 17-item stable-ID order differs", errors)
    _require(len({item.get("data_asset") for item in items}) == 17,
             "Cosmetic data-asset paths are not unique", errors)
    mesh_items = [item for item in items if item.get("mesh_kind") == "Static"]
    _require([item.get("item_id") for item in mesh_items] == EXPECTED_MESH_ITEMS,
             "Frozen seven mesh-backed cosmetic items differ", errors)
    _require(len({item.get("mesh_asset") for item in mesh_items}) == 7,
             "Static mesh asset paths are not unique", errors)
    _require(len({item.get("source_fbx") for item in mesh_items}) == 7,
             "Static source FBX paths are not unique", errors)
    _require(Counter(item.get("kind") for item in items) == {
        "Hair": 4, "FacialHair": 3, "Eyebrow": 2, "Scar": 2,
        "Tattoo": 2, "Voice": 2, "PronounSet": 2,
    }, "Catalog kind counts differ", errors)
    for item in items:
        item_id = item.get("item_id")
        _require(str(item.get("data_asset", "")).startswith(
                 "/Game/DiscGolf/Characters/Customization/Data/Items/"),
                 f"Data asset escaped owned namespace: {item_id}", errors)
        if item.get("mesh_kind") == "Static":
            source = Path(item.get("source_fbx", ""))
            _require(not source.is_absolute() and ".." not in source.parts
                     and len(source.parts) == 2 and source.parts[0] == "FBX"
                     and source.suffix.casefold() == ".fbx",
                     f"Unsafe static source path: {item_id}", errors)
            _require(str(item.get("mesh_asset", "")).startswith(
                     "/Game/DiscGolf/Characters/Customization/Cosmetics/"),
                     f"Mesh escaped owned namespace: {item_id}", errors)
            _require(item.get("attach_socket") == "head",
                     f"Mesh-backed item does not attach to head: {item_id}", errors)
        else:
            _require(item.get("mesh_kind") == "None" and not item.get("mesh_asset")
                     and not item.get("source_fbx"),
                     f"Non-mesh entry unexpectedly owns geometry: {item_id}", errors)

    presets_path = PROJECT_ROOT / spec.get("buildkit_face_presets", "")
    if presets_path.is_file():
        donor_presets = _load_json(presets_path).get("presets", [])
        normalized = [
            {"preset_id": preset.get("id"), "display_name": preset.get("display_name"),
             "morphs": preset.get("morphs", {})}
            for preset in donor_presets
        ]
        _require(spec.get("face_presets") == normalized,
                 "Face presets differ from frozen donor initialization data", errors)

    for path_key, hash_key in (
        ("master_source_blend", "master_source_blend_sha256"),
        ("master_source_fbx", "master_source_fbx_sha256"),
        ("skeleton_contract", "skeleton_contract_sha256"),
        ("buildkit_face_contract", "buildkit_face_contract_sha256"),
        ("buildkit_face_presets", "buildkit_face_presets_sha256"),
        ("prepared_generator_reference", "prepared_generator_reference_sha256"),
    ):
        _validate_frozen_input(spec, path_key, hash_key, errors)

    face_path = PROJECT_ROOT / spec.get("buildkit_face_contract", "")
    if face_path.is_file():
        donor_morphs = _load_json(face_path).get("morphs", {})
        _require(donor_morphs == (EXPECTED_VISIBLE | EXPECTED_DEFERRED),
                 "Visible+deferred ledgers do not exactly cover donor face contract", errors)
    return mesh_items


def _validate_generator(errors: list[str]) -> None:
    _require(GENERATOR_PATH.is_file(), f"Generator missing: {GENERATOR_PATH}", errors)
    if not GENERATOR_PATH.is_file():
        return
    source = GENERATOR_PATH.read_text(encoding="utf-8")
    _require("--project-root" in source, "Generator lacks explicit project-root contract", errors)
    _require("Path.home()" not in source and "Desktop" not in source,
             "Generator may not write to a user Desktop/home path", errors)
    _require("read_factory_settings" in source and "PASS_BLENDER_5_2_FBX_ROUNDTRIP" in source,
             "Generator lacks isolated FBX round-trip evidence", errors)
    _require("bone.matrix_local.inverted()" in source,
             "Static cosmetics are not converted to rest-head-local space", errors)
    _require("bpy.ops.wm.open_mainfile" in source,
             "Generator does not open the accepted master source", errors)
    _require("write_text" in source and "proxy_customization_source_manifest.json" in source,
             "Generator does not author the source manifest", errors)
    _require("PYTHONHASHSEED" in source and "fixed_header" in source
             and "_keys_to_uuids.clear()" in source,
             "Generator lacks pinned byte-deterministic FBX export controls", errors)


def _validate_roundtrip(record: dict, skeletal: bool, expected_morphs: list[str],
                        label: str, errors: list[str]) -> None:
    roundtrip = record.get("roundtrip_validation", {})
    bounds = roundtrip.get("bounds_blender_units", [])
    _require(
        roundtrip.get("status") == "PASS_BLENDER_5_2_FBX_ROUNDTRIP"
        and roundtrip.get("mesh_count") == 1
        and roundtrip.get("armature_count") == (1 if skeletal else 0)
        and roundtrip.get("bone_count") == (69 if skeletal else 0)
        and roundtrip.get("material_slot_count") == 1
        and roundtrip.get("vertex_count", 0) > 0
        and roundtrip.get("polygon_count", 0) > 0
        and roundtrip.get("morph_targets") == expected_morphs
        and isinstance(bounds, list) and len(bounds) == 3
        and all(isinstance(value, (int, float)) and value > 0.0 for value in bounds),
        f"Blender round-trip contract differs: {label}", errors)
    if skeletal:
        _require("DG_SurfaceMask" in roundtrip.get("vertex_color_attributes", []),
                 "Head FBX lost DG_SurfaceMask", errors)


def _validate_generated(spec: dict, mesh_items: list[dict], errors: list[str]) -> tuple[str, int]:
    fbx_dir = SOURCE_ROOT / "FBX"
    actual_fbx = {path.resolve() for path in fbx_dir.glob("*.fbx")} if fbx_dir.is_dir() else set()
    if not MANIFEST_PATH.is_file():
        _require(not actual_fbx, "Generated FBX exists without its source manifest", errors)
        _require(not (SOURCE_ROOT / "DG_Session7_ProxyCustomization.blend").exists(),
                 "Generated blend exists without its source manifest", errors)
        return "INPUTS_READY_NOT_GENERATED", 0

    manifest = _load_json(MANIFEST_PATH)
    _require(manifest.get("schema") ==
             "DiscGolfTour.Session7ProxyCustomizationSourceManifest.v1",
             "Generated manifest schema differs", errors)
    _require(manifest.get("status") == "PASS_GENERATED_SOURCE_FIXTURE",
             "Generated manifest status differs", errors)
    _require(manifest.get("deterministic_export_contract") ==
             "PYTHONHASHSEED_0;FBX_HEADER_2000_01_01;RESET_EXPORTER_UUID_MAPS",
             "Generated manifest deterministic-export contract differs", errors)
    _require(manifest.get("generation_idempotence_policy") ==
             "MANIFEST_HASH_GATED_ALREADY_CURRENT_NO_WRITE",
             "Generated manifest idempotence policy differs", errors)
    for key in ("content_status", "shipping_status", "brand_id", "brand_status",
                "geometry_recipe_id", "rights_status", "head_attachment_policy",
                "head_skinning_policy", "static_attachment_policy", "legacy_head_policy",
                "visible_morphs", "deferred_visual_morphs"):
        _require(manifest.get(key) == spec.get(key),
                 f"Manifest/spec field differs: {key}", errors)
    _require(manifest.get("external_sources") == [] and manifest.get("logos") == []
             and manifest.get("production_license_claim") == "NONE",
             "Generated manifest gained external/license/logo claims", errors)
    _require(manifest.get("bone_count") == 69,
             "Generated manifest bone count differs", errors)
    _require(str(manifest.get("blender_version", "")).startswith("5.2"),
             "Generated source was not produced by pinned Blender 5.2", errors)
    _require(manifest.get("generator") == GENERATOR_PATH.relative_to(PROJECT_ROOT).as_posix()
             and manifest.get("generator_sha256") == _sha256(GENERATOR_PATH),
             "Generated manifest generator hash differs", errors)
    _require(manifest.get("catalog_spec") == SPEC_PATH.relative_to(PROJECT_ROOT).as_posix()
             and manifest.get("catalog_spec_sha256") == _sha256(SPEC_PATH),
             "Generated manifest catalog-spec hash differs", errors)

    blend = PROJECT_ROOT / str(manifest.get("blend_source", ""))
    _require(blend.is_file(), "Generated source blend missing", errors)
    if blend.is_file():
        _require(blend.stat().st_size == manifest.get("blend_source_bytes")
                 and _sha256(blend) == manifest.get("blend_source_sha256"),
                 "Generated blend size/hash differs", errors)

    enclosure = manifest.get("legacy_head_enclosure_validation", {})
    _require(enclosure.get("status") ==
             "PASS_CONSERVATIVE_SIMULTANEOUS_NEGATIVE_EXTREMES"
             and enclosure.get("supported_extremes") == [-1.0, 1.0]
             and enclosure.get("extreme_combination_count") == 32
             and enclosure.get("blendshape_combination_rule") ==
             "ADDITIVE_DELTAS",
             "Generated head-enclosure proof differs", errors)

    cosmetic_design = manifest.get("head_local_cosmetic_validation", {})
    cosmetic_bounds = cosmetic_design.get("rest_armature_space_bounds", {})
    _require(
        cosmetic_design.get("status") ==
        "PASS_HEAD_LOCAL_OVERLAP_AT_SUPPORTED_VISIBLE_EXTREMES"
        and cosmetic_design.get("hat_hair_policy") ==
        "HAT_HAIR_COVERAGE_HIDES_COMPONENT;STORED_HAIR_ID_UNCHANGED;REMOVE_RESTORES"
        and list(cosmetic_bounds) == EXPECTED_MESH_ITEMS,
        "Generated head-local cosmetic/seam proof differs", errors)

    expected_morphs = list(EXPECTED_VISIBLE.values())
    head = manifest.get("head_output", {})
    _require(head.get("mesh_asset") == spec.get("head_mesh_asset")
             and head.get("source_fbx") == spec.get("head_source_fbx"),
             "Generated head record paths differ", errors)
    head_path = SOURCE_ROOT / str(head.get("source_fbx", ""))
    _require(head_path.is_file(), "Generated head FBX missing", errors)
    if head_path.is_file():
        _require(head_path.stat().st_size == head.get("bytes")
                 and _sha256(head_path) == head.get("sha256"),
                 "Generated head FBX size/hash differs", errors)
    _validate_roundtrip(head, True, expected_morphs, "head", errors)
    actual_shell = manifest.get("actual_head_shell_validation", {})
    _require(
        actual_shell.get("status") == "PASS_ACTUAL_SHELL_VERTEX_ENDPOINT_ENCLOSURE"
        and actual_shell.get("extreme_combination_count") == 32
        and actual_shell.get("shell_vertex_count", 0) > 0
        and all(value >= -1.0e-6 for value in
                actual_shell.get("minimum_axis_clearance_beyond_required_m", []))
        and len(actual_shell.get("minimum_axis_clearance_beyond_required_m", [])) == 3,
        "Generated actual shell endpoint-enclosure proof differs", errors)
    roundtrip_bounds = head.get("roundtrip_validation", {}).get(
        "bounds_blender_units", [])
    _require(
        len(roundtrip_bounds) == 3
        and roundtrip_bounds[0] >= 0.24
        and roundtrip_bounds[1] >= 0.22
        and roundtrip_bounds[2] >= 0.28,
        "Head FBX round-trip bounds cannot enclose the frozen head envelope", errors)

    records = manifest.get("fbx_outputs", [])
    _require([record.get("item_id") for record in records] == EXPECTED_MESH_ITEMS,
             "Generated static record order/IDs differ", errors)
    expected_paths = {head_path.resolve()}
    for item, record in zip(mesh_items, records):
        _require(record.get("kind") == item.get("kind")
                 and record.get("mesh_kind") == "Static"
                 and record.get("mesh_asset") == item.get("mesh_asset")
                 and record.get("source_fbx") == item.get("source_fbx")
                 and record.get("attach_socket") == "head"
                 and record.get("relative_attachment_transform") == "Identity",
                 f"Generated static record differs: {item.get('item_id')}", errors)
        path = SOURCE_ROOT / str(record.get("source_fbx", ""))
        expected_paths.add(path.resolve())
        _require(path.is_file(), f"Generated static FBX missing: {path}", errors)
        if path.is_file():
            _require(path.stat().st_size == record.get("bytes")
                     and _sha256(path) == record.get("sha256"),
                     f"Generated static FBX size/hash differs: {path}", errors)
        _validate_roundtrip(record, False, [], item.get("item_id", ""), errors)
    _require(actual_fbx == expected_paths,
             "Unexpected or missing generated FBX files", errors)
    _require(manifest.get("static_mesh_count") == 7
             and manifest.get("catalog_item_count") == 17,
             "Generated manifest counts differ", errors)
    return "GENERATED_MANIFEST_VALIDATED", len(records) + 1


def validate() -> dict:
    errors: list[str] = []
    _require(SPEC_PATH.is_file(), f"Catalog spec missing: {SPEC_PATH}", errors)
    if SPEC_PATH.is_file():
        _require(_sha256(SPEC_PATH) == FROZEN_CATALOG_SPEC_SHA256,
                 "Frozen catalog spec byte hash differs", errors)
    spec = _load_json(SPEC_PATH) if SPEC_PATH.is_file() else {}
    mesh_items = _validate_spec(spec, errors)
    _validate_generator(errors)
    generated_status, generated_records = _validate_generated(spec, mesh_items, errors)
    result = {
        "schema": "DiscGolfTour.Session7ProxySourceValidation.v1",
        "status": "PASS" if not errors else "FAIL",
        "source_status": generated_status,
        "acceptance_ready": not errors and generated_status == "GENERATED_MANIFEST_VALIDATED",
        "content_status": spec.get("content_status"),
        "shipping_status": spec.get("shipping_status"),
        "brand_id": spec.get("brand_id"),
        "catalog_object_path": spec.get("catalog_asset"),
        "head_object_path": spec.get("head_mesh_asset"),
        "catalog_item_count": len(spec.get("items", [])),
        "static_mesh_count": len(mesh_items),
        "visible_morph_count": len(spec.get("visible_morphs", {})),
        "deferred_visual_morph_count": len(spec.get("deferred_visual_morphs", {})),
        "generated_fbx_records": generated_records,
        "generator_sha256": _sha256(GENERATOR_PATH) if GENERATOR_PATH.is_file() else None,
        "catalog_spec_sha256": _sha256(SPEC_PATH) if SPEC_PATH.is_file() else None,
        "unreal_launched": False,
        "ubt_launched": False,
        "blender_launched_by_validator": False,
        "network_access": "NONE",
        "errors": errors,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if errors:
        raise RuntimeError("Session 7 proxy source validation failed: " + "; ".join(errors))
    return result


def main() -> int:
    result = validate()
    print(
        "SESSION 7 PROXY SOURCE PASS: "
        f"items={result['catalog_item_count']} static=7 visible=5 deferred=15 "
        f"source={result['source_status']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
