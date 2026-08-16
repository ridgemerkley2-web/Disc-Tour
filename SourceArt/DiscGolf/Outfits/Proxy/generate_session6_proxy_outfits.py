"""Generate deterministic, generic Session 6 outfit validation proxies.

Run with the project-pinned Blender 5.2 executable, for example::

    blender.exe --background --python generate_session6_proxy_outfits.py -- \
        --project-root C:\\DGTour

The generated meshes are deliberately blocky mechanical fixtures.  They contain
no downloaded geometry, textures, logos, product shapes, or real-company marks.
Every output remains NON_PRODUCTION_PROXY / DO_NOT_SHIP.

Nine skeletal pieces carry the complete accepted 69-bone armature and rigid
weights for Leader Pose validation.  Six accessories are exported in their
attachment bone's rest-local coordinate space.  Their Unreal data assets can
therefore use an Identity relative attachment transform; the runtime component
must ignore inherited socket scale for these static proxies because the accepted
master has a legacy non-unit root scale.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy


GENERATED_PREFIX = "DG_OUTFIT_"
GENERATED_COLLECTION = "DG_SESSION6_PROXY_OUTFITS"
COLOR_ATTRIBUTE = "DG_ColorRole"
MATERIAL_NAME = "DG_OutfitProxy_SourceSlot"

ROLE_COLORS = {
    "Primary": (1.0, 0.0, 0.0, 1.0),
    "Secondary": (0.0, 1.0, 0.0, 1.0),
    "Accent": (0.0, 0.0, 1.0, 1.0),
}


def _parse_args() -> argparse.Namespace:
    args = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", type=Path, required=True)
    return parser.parse_args(args)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _validate_spec(spec: dict) -> None:
    if spec.get("schema") != "DiscGolfTour.Session6ProxyOutfitCatalogSpec.v1":
        raise RuntimeError("Unexpected Session 6 proxy catalog schema")
    if spec.get("content_status") != "NON_PRODUCTION_PROXY":
        raise RuntimeError("Proxy catalog lost NON_PRODUCTION_PROXY status")
    if spec.get("shipping_status") != "DO_NOT_SHIP":
        raise RuntimeError("Proxy catalog lost DO_NOT_SHIP status")
    if spec.get("brand_id") != "dg_generic":
        raise RuntimeError("Only dg_generic is allowed for generated proxy outfits")
    if spec.get("external_sources") or spec.get("logos"):
        raise RuntimeError("Proxy catalog may not declare external sources or logos")
    if spec.get("geometry_recipe_id") != "SESSION6_GENERIC_BLOCKOUT_V1":
        raise RuntimeError("Unexpected generic proxy geometry recipe")
    if spec.get("skeletal_attachment_policy") != \
            "LEADER_POSE_ACCEPTED_SKEL_DG_MASTER":
        raise RuntimeError("Proxy skeletal attachment policy changed")
    if spec.get("skinning_policy") != \
            "FULL_69_BONE_EXPORT_RIGID_SINGLE_BONE_WEIGHTS":
        raise RuntimeError("Proxy rigid skinning policy changed")
    if spec.get("static_attachment_policy") != \
            "REST_BONE_LOCAL_IDENTITY_WITH_ABSOLUTE_RUNTIME_SCALE":
        raise RuntimeError("Proxy static attachment policy changed")
    if spec.get("coverage_policy") != \
            "METADATA_ONLY_PROXY_BODY_HAS_NO_REGION_MASK_ART":
        raise RuntimeError("Proxy coverage boundary changed")
    if spec.get("bag_authority") != \
            "COSMETIC_ONLY_NO_INVENTORY_NO_DISC_SPAWN_OR_LAUNCH":
        raise RuntimeError("Proxy bag authority boundary changed")
    if spec.get("catalog_asset") != \
            "/Game/DiscGolf/Outfits/Data/DA_DG_OutfitCatalog.DA_DG_OutfitCatalog":
        raise RuntimeError("Proxy catalog path changed")
    if spec.get("material_asset") != \
            "/Game/DiscGolf/Materials/Outfits/M_DG_OutfitProxy":
        raise RuntimeError("Proxy shared material path changed")
    if spec.get("master_source_blend") != \
            "SourceArt/DiscGolf/Characters/SK_DG_Master_Proxy.blend" \
            or spec.get("master_source_fbx") != \
            "SourceArt/DiscGolf/Characters/SK_DG_Master_Proxy.fbx" \
            or spec.get("skeleton_contract") != \
            "Plugins/DiscGolfCharacterFramework/Config/DG_MasterSkeletonContract.json":
        raise RuntimeError("Accepted master source/contract path changed")

    items = spec.get("items", [])
    if len(items) != 15:
        raise RuntimeError(f"Expected exactly 15 proxy items, found {len(items)}")
    if len({item["item_id"] for item in items}) != len(items):
        raise RuntimeError("Proxy ItemIds are not unique")
    if len({item["data_asset"] for item in items}) != len(items):
        raise RuntimeError("Proxy item data-asset paths are not unique")
    if len({item["mesh_asset"] for item in items}) != len(items):
        raise RuntimeError("Proxy mesh paths are not unique")
    if len({item["source_fbx"] for item in items}) != len(items):
        raise RuntimeError("Proxy source FBX paths are not unique")
    if sum(item["mesh_kind"] == "Skeletal" for item in items) != 9:
        raise RuntimeError("Expected exactly nine skeletal proxy meshes")
    if sum(item["mesh_kind"] == "Static" for item in items) != 6:
        raise RuntimeError("Expected exactly six static proxy meshes")

    expected_slots = {
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
    actual_slots = {slot: 0 for slot in expected_slots}
    for item in items:
        actual_slots[item["slot"]] += 1
        source_fbx = Path(item["source_fbx"])
        if source_fbx.is_absolute() or ".." in source_fbx.parts \
                or len(source_fbx.parts) != 2 \
                or source_fbx.parts[0] != "FBX" \
                or source_fbx.suffix.casefold() != ".fbx":
            raise RuntimeError(
                f"Proxy FBX must remain a direct child of the owned FBX folder: "
                f"{item['item_id']} {source_fbx}")
        if not str(item["mesh_asset"]).startswith("/Game/DiscGolf/Outfits/") \
                or not str(item["data_asset"]).startswith(
                    "/Game/DiscGolf/Outfits/Data/Items/"):
            raise RuntimeError(
                f"Proxy asset escaped the owned namespace: {item['item_id']}")
        if item["conflicting_slots"]:
            raise RuntimeError(f"Proxy conflicts must remain empty: {item['item_id']}")
        if item["relative_attachment_transform"] != "Identity":
            raise RuntimeError(f"Proxy attachment must remain Identity: {item['item_id']}")
    if actual_slots != expected_slots:
        raise RuntimeError(f"Proxy slot counts differ: {actual_slots}")

    variants = spec.get("variants", [])
    if [entry.get("variant_id") for entry in variants] != ["Default", "Graphite", "Teal"]:
        raise RuntimeError("Expected exact stable variants Default, Graphite, Teal")


def _expand_contract_hierarchy(contract: dict) -> list[tuple[str, str | None]]:
    hierarchy = [(str(name), parent if parent is None else str(parent))
                 for name, parent in contract["hierarchy"]]
    pattern = contract["finger_pattern"]
    for side in ("l", "r"):
        for finger in pattern["each_hand"]:
            parent = f"hand_{side}"
            for segment in range(1, int(pattern["segments"]) + 1):
                name = pattern["name_template"].format(
                    finger=finger, segment=segment, side=side)
                hierarchy.append((name, parent))
                parent = name
    return hierarchy


def _validate_armature(arm: bpy.types.Object, contract: dict) -> None:
    expected = _expand_contract_hierarchy(contract)
    if len(expected) != 69:
        raise RuntimeError(f"Skeleton contract expanded to {len(expected)}, expected 69")
    actual = {bone.name: bone for bone in arm.data.bones}
    if set(actual) != {name for name, _ in expected}:
        missing = sorted({name for name, _ in expected} - set(actual))
        unexpected = sorted(set(actual) - {name for name, _ in expected})
        raise RuntimeError(f"Master armature mismatch missing={missing} unexpected={unexpected}")
    for name, parent in expected:
        actual_parent = actual[name].parent.name if actual[name].parent else None
        if actual_parent != parent:
            raise RuntimeError(
                f"Master armature parent mismatch {name}: {actual_parent} != {parent}")


def _remove_prior_outputs() -> bpy.types.Collection:
    prior = bpy.data.collections.get(GENERATED_COLLECTION)
    if prior:
        for obj in list(prior.objects):
            bpy.data.objects.remove(obj, do_unlink=True)
        bpy.data.collections.remove(prior)
    for obj in list(bpy.data.objects):
        if obj.name.startswith(GENERATED_PREFIX):
            bpy.data.objects.remove(obj, do_unlink=True)
    collection = bpy.data.collections.new(GENERATED_COLLECTION)
    bpy.context.scene.collection.children.link(collection)
    return collection


def _ensure_source_material() -> bpy.types.Material:
    material = bpy.data.materials.get(MATERIAL_NAME)
    if material is None:
        material = bpy.data.materials.new(MATERIAL_NAME)
    material.diffuse_color = (0.18, 0.42, 0.55, 1.0)
    return material


def _mark_non_production(block, item_id: str = "") -> None:
    block["DG_Session"] = 6
    block["DG_ContentStatus"] = "NON_PRODUCTION_PROXY"
    block["DG_ShippingStatus"] = "DO_NOT_SHIP"
    block["DG_BrandId"] = "dg_generic"
    block["DG_SourceKind"] = "PROJECT_LOCAL_GENERATED_BLOCKOUT"
    if item_id:
        block["DG_ItemId"] = item_id


def _move_to_collection(obj: bpy.types.Object, collection: bpy.types.Collection) -> None:
    for owner in list(obj.users_collection):
        owner.objects.unlink(obj)
    collection.objects.link(obj)


def _box(
    collection: bpy.types.Collection,
    material: bpy.types.Material,
    name: str,
    location: tuple[float, float, float],
    dimensions: tuple[float, float, float],
    role: str,
    bone_name: str | None = None,
) -> bpy.types.Object:
    if role not in ROLE_COLORS:
        raise RuntimeError(f"Unknown color role {role}")
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.object
    obj.name = f"{GENERATED_PREFIX}PART_{name}"
    obj.scale = dimensions
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    _move_to_collection(obj, collection)
    obj.data.materials.append(material)
    color = obj.data.color_attributes.new(
        name=COLOR_ATTRIBUTE, type="BYTE_COLOR", domain="CORNER")
    for datum in color.data:
        datum.color = ROLE_COLORS[role]
    if bone_name:
        group = obj.vertex_groups.new(name=bone_name)
        group.add(list(range(len(obj.data.vertices))), 1.0, "REPLACE")
    return obj


def _join_parts(
    parts: list[bpy.types.Object],
    output_name: str,
    material: bpy.types.Material,
) -> bpy.types.Object:
    if not parts:
        raise RuntimeError(f"No geometry parts supplied for {output_name}")
    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = f"{GENERATED_PREFIX}{output_name}"
    if obj.data.color_attributes.get(COLOR_ATTRIBUTE) is None:
        raise RuntimeError(
            f"Joining geometry lost the {COLOR_ATTRIBUTE} vertex-color contract: {output_name}")
    obj.data.materials.clear()
    obj.data.materials.append(material)
    for polygon in obj.data.polygons:
        polygon.material_index = 0
    obj.matrix_world.identity()
    return obj


def _skeletal_item(
    parts: list[bpy.types.Object],
    output_name: str,
    arm: bpy.types.Object,
    material: bpy.types.Material,
) -> bpy.types.Object:
    obj = _join_parts(parts, output_name, material)
    group_names = {group.index: group.name for group in obj.vertex_groups}
    if not group_names or any(name not in arm.data.bones for name in group_names.values()):
        raise RuntimeError(f"Follower mesh has an invalid deform group: {output_name}")
    for vertex in obj.data.vertices:
        if len(vertex.groups) != 1 or abs(float(vertex.groups[0].weight) - 1.0) > 1.0e-6:
            raise RuntimeError(
                f"Follower proxy requires exactly one rigid unit weight per vertex: "
                f"{output_name} vertex={vertex.index}")
    modifier = obj.modifiers.new(name="Armature", type="ARMATURE")
    modifier.object = arm
    obj.parent = arm
    obj.matrix_parent_inverse = arm.matrix_world.inverted()
    return obj


def _static_item(
    parts: list[bpy.types.Object],
    output_name: str,
    attach_bone: str,
    arm: bpy.types.Object,
    material: bpy.types.Material,
) -> bpy.types.Object:
    obj = _join_parts(parts, output_name, material)
    if len(obj.vertex_groups) != 0:
        raise RuntimeError(f"Static proxy unexpectedly has deform groups: {output_name}")
    bone = arm.data.bones.get(attach_bone)
    if bone is None:
        raise RuntimeError(f"Static proxy attach bone does not exist: {attach_bone}")
    # Convert armature-space vertices to the exact rest-local coordinate system
    # of the attachment bone.  The Unreal item can then retain Identity as its
    # documented local transform and avoid scale-sensitive relative offsets.
    obj.data.transform(bone.matrix_local.inverted())
    obj.data.update()
    return obj


def _parts_for_item(
    item_id: str,
    collection: bpy.types.Collection,
    material: bpy.types.Material,
) -> list[bpy.types.Object]:
    def box(label, loc, dims, role="Primary", bone=None):
        return _box(collection, material, f"{item_id}_{label}", loc, dims, role, bone)

    if item_id == "proxy_s6_top_tee_01":
        return [
            box("torso", (0.0, 0.0, 1.33), (0.315, 0.150, 0.31), "Primary", "spine_03"),
            box("sleeve_l", (0.26, 0.0, 1.47), (0.115, 0.09, 0.10), "Secondary", "upperarm_l"),
            box("sleeve_r", (-0.26, 0.0, 1.47), (0.115, 0.09, 0.10), "Secondary", "upperarm_r"),
            box("chest_accent", (0.0, -0.153, 1.40), (0.10, 0.012, 0.035), "Accent", "spine_03"),
        ]
    if item_id == "proxy_s6_top_long_sleeve_01":
        return [
            box("torso", (0.0, 0.0, 1.33), (0.32, 0.155, 0.315), "Primary", "spine_03"),
            box("upper_l", (0.34, 0.0, 1.475), (0.17, 0.09, 0.09), "Primary", "upperarm_l"),
            box("upper_r", (-0.34, 0.0, 1.475), (0.17, 0.09, 0.09), "Primary", "upperarm_r"),
            box("lower_l", (0.62, 0.0, 1.465), (0.15, 0.075, 0.075), "Secondary", "lowerarm_l"),
            box("lower_r", (-0.62, 0.0, 1.465), (0.15, 0.075, 0.075), "Secondary", "lowerarm_r"),
            box("collar", (0.0, -0.158, 1.505), (0.15, 0.012, 0.025), "Accent", "spine_04"),
        ]
    if item_id == "proxy_s6_outerwear_jacket_01":
        return [
            box("torso", (0.0, 0.0, 1.33), (0.34, 0.175, 0.33), "Primary", "spine_03"),
            box("upper_l", (0.34, 0.0, 1.475), (0.18, 0.10, 0.10), "Primary", "upperarm_l"),
            box("upper_r", (-0.34, 0.0, 1.475), (0.18, 0.10, 0.10), "Primary", "upperarm_r"),
            box("lower_l", (0.62, 0.0, 1.465), (0.155, 0.085, 0.085), "Secondary", "lowerarm_l"),
            box("lower_r", (-0.62, 0.0, 1.465), (0.155, 0.085, 0.085), "Secondary", "lowerarm_r"),
            box("zip", (0.0, -0.178, 1.34), (0.018, 0.012, 0.27), "Accent", "spine_03"),
        ]
    if item_id == "proxy_s6_bottom_shorts_01":
        return [
            box("hips", (0.0, 0.0, 0.94), (0.27, 0.15, 0.13), "Primary", "pelvis"),
            box("leg_l", (0.10, 0.0, 0.82), (0.125, 0.115, 0.15), "Secondary", "thigh_l"),
            box("leg_r", (-0.10, 0.0, 0.82), (0.125, 0.115, 0.15), "Secondary", "thigh_r"),
            box("waist", (0.0, -0.152, 0.985), (0.18, 0.012, 0.025), "Accent", "pelvis"),
        ]
    if item_id == "proxy_s6_bottom_pants_01":
        return [
            box("hips", (0.0, 0.0, 0.94), (0.275, 0.155, 0.14), "Primary", "pelvis"),
            box("thigh_l", (0.10, 0.0, 0.76), (0.105, 0.105, 0.32), "Primary", "thigh_l"),
            box("thigh_r", (-0.10, 0.0, 0.76), (0.105, 0.105, 0.32), "Primary", "thigh_r"),
            box("calf_l", (0.10, 0.0, 0.34), (0.085, 0.085, 0.34), "Secondary", "calf_l"),
            box("calf_r", (-0.10, 0.0, 0.34), (0.085, 0.085, 0.34), "Secondary", "calf_r"),
            box("waist", (0.0, -0.158, 0.99), (0.19, 0.012, 0.022), "Accent", "pelvis"),
        ]
    if item_id == "proxy_s6_socks_crew_01":
        return [
            box("sock_l", (0.10, 0.0, 0.20), (0.082, 0.082, 0.18), "Primary", "calf_l"),
            box("sock_r", (-0.10, 0.0, 0.20), (0.082, 0.082, 0.18), "Primary", "calf_r"),
            box("band_l", (0.10, -0.084, 0.275), (0.085, 0.010, 0.025), "Accent", "calf_l"),
            box("band_r", (-0.10, -0.084, 0.275), (0.085, 0.010, 0.025), "Accent", "calf_r"),
        ]
    if item_id == "proxy_s6_footwear_low_01":
        return [
            box("shoe_l", (0.10, -0.10, 0.08), (0.095, 0.18, 0.075), "Primary", "foot_l"),
            box("shoe_r", (-0.10, -0.10, 0.08), (0.095, 0.18, 0.075), "Primary", "foot_r"),
            box("sole_l", (0.10, -0.10, 0.042), (0.10, 0.185, 0.018), "Secondary", "foot_l"),
            box("sole_r", (-0.10, -0.10, 0.042), (0.10, 0.185, 0.018), "Secondary", "foot_r"),
        ]
    if item_id == "proxy_s6_footwear_trail_01":
        return [
            box("shoe_l", (0.10, -0.10, 0.085), (0.105, 0.19, 0.085), "Primary", "foot_l"),
            box("shoe_r", (-0.10, -0.10, 0.085), (0.105, 0.19, 0.085), "Primary", "foot_r"),
            box("toe_l", (0.10, -0.205, 0.078), (0.11, 0.055, 0.07), "Secondary", "foot_l"),
            box("toe_r", (-0.10, -0.205, 0.078), (0.11, 0.055, 0.07), "Secondary", "foot_r"),
            box("lug_l", (0.10, -0.10, 0.037), (0.11, 0.20, 0.022), "Accent", "foot_l"),
            box("lug_r", (-0.10, -0.10, 0.037), (0.11, 0.20, 0.022), "Accent", "foot_r"),
        ]
    if item_id == "proxy_s6_glove_pair_01":
        return [
            box("hand_l", (0.81, 0.0, 1.46), (0.13, 0.065, 0.050), "Primary", "hand_l"),
            box("hand_r", (-0.81, 0.0, 1.46), (0.13, 0.065, 0.050), "Primary", "hand_r"),
            box("band_l", (0.755, -0.036, 1.46), (0.025, 0.010, 0.055), "Accent", "hand_l"),
            box("band_r", (-0.755, -0.036, 1.46), (0.025, 0.010, 0.055), "Accent", "hand_r"),
        ]
    if item_id == "proxy_s6_headwear_cap_01":
        return [
            box("crown", (0.0, 0.0, 1.82), (0.150, 0.120, 0.110), "Primary"),
            box("brim", (0.0, -0.090, 1.785), (0.140, 0.100, 0.018), "Secondary"),
            box("accent", (0.0, -0.061, 1.83), (0.060, 0.008, 0.025), "Accent"),
        ]
    if item_id == "proxy_s6_headwear_beanie_01":
        return [
            box("crown", (0.0, 0.0, 1.81), (0.145, 0.115, 0.150), "Primary"),
            box("band", (0.0, -0.059, 1.765), (0.148, 0.008, 0.040), "Secondary"),
            box("top", (0.0, 0.0, 1.90), (0.040, 0.040, 0.040), "Accent"),
        ]
    if item_id == "proxy_s6_eyewear_sport_01":
        return [
            box("lens_l", (0.035, -0.058, 1.70), (0.055, 0.012, 0.040), "Primary"),
            box("lens_r", (-0.035, -0.058, 1.70), (0.055, 0.012, 0.040), "Primary"),
            box("bridge", (0.0, -0.059, 1.70), (0.020, 0.010, 0.012), "Accent"),
            box("temple_l", (0.060, -0.005, 1.70), (0.012, 0.100, 0.012), "Secondary"),
            box("temple_r", (-0.060, -0.005, 1.70), (0.012, 0.100, 0.012), "Secondary"),
        ]
    if item_id == "proxy_s6_wrist_band_left_01":
        return [
            box("band", (0.72, 0.0, 1.462), (0.035, 0.075, 0.075), "Primary"),
            box("face", (0.72, -0.040, 1.462), (0.045, 0.010, 0.040), "Accent"),
        ]
    if item_id == "proxy_s6_bag_backpack_01":
        return [
            box("body", (0.0, 0.18, 1.29), (0.31, 0.16, 0.44), "Primary"),
            box("pocket", (0.0, 0.268, 1.20), (0.23, 0.045, 0.15), "Secondary"),
            box("accent", (0.0, 0.292, 1.34), (0.13, 0.012, 0.025), "Accent"),
        ]
    if item_id == "proxy_s6_accessory_towel_left_01":
        return [
            box("towel", (0.24, 0.13, 0.79), (0.12, 0.035, 0.32), "Primary"),
            box("clip", (0.24, 0.13, 0.96), (0.055, 0.05, 0.035), "Accent"),
            box("stripe", (0.24, 0.112, 0.73), (0.125, 0.010, 0.035), "Secondary"),
        ]
    raise RuntimeError(f"No deterministic geometry recipe for {item_id}")


def _export_fbx(asset: bpy.types.Object, arm: bpy.types.Object | None, path: Path) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    # The nine follower meshes share one Armature parent. Hide every other
    # generated object so dependency traversal cannot accidentally fold another
    # garment into this item's FBX when the Armature is selected.
    for candidate in bpy.data.objects:
        if candidate.name.startswith(GENERATED_PREFIX):
            hidden = candidate is not asset
            candidate.hide_set(hidden)
            candidate.hide_viewport = hidden
            candidate.hide_render = hidden
    if arm is not None:
        arm.hide_set(False)
        arm.hide_viewport = False
        arm.select_set(True)
        bpy.context.view_layer.objects.active = arm
    asset.hide_set(False)
    asset.hide_viewport = False
    asset.select_set(True)
    if arm is None:
        bpy.context.view_layer.objects.active = asset
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=str(path),
        use_selection=True,
        object_types={"ARMATURE", "MESH"} if arm is not None else {"MESH"},
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_ALL",
        use_space_transform=True,
        bake_space_transform=False,
        use_mesh_modifiers=True,
        mesh_smooth_type="FACE",
        use_subsurf=False,
        use_mesh_edges=False,
        use_tspace=False,
        add_leaf_bones=False,
        primary_bone_axis="Y",
        secondary_bone_axis="X",
        use_armature_deform_only=False,
        armature_nodetype="NULL",
        bake_anim=False,
        path_mode="STRIP",
        embed_textures=False,
        batch_mode="OFF",
        use_batch_own_dir=False,
        use_metadata=False,
        axis_forward="-Y",
        axis_up="Z",
    )
    if not path.is_file() or path.stat().st_size <= 0:
        raise RuntimeError(f"Blender did not emit a non-empty FBX: {path}")


def _validate_export_roundtrip(path: Path, item: dict, contract: dict) -> dict:
    """Reimport one FBX and prove the emitted file retained its core contract."""
    prior_objects = set(bpy.data.objects.keys())
    prior_collections = set(bpy.data.collections.keys())
    imported_objects: list[bpy.types.Object] = []
    try:
        result = bpy.ops.import_scene.fbx(filepath=str(path), use_anim=False)
        if "FINISHED" not in result:
            raise RuntimeError(f"FBX round-trip import did not finish: {path} {result}")
        imported_objects = [
            bpy.data.objects[name]
            for name in sorted(set(bpy.data.objects.keys()) - prior_objects)
        ]
        meshes = [obj for obj in imported_objects if obj.type == "MESH"]
        armatures = [obj for obj in imported_objects if obj.type == "ARMATURE"]
        unexpected = [
            obj for obj in imported_objects
            if obj.type not in {"MESH", "ARMATURE", "EMPTY"}
        ]
        if len(meshes) != 1 or unexpected:
            raise RuntimeError(
                f"FBX round trip expected one mesh and no camera/light: {path} "
                f"meshes={len(meshes)} unexpected={[obj.type for obj in unexpected]}")
        mesh = meshes[0]
        if len(mesh.data.materials) != 1:
            raise RuntimeError(f"FBX lost its single source material slot: {path}")
        if len(mesh.data.vertices) <= 0 or len(mesh.data.polygons) <= 0:
            raise RuntimeError(f"FBX round trip has no render geometry: {path}")

        attributes = list(mesh.data.color_attributes)
        if not attributes:
            raise RuntimeError(f"FBX lost all vertex colors: {path}")
        role_attribute = mesh.data.color_attributes.get(COLOR_ATTRIBUTE) or attributes[0]
        unique_colors = {
            tuple(round(float(component), 4) for component in datum.color)
            for datum in role_attribute.data
        }
        if len(unique_colors) < 2:
            raise RuntimeError(f"FBX lost its multi-role vertex-color data: {path}")

        points = [coordinate for corner in mesh.bound_box for coordinate in corner]
        if not points or not all(math.isfinite(float(value)) for value in points):
            raise RuntimeError(f"FBX round-trip bounds are not finite: {path}")
        axis_extents = [
            max(corner[axis] for corner in mesh.bound_box)
            - min(corner[axis] for corner in mesh.bound_box)
            for axis in range(3)
        ]
        if not all(float(value) > 1.0e-6 for value in axis_extents):
            raise RuntimeError(f"FBX round-trip bounds collapsed on an axis: {path}")

        if item["mesh_kind"] == "Skeletal":
            if len(armatures) != 1:
                raise RuntimeError(
                    f"Skeletal FBX round trip expected one Armature: {path} "
                    f"found={len(armatures)}")
            _validate_armature(armatures[0], contract)
            group_names = {
                group.index: group.name for group in mesh.vertex_groups}
            if not group_names:
                raise RuntimeError(f"Skeletal FBX lost all skin groups: {path}")
            for vertex in mesh.data.vertices:
                if len(vertex.groups) != 1 \
                        or abs(float(vertex.groups[0].weight) - 1.0) > 1.0e-4:
                    raise RuntimeError(
                        f"Skeletal FBX lost rigid unit weights: {path} "
                        f"vertex={vertex.index}")
                if group_names[vertex.groups[0].group] not in armatures[0].data.bones:
                    raise RuntimeError(
                        f"Skeletal FBX has an unknown deform group: {path}")
        else:
            if armatures or len(mesh.vertex_groups) != 0:
                raise RuntimeError(
                    f"Static FBX unexpectedly contains a rig/deform group: {path}")

        return {
            "status": "PASS_BLENDER_5_2_FBX_ROUNDTRIP",
            "mesh_count": len(meshes),
            "armature_count": len(armatures),
            "bone_count": len(armatures[0].data.bones) if armatures else 0,
            "vertex_count": len(mesh.data.vertices),
            "polygon_count": len(mesh.data.polygons),
            "material_slot_count": len(mesh.data.materials),
            "vertex_color_attribute": role_attribute.name,
            "vertex_color_role_count": len(unique_colors),
            "bounds_blender_units": [float(value) for value in axis_extents],
        }
    finally:
        for obj in imported_objects:
            if obj.name in bpy.data.objects:
                bpy.data.objects.remove(obj, do_unlink=True)
        for name in sorted(set(bpy.data.collections.keys()) - prior_collections, reverse=True):
            collection = bpy.data.collections.get(name)
            if collection is not None \
                    and len(collection.objects) == 0 \
                    and len(collection.children) == 0:
                bpy.data.collections.remove(collection)


def main() -> None:
    args = _parse_args()
    project_root = args.project_root.resolve()
    source_root = project_root / "SourceArt" / "DiscGolf" / "Outfits" / "Proxy"
    script_path = Path(__file__).resolve()
    spec_path = source_root / "proxy_outfit_catalog_spec.json"
    manifest_path = source_root / "proxy_outfit_source_manifest.json"
    blend_output = source_root / "DG_Session6_ProxyOutfits.blend"
    spec = _load_json(spec_path)
    _validate_spec(spec)

    if tuple(bpy.app.version[:2]) != (5, 2):
        raise RuntimeError(
            f"Session 6 generator requires Blender 5.2.x, found {bpy.app.version_string}")

    master_blend = project_root / spec["master_source_blend"]
    master_fbx = project_root / spec["master_source_fbx"]
    contract_path = project_root / spec["skeleton_contract"]
    if _sha256(master_blend) != spec["master_source_blend_sha256"]:
        raise RuntimeError("Accepted master .blend hash changed; refusing proxy generation")
    if _sha256(master_fbx) != spec["master_source_fbx_sha256"]:
        raise RuntimeError("Accepted master FBX hash changed; refusing proxy generation")
    if _sha256(contract_path) != spec["skeleton_contract_sha256"]:
        raise RuntimeError("Installed skeleton contract hash changed; refusing proxy generation")
    contract = _load_json(contract_path)

    bpy.ops.wm.open_mainfile(filepath=str(master_blend))
    arm = bpy.data.objects.get("Armature")
    if arm is None or arm.type != "ARMATURE":
        raise RuntimeError("Accepted master Armature object is missing")
    _validate_armature(arm, contract)
    arm.data.pose_position = "REST"

    collection = _remove_prior_outputs()
    material = _ensure_source_material()
    _mark_non_production(bpy.context.scene)
    _mark_non_production(collection)
    _mark_non_production(material)
    generated: dict[str, bpy.types.Object] = {}
    recipe_ids = {item["item_id"] for item in spec["items"]}

    for item in spec["items"]:
        item_id = item["item_id"]
        output_name = Path(item["source_fbx"]).stem
        parts = _parts_for_item(item_id, collection, material)
        if item["mesh_kind"] == "Skeletal":
            obj = _skeletal_item(parts, output_name, arm, material)
        else:
            if not item["attach_socket"]:
                raise RuntimeError(f"Static proxy lacks attach socket: {item_id}")
            obj = _static_item(
                parts, output_name, item["attach_socket"], arm, material)
        _mark_non_production(obj, item_id)
        generated[item_id] = obj
    if set(generated) != recipe_ids:
        raise RuntimeError("Generated proxy recipe set differs from catalog")

    # Save a project-local source scene. Never overwrite the accepted master.
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_output), check_existing=False)

    fbx_records = []
    for item in spec["items"]:
        item_id = item["item_id"]
        fbx_path = source_root / item["source_fbx"]
        _export_fbx(
            generated[item_id],
            arm if item["mesh_kind"] == "Skeletal" else None,
            fbx_path,
        )
        roundtrip = _validate_export_roundtrip(fbx_path, item, contract)
        fbx_records.append({
            "item_id": item_id,
            "slot": item["slot"],
            "mesh_kind": item["mesh_kind"],
            "mesh_asset": item["mesh_asset"],
            "source_fbx": item["source_fbx"],
            "attach_socket": item["attach_socket"],
            "relative_attachment_transform": item["relative_attachment_transform"],
            "covered_regions": item["covered_regions"],
            "bytes": fbx_path.stat().st_size,
            "sha256": _sha256(fbx_path),
            "roundtrip_validation": roundtrip,
        })

    manifest = {
        "schema": "DiscGolfTour.Session6ProxyOutfitSourceManifest.v1",
        "status": "PASS_GENERATED_SOURCE_FIXTURE",
        "content_status": spec["content_status"],
        "shipping_status": spec["shipping_status"],
        "brand_id": spec["brand_id"],
        "brand_status": spec["brand_status"],
        "rights_status": "PROJECT_LOCAL_GENERATED_TEST_FIXTURE_DERIVED_FROM_ACCEPTED_PROJECT_PROXY",
        "production_license_claim": "NONE",
        "external_sources": [],
        "logos": [],
        "geometry_recipe_id": spec["geometry_recipe_id"],
        "skeletal_attachment_policy": spec["skeletal_attachment_policy"],
        "skinning_policy": spec["skinning_policy"],
        "static_attachment_policy": spec["static_attachment_policy"],
        "coverage_policy": spec["coverage_policy"],
        "bag_authority": spec["bag_authority"],
        "source_material_name": MATERIAL_NAME,
        "source_material_count": 1,
        "unreal_shared_material": spec["material_asset"],
        "blender_version": bpy.app.version_string,
        "generator": str(script_path.relative_to(project_root)).replace("\\", "/"),
        "generator_sha256": _sha256(script_path),
        "catalog_spec": str(spec_path.relative_to(project_root)).replace("\\", "/"),
        "catalog_spec_sha256": _sha256(spec_path),
        "master_source_blend": spec["master_source_blend"],
        "master_source_blend_sha256": _sha256(master_blend),
        "master_source_fbx": spec["master_source_fbx"],
        "master_source_fbx_sha256": _sha256(master_fbx),
        "skeleton_contract": spec["skeleton_contract"],
        "skeleton_contract_sha256": _sha256(contract_path),
        "bone_count": len(arm.data.bones),
        "skeletal_mesh_count": sum(item["mesh_kind"] == "Skeletal" for item in spec["items"]),
        "static_mesh_count": sum(item["mesh_kind"] == "Static" for item in spec["items"]),
        "item_count": len(spec["items"]),
        "variant_ids": [variant["variant_id"] for variant in spec["variants"]],
        "blend_source": str(blend_output.relative_to(project_root)).replace("\\", "/"),
        "blend_source_bytes": blend_output.stat().st_size,
        "blend_source_sha256": _sha256(blend_output),
        "fbx_outputs": fbx_records,
    }
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(
        "DG_SESSION6_PROXY_GENERATION_PASS "
        f"items={len(spec['items'])} skeletal=9 static=6 manifest={manifest_path}")


if __name__ == "__main__":
    main()
