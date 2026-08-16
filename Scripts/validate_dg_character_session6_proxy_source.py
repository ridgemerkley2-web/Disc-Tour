"""Validate the owned Session 6 generic proxy source and provenance contract.

This validator is ordinary CPython and never launches Unreal, UBT, or Blender.
Before DCC generation it proves the frozen catalog/generator inputs are ready.
After generation it additionally verifies every FBX and the Blender source hash.
"""

from __future__ import annotations

import hashlib
import json
from collections import Counter
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "DiscGolf" / "Outfits" / "Proxy"
SPEC_PATH = SOURCE_ROOT / "proxy_outfit_catalog_spec.json"
GENERATOR_PATH = SOURCE_ROOT / "generate_session6_proxy_outfits.py"
MANIFEST_PATH = SOURCE_ROOT / "proxy_outfit_source_manifest.json"
REPORT_PATH = PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session6ProxyOutfitSourceValidation.json"

EXPECTED_MASTER_BLEND_SHA256 = \
    "24E5B920A5EDC70E4DB51D4CFE4E02EEB65ED9D57C30640E2899C12AA320A40C"
EXPECTED_MASTER_FBX_SHA256 = \
    "82ED7FC89A572A346460CCB5530EA493B8397BC0D8BECBC2E22C64B8A31CD683"
EXPECTED_SKELETON_CONTRACT_SHA256 = \
    "E2223DE774CE26900BD482130939EAAB6F813023EBB689447FCB00697C34A7B2"

EXPECTED_SLOT_COUNTS = {
    "Headwear": 2,
    "Eyewear": 1,
    "Top": 2,
    "Outerwear": 1,
    "Bottom": 2,
    "Socks": 1,
    "Footwear": 2,
    "Glove": 1,
    "Wrist": 1,
    "Bag": 1,
    "Accessory": 1,
}

EXPECTED_ITEMS = {
    "proxy_s6_headwear_cap_01": ("Headwear", "Static", ("Hair",), "head"),
    "proxy_s6_headwear_beanie_01": ("Headwear", "Static", ("Hair",), "head"),
    "proxy_s6_eyewear_sport_01": ("Eyewear", "Static", (), "head"),
    "proxy_s6_top_tee_01": ("Top", "Skeletal", ("Torso", "UpperArms"), ""),
    "proxy_s6_top_long_sleeve_01": (
        "Top", "Skeletal", ("Torso", "UpperArms", "Forearms"), ""),
    "proxy_s6_outerwear_jacket_01": (
        "Outerwear", "Skeletal", ("Torso", "UpperArms", "Forearms"), ""),
    "proxy_s6_bottom_shorts_01": (
        "Bottom", "Skeletal", ("Hips", "UpperLegs"), ""),
    "proxy_s6_bottom_pants_01": (
        "Bottom", "Skeletal", ("Hips", "UpperLegs", "LowerLegs"), ""),
    "proxy_s6_socks_crew_01": (
        "Socks", "Skeletal", ("LowerLegs", "Feet"), ""),
    "proxy_s6_footwear_low_01": ("Footwear", "Skeletal", ("Feet",), ""),
    "proxy_s6_footwear_trail_01": ("Footwear", "Skeletal", ("Feet",), ""),
    "proxy_s6_glove_pair_01": ("Glove", "Skeletal", ("Hands",), ""),
    "proxy_s6_wrist_band_left_01": ("Wrist", "Static", (), "hand_l"),
    "proxy_s6_bag_backpack_01": ("Bag", "Static", (), "spine_04"),
    "proxy_s6_accessory_towel_left_01": ("Accessory", "Static", (), "pelvis"),
}

EXPECTED_PATHS = {
    "proxy_s6_headwear_cap_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Headwear_Cap01",
        "/Game/DiscGolf/Outfits/Headwear/SM_DG_Headwear_ProxyCap01",
        "FBX/SM_DG_Headwear_ProxyCap01.fbx"),
    "proxy_s6_headwear_beanie_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Headwear_Beanie01",
        "/Game/DiscGolf/Outfits/Headwear/SM_DG_Headwear_ProxyBeanie01",
        "FBX/SM_DG_Headwear_ProxyBeanie01.fbx"),
    "proxy_s6_eyewear_sport_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Eyewear_Sport01",
        "/Game/DiscGolf/Outfits/Eyewear/SM_DG_Eyewear_ProxySport01",
        "FBX/SM_DG_Eyewear_ProxySport01.fbx"),
    "proxy_s6_top_tee_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Top_Tee01",
        "/Game/DiscGolf/Outfits/Tops/SK_DG_Top_ProxyTee01",
        "FBX/SK_DG_Top_ProxyTee01.fbx"),
    "proxy_s6_top_long_sleeve_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Top_LongSleeve01",
        "/Game/DiscGolf/Outfits/Tops/SK_DG_Top_ProxyLongSleeve01",
        "FBX/SK_DG_Top_ProxyLongSleeve01.fbx"),
    "proxy_s6_outerwear_jacket_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Outerwear_Jacket01",
        "/Game/DiscGolf/Outfits/Outerwear/SK_DG_Outerwear_ProxyJacket01",
        "FBX/SK_DG_Outerwear_ProxyJacket01.fbx"),
    "proxy_s6_bottom_shorts_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Bottom_Shorts01",
        "/Game/DiscGolf/Outfits/Bottoms/SK_DG_Bottom_ProxyShorts01",
        "FBX/SK_DG_Bottom_ProxyShorts01.fbx"),
    "proxy_s6_bottom_pants_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Bottom_Pants01",
        "/Game/DiscGolf/Outfits/Bottoms/SK_DG_Bottom_ProxyPants01",
        "FBX/SK_DG_Bottom_ProxyPants01.fbx"),
    "proxy_s6_socks_crew_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Socks_Crew01",
        "/Game/DiscGolf/Outfits/Socks/SK_DG_Socks_ProxyCrew01",
        "FBX/SK_DG_Socks_ProxyCrew01.fbx"),
    "proxy_s6_footwear_low_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Footwear_Low01",
        "/Game/DiscGolf/Outfits/Footwear/SK_DG_Footwear_ProxyLow01",
        "FBX/SK_DG_Footwear_ProxyLow01.fbx"),
    "proxy_s6_footwear_trail_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Footwear_Trail01",
        "/Game/DiscGolf/Outfits/Footwear/SK_DG_Footwear_ProxyTrail01",
        "FBX/SK_DG_Footwear_ProxyTrail01.fbx"),
    "proxy_s6_glove_pair_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Glove_Pair01",
        "/Game/DiscGolf/Outfits/Gloves/SK_DG_Glove_ProxyPair01",
        "FBX/SK_DG_Glove_ProxyPair01.fbx"),
    "proxy_s6_wrist_band_left_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Wrist_BandLeft01",
        "/Game/DiscGolf/Outfits/Wrist/SM_DG_Wrist_ProxyBandLeft01",
        "FBX/SM_DG_Wrist_ProxyBandLeft01.fbx"),
    "proxy_s6_bag_backpack_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Bag_Backpack01",
        "/Game/DiscGolf/Outfits/Bags/SM_DG_Bag_ProxyBackpack01",
        "FBX/SM_DG_Bag_ProxyBackpack01.fbx"),
    "proxy_s6_accessory_towel_left_01": (
        "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_Accessory_TowelLeft01",
        "/Game/DiscGolf/Outfits/Accessories/SM_DG_Accessory_ProxyTowelLeft01",
        "FBX/SM_DG_Accessory_ProxyTowelLeft01.fbx"),
}

FORBIDDEN_BRAND_TOKENS = (
    "adidas", "discraft", "discmania", "dynamic discs", "innova", "latitude 64",
    "mvp disc", "nike", "oakley", "prodigy disc", "premium disc golf",
    "under armour", "westside discs",
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _require(condition: bool, message: str, errors: list[str]) -> None:
    if not condition:
        errors.append(message)


def _object_path(package_path: str) -> str:
    if "." in package_path.rsplit("/", 1)[-1]:
        return package_path
    return f"{package_path}.{package_path.rsplit('/', 1)[-1]}"


def main() -> None:
    errors: list[str] = []
    _require(SPEC_PATH.is_file(), f"Missing spec: {SPEC_PATH}", errors)
    _require(GENERATOR_PATH.is_file(), f"Missing generator: {GENERATOR_PATH}", errors)
    if errors:
        raise RuntimeError("; ".join(errors))

    spec = _load(SPEC_PATH)
    _require(
        spec.get("schema") == "DiscGolfTour.Session6ProxyOutfitCatalogSpec.v1",
        "Unexpected proxy spec schema", errors)
    _require(spec.get("content_status") == "NON_PRODUCTION_PROXY",
             "Spec must remain NON_PRODUCTION_PROXY", errors)
    _require(spec.get("shipping_status") == "DO_NOT_SHIP",
             "Spec must remain DO_NOT_SHIP", errors)
    _require(spec.get("brand_id") == "dg_generic",
             "Spec brand must remain dg_generic", errors)
    _require(spec.get("brand_status") == "GENERIC_UNBRANDED",
             "Spec brand status must remain GENERIC_UNBRANDED", errors)
    _require(spec.get("external_sources") == [],
             "Spec may not contain external sources", errors)
    _require(spec.get("logos") == [], "Spec may not contain logos", errors)
    _require(spec.get("geometry_recipe_id") == "SESSION6_GENERIC_BLOCKOUT_V1",
             "Generic blockout recipe ID differs", errors)
    _require(spec.get("skeletal_attachment_policy") ==
             "LEADER_POSE_ACCEPTED_SKEL_DG_MASTER",
             "Skeletal Leader Pose policy differs", errors)
    _require(spec.get("skinning_policy") ==
             "FULL_69_BONE_EXPORT_RIGID_SINGLE_BONE_WEIGHTS",
             "Rigid follower skinning policy differs", errors)
    _require(spec.get("static_attachment_policy") ==
             "REST_BONE_LOCAL_IDENTITY_WITH_ABSOLUTE_RUNTIME_SCALE",
             "Static socket-local/absolute-scale policy differs", errors)
    _require(spec.get("coverage_policy") ==
             "METADATA_ONLY_PROXY_BODY_HAS_NO_REGION_MASK_ART",
             "Proxy coverage boundary differs", errors)
    _require(spec.get("bag_authority") ==
             "COSMETIC_ONLY_NO_INVENTORY_NO_DISC_SPAWN_OR_LAUNCH",
             "Cosmetic bag authority boundary differs", errors)
    _require(
        spec.get("catalog_asset") ==
        "/Game/DiscGolf/Outfits/Data/DA_DG_OutfitCatalog.DA_DG_OutfitCatalog",
        "Catalog object path differs from runtime authority", errors)
    _require(spec.get("material_asset") ==
             "/Game/DiscGolf/Materials/Outfits/M_DG_OutfitProxy",
             "Shared proxy material path differs", errors)
    _require(spec.get("master_mesh_asset") ==
             "/Game/DiscGolf/Characters/Meshes/SK_DG_Master"
             and spec.get("master_skeleton_asset") ==
             "/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master",
             "Accepted master asset paths differ", errors)
    _require(spec.get("master_source_blend") ==
             "SourceArt/DiscGolf/Characters/SK_DG_Master_Proxy.blend"
             and spec.get("master_source_fbx") ==
             "SourceArt/DiscGolf/Characters/SK_DG_Master_Proxy.fbx",
             "Accepted master source paths differ", errors)
    _require(spec.get("skeleton_contract") ==
             "Plugins/DiscGolfCharacterFramework/Config/DG_MasterSkeletonContract.json",
             "Installed skeleton-contract path differs", errors)
    _require(spec.get("master_source_blend_sha256") == EXPECTED_MASTER_BLEND_SHA256
             and spec.get("master_source_fbx_sha256") == EXPECTED_MASTER_FBX_SHA256,
             "Accepted master source hashes differ from the frozen checkpoint", errors)
    _require(spec.get("skeleton_contract_sha256") ==
             EXPECTED_SKELETON_CONTRACT_SHA256,
             "Installed skeleton-contract hash differs from the frozen checkpoint", errors)
    _require(spec.get("height_range_cm") == [150.0, 210.0],
             "Height range must be exactly 150-210 cm", errors)
    _require(spec.get("slot_order") == list(EXPECTED_SLOT_COUNTS),
             "Slot order differs from installed enum authority", errors)
    _require(
        spec.get("material_parameters") == [
            "DG_PrimaryColor", "DG_SecondaryColor", "DG_AccentColor",
            "DG_RoughnessBias"],
        "Material parameter contract differs", errors)

    master_blend = PROJECT_ROOT / spec.get("master_source_blend", "")
    master_fbx = PROJECT_ROOT / spec.get("master_source_fbx", "")
    skeleton_contract = PROJECT_ROOT / spec.get("skeleton_contract", "")
    contract_bones: set[str] = set()
    for path, hash_field in (
        (master_blend, "master_source_blend_sha256"),
        (master_fbx, "master_source_fbx_sha256"),
    ):
        _require(path.is_file(), f"Missing protected master source: {path}", errors)
        if path.is_file():
            _require(_sha256(path) == spec.get(hash_field),
                     f"Protected master source hash changed: {path}", errors)
    _require(skeleton_contract.is_file(),
             f"Missing installed skeleton contract: {skeleton_contract}", errors)
    if skeleton_contract.is_file():
        _require(_sha256(skeleton_contract) == spec.get("skeleton_contract_sha256"),
                 "Installed skeleton contract hash changed", errors)
        contract = _load(skeleton_contract)
        contract_bones.update(str(entry[0]) for entry in contract["hierarchy"])
        finger_pattern = contract["finger_pattern"]
        for side in ("l", "r"):
            for finger in finger_pattern["each_hand"]:
                for segment in range(1, int(finger_pattern["segments"]) + 1):
                    contract_bones.add(finger_pattern["name_template"].format(
                        finger=finger, segment=segment, side=side))
        finger_count = (
            2 * len(contract["finger_pattern"]["each_hand"])
            * int(contract["finger_pattern"]["segments"]))
        _require(len(contract["hierarchy"]) + finger_count == 69
                 and len(contract_bones) == 69,
                 "Installed skeleton contract does not expand to 69 bones", errors)
        _require("disc_grip_r" in contract_bones and "disc_grip_l" in contract_bones,
                 "Accepted grip-bone contract is incomplete", errors)

    variants = spec.get("variants", [])
    _require([variant.get("variant_id") for variant in variants]
             == ["Default", "Graphite", "Teal"],
             "Stable variant order/IDs differ", errors)
    for variant in variants:
        for field in ("primary_color", "secondary_color", "accent_color"):
            value = variant.get(field)
            _require(isinstance(value, list) and len(value) == 4
                     and all(isinstance(component, (int, float))
                             and 0.0 <= component <= 1.0 for component in value),
                     f"Invalid {field} for {variant.get('variant_id')}", errors)
        bias = variant.get("roughness_bias")
        _require(isinstance(bias, (int, float)) and -1.0 <= bias <= 1.0,
                 f"Invalid roughness bias for {variant.get('variant_id')}", errors)

    items = spec.get("items", [])
    _require(len(items) == 15, f"Expected 15 items, found {len(items)}", errors)
    _require([item.get("item_id") for item in items] == list(EXPECTED_ITEMS),
             "Exact proxy catalog item order differs", errors)
    _require(set(item.get("item_id") for item in items) == set(EXPECTED_ITEMS),
             "Exact proxy ItemId set differs", errors)
    _require(len({item.get("item_id") for item in items}) == len(items),
             "Duplicate ItemId", errors)
    _require(len({item.get("data_asset") for item in items}) == len(items),
             "Duplicate data-asset path", errors)
    _require(len({item.get("mesh_asset") for item in items}) == len(items),
             "Duplicate mesh path", errors)
    _require(len({item.get("source_fbx") for item in items}) == len(items),
             "Duplicate source FBX", errors)
    _require(dict(Counter(item.get("slot") for item in items)) == EXPECTED_SLOT_COUNTS,
             "Exact slot counts differ", errors)
    _require(sum(item.get("mesh_kind") == "Skeletal" for item in items) == 9,
             "Expected nine skeletal items", errors)
    _require(sum(item.get("mesh_kind") == "Static" for item in items) == 6,
             "Expected six static items", errors)

    searchable_strings: list[str] = []
    for item in items:
        item_id = item.get("item_id")
        if item_id not in EXPECTED_ITEMS:
            continue
        slot, mesh_kind, coverage, socket = EXPECTED_ITEMS[item_id]
        _require(item.get("slot") == slot, f"Slot mismatch: {item_id}", errors)
        _require(item.get("mesh_kind") == mesh_kind,
                 f"Mesh-kind mismatch: {item_id}", errors)
        _require(tuple(item.get("covered_regions", [])) == coverage,
                 f"Coverage mismatch: {item_id}", errors)
        _require(item.get("attach_socket") == socket,
                 f"Socket mismatch: {item_id}", errors)
        if mesh_kind == "Static":
            _require(socket in contract_bones,
                     f"Static attach target is not an accepted master bone: {item_id}", errors)
            _require(socket not in {"disc_grip_l", "disc_grip_r"},
                     f"Cosmetic proxy may not attach to a gameplay grip bone: {item_id}", errors)
        _require(item.get("relative_attachment_transform") == "Identity",
                 f"Attachment transform must be Identity: {item_id}", errors)
        _require(item.get("conflicting_slots") == [],
                 f"Proxy conflicts must remain empty: {item_id}", errors)
        _require(
            (item.get("data_asset"), item.get("mesh_asset"), item.get("source_fbx"))
            == EXPECTED_PATHS[item_id],
            f"Frozen owned asset/source path differs: {item_id}", errors)
        _require(str(item.get("display_name", "")).startswith("PROXY - ")
                 and str(item.get("display_name", "")).endswith("(DO NOT SHIP)"),
                 f"Display name lacks proxy/shipping marker: {item_id}", errors)
        _require(str(item.get("data_asset", "")).startswith(
                     "/Game/DiscGolf/Outfits/Data/Items/DA_DG_Outfit_Proxy_"),
                 f"Data asset outside owned proxy namespace: {item_id}", errors)
        _require(str(item.get("mesh_asset", "")).startswith(
                     "/Game/DiscGolf/Outfits/"),
                 f"Mesh outside owned outfit namespace: {item_id}", errors)
        _require(str(item.get("source_fbx", "")).startswith("FBX/")
                 and str(item.get("source_fbx", "")).endswith(".fbx"),
                 f"Source FBX path invalid: {item_id}", errors)
        searchable_strings.extend(str(value) for value in item.values())

    searchable = "\n".join(searchable_strings).casefold()
    for token in FORBIDDEN_BRAND_TOKENS:
        _require(token not in searchable,
                 f"Forbidden brand token in proxy catalog: {token}", errors)

    generator_text = GENERATOR_PATH.read_text(encoding="utf-8")
    _require("http://" not in generator_text.casefold()
             and "https://" not in generator_text.casefold(),
             "Generator may not reference network sources", errors)
    _require("requests" not in generator_text
             and "urllib" not in generator_text,
             "Generator may not import network clients", errors)
    _require("bpy.ops.wm.open_mainfile" in generator_text,
             "Generator must open the accepted master blend", errors)
    _require("bone.matrix_local.inverted()" in generator_text,
             "Generator must convert static meshes to socket-local space", errors)
    _require("use_armature_deform_only=False" in generator_text,
             "Generator must export the complete accepted 69-bone hierarchy", errors)
    _require("_mark_non_production" in generator_text
             and "DG_ShippingStatus" in generator_text
             and "DO_NOT_SHIP" in generator_text,
             "Generator must tag the Blender source scene/objects DO_NOT_SHIP", errors)

    generated_status = "GENERATION_PENDING"
    generated_records = []
    if MANIFEST_PATH.is_file():
        manifest = _load(MANIFEST_PATH)
        generated_status = "GENERATED_MANIFEST_VALIDATED"
        _require(
            manifest.get("schema") ==
            "DiscGolfTour.Session6ProxyOutfitSourceManifest.v1",
            "Generated manifest schema differs", errors)
        _require(manifest.get("content_status") == "NON_PRODUCTION_PROXY",
                 "Generated manifest content status differs", errors)
        _require(manifest.get("shipping_status") == "DO_NOT_SHIP",
                 "Generated manifest shipping status differs", errors)
        _require(manifest.get("brand_id") == "dg_generic",
                 "Generated manifest brand differs", errors)
        _require(manifest.get("external_sources") == []
                 and manifest.get("logos") == [],
                 "Generated manifest introduced external source/logo", errors)
        for field in (
            "geometry_recipe_id", "skeletal_attachment_policy",
            "skinning_policy", "static_attachment_policy", "coverage_policy",
            "bag_authority",
        ):
            _require(manifest.get(field) == spec.get(field),
                     f"Generated manifest policy differs: {field}", errors)
        _require(str(manifest.get("blender_version", "")).startswith("5.2"),
                 "Generated manifest was not produced by Blender 5.2", errors)
        _require(manifest.get("generator_sha256") == _sha256(GENERATOR_PATH),
                 "Generated manifest generator hash differs", errors)
        _require(manifest.get("catalog_spec_sha256") == _sha256(SPEC_PATH),
                 "Generated manifest spec hash differs", errors)
        _require(manifest.get("bone_count") == 69,
                 "Generated manifest bone count differs", errors)
        _require(manifest.get("skeleton_contract_sha256") ==
                 spec.get("skeleton_contract_sha256"),
                 "Generated manifest skeleton-contract hash differs", errors)
        _require(manifest.get("item_count") == 15
                 and manifest.get("skeletal_mesh_count") == 9
                 and manifest.get("static_mesh_count") == 6,
                 "Generated manifest counts differ", errors)
        _require(manifest.get("variant_ids") == ["Default", "Graphite", "Teal"],
                 "Generated manifest variants differ", errors)
        _require(manifest.get("source_material_name") == "DG_OutfitProxy_SourceSlot"
                 and manifest.get("source_material_count") == 1
                 and manifest.get("unreal_shared_material") ==
                 spec.get("material_asset"),
                 "Generated manifest single shared-material contract differs", errors)

        blend_source = PROJECT_ROOT / str(manifest.get("blend_source", ""))
        _require(blend_source.is_file(), f"Generated blend missing: {blend_source}", errors)
        if blend_source.is_file():
            _require(blend_source.stat().st_size == manifest.get("blend_source_bytes")
                     and _sha256(blend_source) == manifest.get("blend_source_sha256"),
                     "Generated blend size/hash differs", errors)

        generated_records = manifest.get("fbx_outputs", [])
        _require([record.get("item_id") for record in generated_records]
                 == [item.get("item_id") for item in items],
                 "Generated FBX record order/IDs differ", errors)
        expected_fbx_paths = set()
        for record, item in zip(generated_records, items):
            fbx_path = SOURCE_ROOT / str(record.get("source_fbx", ""))
            expected_fbx_paths.add(fbx_path.resolve())
            _require(record.get("mesh_asset") == item.get("mesh_asset")
                     and record.get("mesh_kind") == item.get("mesh_kind")
                     and record.get("attach_socket") == item.get("attach_socket")
                     and record.get("relative_attachment_transform") ==
                     item.get("relative_attachment_transform")
                     and record.get("covered_regions") == item.get("covered_regions"),
                     f"Generated FBX record differs: {item.get('item_id')}", errors)
            roundtrip = record.get("roundtrip_validation", {})
            expected_armatures = 1 if item.get("mesh_kind") == "Skeletal" else 0
            bounds = roundtrip.get("bounds_blender_units", [])
            _require(
                roundtrip.get("status") == "PASS_BLENDER_5_2_FBX_ROUNDTRIP"
                and roundtrip.get("mesh_count") == 1
                and roundtrip.get("armature_count") == expected_armatures
                and roundtrip.get("bone_count") == (69 if expected_armatures else 0)
                and roundtrip.get("vertex_count", 0) > 0
                and roundtrip.get("polygon_count", 0) > 0
                and roundtrip.get("material_slot_count") == 1
                and roundtrip.get("vertex_color_role_count", 0) >= 2
                and isinstance(bounds, list) and len(bounds) == 3
                and all(isinstance(value, (int, float)) and value > 0.0
                        for value in bounds),
                f"Generated FBX Blender round-trip contract differs: "
                f"{item.get('item_id')}", errors)
            _require(fbx_path.is_file(), f"Generated FBX missing: {fbx_path}", errors)
            if fbx_path.is_file():
                _require(fbx_path.stat().st_size == record.get("bytes")
                         and _sha256(fbx_path) == record.get("sha256"),
                         f"Generated FBX size/hash differs: {fbx_path}", errors)
        actual_fbx_paths = {path.resolve() for path in (SOURCE_ROOT / "FBX").glob("*.fbx")}
        _require(actual_fbx_paths == expected_fbx_paths,
                 "Unexpected or missing generated FBX file", errors)

    report = {
        "schema": "DiscGolfTour.Session6ProxyOutfitSourceValidation.v1",
        "status": "PASS" if not errors else "FAIL",
        "source_status": generated_status,
        "acceptance_ready": (
            not errors and generated_status == "GENERATED_MANIFEST_VALIDATED"),
        "ue_launched": False,
        "ubt_launched": False,
        "blender_launched_by_validator": False,
        "content_status": spec.get("content_status"),
        "shipping_status": spec.get("shipping_status"),
        "brand_id": spec.get("brand_id"),
        "generator": str(GENERATOR_PATH),
        "generator_sha256": _sha256(GENERATOR_PATH),
        "catalog_spec": str(SPEC_PATH),
        "catalog_spec_sha256": _sha256(SPEC_PATH),
        "item_count": len(items),
        "skeletal_mesh_count": sum(item.get("mesh_kind") == "Skeletal" for item in items),
        "static_mesh_count": sum(item.get("mesh_kind") == "Static" for item in items),
        "slot_counts": dict(Counter(item.get("slot") for item in items)),
        "variant_ids": [variant.get("variant_id") for variant in variants],
        "generated_fbx_records": len(generated_records),
        "catalog_object_path": spec.get("catalog_asset"),
        "errors": errors,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if errors:
        raise RuntimeError("Session 6 proxy source validation failed: " + "; ".join(errors))
    print(
        "SESSION 6 PROXY SOURCE PASS: "
        f"items={len(items)} skeletal=9 static=6 source_status={generated_status}")


if __name__ == "__main__":
    main()
