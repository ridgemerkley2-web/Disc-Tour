"""Generate deterministic Session 7 head and cosmetic proxy fixtures.

Run only with the project-pinned Blender 5.2 executable::

    blender.exe --background --python generate_session7_proxy_customization.py -- \
        --project-root C:\\DGTour

The output is original blockout geometry derived from the accepted project-local
DG master proxy.  It contains no downloaded geometry, textures, logos, likeness,
or vendor content.  Every generated file is NON_PRODUCTION_PROXY / DO_NOT_SHIP.

The frozen body already contains a rigid block head.  The modular proxy is an
opaque rounded-box shell with enough clearance to enclose that block at every
supported +/-1 morph extreme; the accepted body and 69-bone hierarchy remain
untouched.  Exactly five morph targets are visually implemented.  The other 15
contract controls remain explicit production-visual deferrals in the catalog
specification rather than being represented by fake shapes.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import math
import os
import sys
from itertools import product
from pathlib import Path

import bpy
from mathutils import Vector


GENERATED_PREFIX = "DG_S7_"
GENERATED_COLLECTION = "DG_SESSION7_PROXY_CUSTOMIZATION"
FROZEN_CATALOG_SPEC_SHA256 = (
    "27E6B2C74AA5F1F254CC499627FF8A2347F39727E823A4D0B4F769213D59FE45"
)
SURFACE_MASK_ATTRIBUTE = "DG_SurfaceMask"
HEAD_SOURCE_MATERIAL = "DG_HeadProxy_SourceSlot"
HAIR_SOURCE_MATERIAL = "DG_HairProxy_SourceSlot"
HEAD_CENTER_Z = 1.70
HEAD_HALF = Vector((0.145, 0.135, 0.175))
LEGACY_HEAD_HALF = Vector((0.11, 0.10, 0.13))
MIN_CLEARANCE = 0.01
HEAD_WIDTH_GAIN = 0.08
HEAD_HEIGHT_GAIN = 0.08
CHEEK_GAIN = 0.08
JAW_GAIN = 0.09
CHIN_SHIFT = 0.018


def _parse_args() -> argparse.Namespace:
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
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


def _require_hash(project_root: Path, relative: str, expected: str) -> Path:
    path = project_root / relative
    if not path.is_file():
        raise RuntimeError(f"Required frozen input is missing: {path}")
    actual = _sha256(path)
    if actual != expected:
        raise RuntimeError(f"Frozen input hash differs: {relative} {actual} != {expected}")
    return path


def _validate_spec(spec: dict, project_root: Path) -> None:
    exact = {
        "schema": "DiscGolfTour.Session7ProxyCustomizationCatalogSpec.v1",
        "content_status": "NON_PRODUCTION_PROXY",
        "shipping_status": "DO_NOT_SHIP",
        "brand_id": "dg_generic",
        "brand_status": "GENERIC_UNBRANDED",
        "geometry_recipe_id": "SESSION7_GENERIC_CUSTOMIZATION_BLOCKOUT_V1",
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
        if spec.get(key) != expected:
            raise RuntimeError(f"Frozen spec field changed: {key}")
    if spec.get("external_sources") or spec.get("logos"):
        raise RuntimeError("Generated proxy specification may not declare external sources or logos")
    if spec.get("production_license_claim") != "NONE":
        raise RuntimeError("Generated proxy may not claim a production asset license")

    visible = spec.get("visible_morphs", {})
    expected_visible = {
        "head_width": "DG_Face_HeadWidth",
        "head_height": "DG_Face_HeadHeight",
        "cheek_fullness": "DG_Face_CheekFullness",
        "jaw_width": "DG_Face_JawWidth",
        "chin_length": "DG_Face_ChinLength",
    }
    if visible != expected_visible:
        raise RuntimeError("Exactly five frozen proxy-visible face morphs are required")
    deferred = spec.get("deferred_visual_morphs", {})
    if len(deferred) != 15 or set(visible) & set(deferred):
        raise RuntimeError("The explicit 15-control production-visual deferral ledger changed")
    if len(set(visible.values()) | set(deferred.values())) != 20:
        raise RuntimeError("Visible and deferred ledgers must cover 20 unique morph targets")

    expected_ids = [
        "hair_none", "hair_short", "hair_medium", "hair_mohawk",
        "facialhair_none", "facialhair_stubble", "facialhair_beard",
        "brow_default", "brow_alt", "scar_none", "scar_proxy",
        "tattoo_none", "tattoo_proxy", "voice_default", "voice_alt",
        "pronouns_default", "pronouns_they_them",
    ]
    items = spec.get("items", [])
    if [item.get("item_id") for item in items] != expected_ids:
        raise RuntimeError("Frozen 17-item stable-ID order changed")
    if len({item.get("data_asset") for item in items}) != 17:
        raise RuntimeError("Cosmetic data-asset paths must be unique")
    mesh_items = [item for item in items if item.get("mesh_kind") == "Static"]
    if len(mesh_items) != 7:
        raise RuntimeError("Exactly seven static proxy cosmetic meshes are required")
    if any(item.get("mesh_kind") not in {"None", "Static"} for item in items):
        raise RuntimeError("Catalog cosmetic entries may only be None or Static")
    if len({item["mesh_asset"] for item in mesh_items}) != 7:
        raise RuntimeError("Static cosmetic mesh paths must be unique")
    if len({item["source_fbx"] for item in mesh_items}) != 7:
        raise RuntimeError("Static cosmetic source FBX paths must be unique")
    for item in items:
        if not str(item["data_asset"]).startswith(
                "/Game/DiscGolf/Characters/Customization/Data/Items/"):
            raise RuntimeError(f"Cosmetic data asset escaped owned namespace: {item['item_id']}")
        if item["mesh_kind"] == "Static":
            source = Path(item["source_fbx"])
            if source.is_absolute() or ".." in source.parts \
                    or source.parts[0] != "FBX" or source.suffix.casefold() != ".fbx":
                raise RuntimeError(f"Unsafe source FBX path: {item['item_id']}")
            if not str(item["mesh_asset"]).startswith(
                    "/Game/DiscGolf/Characters/Customization/Cosmetics/"):
                raise RuntimeError(f"Cosmetic mesh escaped owned namespace: {item['item_id']}")
            if item.get("attach_socket") != "head":
                raise RuntimeError(f"Mesh-backed cosmetic must attach to head: {item['item_id']}")
        elif item.get("mesh_asset") or item.get("source_fbx"):
            raise RuntimeError(f"Non-mesh cosmetic unexpectedly names geometry: {item['item_id']}")

    expected_head_params = [
        "DG_SkinTone", "DG_EyeColor", "DG_Complexion", "DG_Freckles",
        "DG_SunExposure", "DG_ScarProxy", "DG_TattooProxy",
    ]
    if spec.get("head_material_parameters") != expected_head_params \
            or spec.get("hair_material_parameters") != ["DG_HairColor"]:
        raise RuntimeError("Material parameter contract changed")
    expected_defaults = {
        "DG_SkinTone": [0.55, 0.35, 0.24, 1.0],
        "DG_EyeColor": [0.15, 0.24, 0.20, 1.0],
        "DG_Complexion": 0.35,
        "DG_Freckles": 0.0,
        "DG_SunExposure": 0.25,
        "DG_ScarProxy": 0.0,
        "DG_TattooProxy": 0.0,
        "DG_HairColor": [0.05, 0.03, 0.02, 1.0],
    }
    if spec.get("material_defaults") != expected_defaults:
        raise RuntimeError("Material defaults changed")

    for path_key, hash_key in (
        ("master_source_blend", "master_source_blend_sha256"),
        ("master_source_fbx", "master_source_fbx_sha256"),
        ("skeleton_contract", "skeleton_contract_sha256"),
        ("buildkit_face_contract", "buildkit_face_contract_sha256"),
        ("buildkit_face_presets", "buildkit_face_presets_sha256"),
        ("prepared_generator_reference", "prepared_generator_reference_sha256"),
    ):
        _require_hash(project_root, spec[path_key], spec[hash_key])


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
    hierarchy = _expand_contract_hierarchy(contract)
    if len(hierarchy) != 69 or len(arm.data.bones) != 69:
        raise RuntimeError(f"Expected accepted 69-bone armature, found {len(arm.data.bones)}")
    expected_names = {name for name, _ in hierarchy}
    actual_names = {bone.name for bone in arm.data.bones}
    if actual_names != expected_names:
        raise RuntimeError(
            "Accepted master bone-name contract changed: "
            f"missing={sorted(expected_names - actual_names)} "
            f"unexpected={sorted(actual_names - expected_names)}")
    for name, expected_parent in hierarchy:
        actual = arm.data.bones[name].parent
        if (actual.name if actual else None) != expected_parent:
            raise RuntimeError(f"Accepted master parent differs: {name}")


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


def _ensure_material(name: str, color: tuple[float, float, float, float]):
    material = bpy.data.materials.get(name)
    if material is None:
        material = bpy.data.materials.new(name)
    material.diffuse_color = color
    return material


def _mark_non_production(block, item_id: str = "") -> None:
    block["DG_Session"] = 7
    block["DG_ContentStatus"] = "NON_PRODUCTION_PROXY"
    block["DG_ShippingStatus"] = "DO_NOT_SHIP"
    block["DG_BrandId"] = "dg_generic"
    block["DG_SourceKind"] = "PROJECT_LOCAL_GENERATED_BLOCKOUT"
    if item_id:
        block["DG_ItemId"] = item_id


def _move_to_collection(obj, collection) -> None:
    for owner in list(obj.users_collection):
        owner.objects.unlink(obj)
    collection.objects.link(obj)


def _box(collection, material, name, location, half_extents, rotation=(0.0, 0.0, 0.0),
         region_group: str = ""):
    bpy.ops.mesh.primitive_cube_add(size=2.0, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = f"{GENERATED_PREFIX}PART_{name}"
    obj.scale = half_extents
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    _move_to_collection(obj, collection)
    obj.data.materials.append(material)
    if region_group:
        group = obj.vertex_groups.new(name=region_group)
        group.add(list(range(len(obj.data.vertices))), 1.0, "REPLACE")
    return obj


def _eye(collection, material, name, location):
    bpy.ops.mesh.primitive_uv_sphere_add(
        segments=12, ring_count=8, location=location, scale=(0.029, 0.014, 0.020))
    obj = bpy.context.object
    obj.name = f"{GENERATED_PREFIX}PART_{name}"
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    _move_to_collection(obj, collection)
    obj.data.materials.append(material)
    group = obj.vertex_groups.new(name="DG_EYE_REGION")
    group.add(list(range(len(obj.data.vertices))), 1.0, "REPLACE")
    return obj


def _join_parts(parts, output_name, material):
    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = f"{GENERATED_PREFIX}{output_name}"
    obj.matrix_world.identity()
    obj.data.materials.clear()
    obj.data.materials.append(material)
    for polygon in obj.data.polygons:
        polygon.material_index = 0
    return obj


def _surface_masks(head) -> None:
    eye_group = head.vertex_groups.get("DG_EYE_REGION")
    eye_vertices = set()
    if eye_group:
        for vertex in head.data.vertices:
            if any(group.group == eye_group.index for group in vertex.groups):
                eye_vertices.add(vertex.index)
    color = head.data.color_attributes.new(
        name=SURFACE_MASK_ATTRIBUTE, type="BYTE_COLOR", domain="CORNER")
    for polygon in head.data.polygons:
        for loop_index in polygon.loop_indices:
            vertex_index = head.data.loops[loop_index].vertex_index
            co = head.data.vertices[vertex_index].co
            is_eye = vertex_index in eye_vertices
            freckles = (
                not is_eye and co.y < -0.105 and 1.665 < co.z < 1.725
                and 0.035 < abs(co.x) < 0.115 and vertex_index % 3 == 0)
            scar = (
                not is_eye and co.y < -0.105 and 0.015 < co.x < 0.065
                and 1.725 < co.z < 1.79)
            color.data[loop_index].color = (
                0.0 if is_eye else 1.0,
                1.0 if freckles else 0.0,
                1.0 if scar else 0.0,
                0.0,
            )
    if eye_group:
        head.vertex_groups.remove(eye_group)


def _shape(head, name: str, transform) -> None:
    key = head.shape_key_add(name=name)
    for index, value in enumerate(key.data):
        transform(value, head.data.vertices[index].co.copy())
    deltas = [(value.co - head.data.vertices[index].co).length
              for index, value in enumerate(key.data)]
    if max(deltas, default=0.0) < 0.002:
        raise RuntimeError(f"Visible morph has no reviewable deformation: {name}")


def _validate_enclosure_design() -> dict:
    # UE combines morph-target deltas additively.  Enumerate all 2^5 endpoint
    # combinations and use the conservative lower-jaw X factor for the full
    # shell.  This proves the envelope without incorrectly multiplying gains.
    extrema = []
    for width, height, cheek, jaw, chin in product((-1.0, 1.0), repeat=5):
        extrema.append((
            HEAD_HALF.x * (1.0 + width * HEAD_WIDTH_GAIN + jaw * JAW_GAIN),
            HEAD_HALF.y * (1.0 + cheek * CHEEK_GAIN),
            HEAD_HALF.z * (1.0 + height * HEAD_HEIGHT_GAIN)
            + chin * CHIN_SHIFT,
        ))
    minimum_x = min(extent[0] for extent in extrema)
    minimum_y = min(extent[1] for extent in extrema)
    minimum_z = min(extent[2] for extent in extrema)
    required = (
        LEGACY_HEAD_HALF.x + MIN_CLEARANCE,
        LEGACY_HEAD_HALF.y + MIN_CLEARANCE,
        LEGACY_HEAD_HALF.z + MIN_CLEARANCE,
    )
    actual = (minimum_x, minimum_y, minimum_z)
    if any(value + 1.0e-6 < limit for value, limit in zip(actual, required)):
        raise RuntimeError(f"Proxy shell no longer encloses frozen head_geo: {actual} < {required}")
    return {
        "status": "PASS_CONSERVATIVE_SIMULTANEOUS_NEGATIVE_EXTREMES",
        "minimum_proxy_half_extents_m": list(actual),
        "required_half_extents_with_clearance_m": list(required),
        "supported_extremes": [-1.0, 1.0],
        "extreme_combination_count": len(extrema),
        "blendshape_combination_rule": "ADDITIVE_DELTAS",
    }


def _create_head(collection, arm, material, visible_morphs):
    bpy.ops.mesh.primitive_cube_add(
        size=2.0, location=(0.0, 0.0, HEAD_CENTER_Z), scale=HEAD_HALF)
    shell = bpy.context.object
    shell.name = f"{GENERATED_PREFIX}PART_HeadShell"
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    _move_to_collection(shell, collection)
    shell.data.materials.append(material)
    bevel = shell.modifiers.new(name="ProxyRound", type="BEVEL")
    bevel.width = 0.012
    bevel.segments = 3
    bpy.context.view_layer.objects.active = shell
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    shell_group = shell.vertex_groups.new(name="DG_HEAD_SHELL")
    shell_group.add(list(range(len(shell.data.vertices))), 1.0, "REPLACE")

    parts = [shell]
    parts.extend((
        _eye(collection, material, "EyeL", (0.050, -0.137, 1.725)),
        _eye(collection, material, "EyeR", (-0.050, -0.137, 1.725)),
        _box(collection, material, "Nose", (0.0, -0.153, 1.675),
             (0.025, 0.022, 0.045)),
        _box(collection, material, "Mouth", (0.0, -0.143, 1.615),
             (0.055, 0.010, 0.012)),
        _box(collection, material, "EarL", (0.148, 0.0, 1.69),
             (0.014, 0.030, 0.050)),
        _box(collection, material, "EarR", (-0.148, 0.0, 1.69),
             (0.014, 0.030, 0.050)),
    ))
    head = _join_parts(parts, "SK_DG_Head_Proxy", material)
    _surface_masks(head)
    group = head.vertex_groups.new(name="head")
    group.add(list(range(len(head.data.vertices))), 1.0, "REPLACE")
    head.shape_key_add(name="Basis")

    _shape(head, visible_morphs["head_width"],
           lambda value, base: setattr(
               value, "co", Vector((base.x * (1.0 + HEAD_WIDTH_GAIN), base.y, base.z))))
    _shape(head, visible_morphs["head_height"],
           lambda value, base: setattr(value, "co", Vector((
               base.x, base.y,
               HEAD_CENTER_Z + (base.z - HEAD_CENTER_Z) * (1.0 + HEAD_HEIGHT_GAIN)))))

    def cheek(value, base):
        z_weight = max(0.0, 1.0 - abs(base.z - 1.69) / 0.09)
        x_weight = max(0.0, 1.0 - abs(abs(base.x) - 0.085) / 0.075)
        front = max(0.0, min(1.0, (-base.y - 0.07) / 0.07))
        value.co = Vector((base.x, base.y * (1.0 + CHEEK_GAIN * z_weight * x_weight * front), base.z))

    def jaw(value, base):
        lower = max(0.0, min(1.0, (1.69 - base.z) / 0.14))
        value.co = Vector((base.x * (1.0 + JAW_GAIN * lower), base.y, base.z))

    def chin(value, base):
        lower = max(0.0, min(1.0, (1.64 - base.z) / 0.10))
        center = max(0.0, 1.0 - abs(base.x) / 0.10)
        value.co = Vector((base.x, base.y, base.z - CHIN_SHIFT * lower * center))

    _shape(head, visible_morphs["cheek_fullness"], cheek)
    _shape(head, visible_morphs["jaw_width"], jaw)
    _shape(head, visible_morphs["chin_length"], chin)

    modifier = head.modifiers.new(name="Armature", type="ARMATURE")
    modifier.object = arm
    head.parent = arm
    head.matrix_parent_inverse = arm.matrix_world.inverted()
    _mark_non_production(head)
    _mark_non_production(head.data)
    return head


def _validate_actual_head_shell(head, visible_morphs) -> dict:
    shell_group = head.vertex_groups.get("DG_HEAD_SHELL")
    if shell_group is None:
        raise RuntimeError("Generated head lost its shell vertex ledger")
    shell_indices = {
        vertex.index for vertex in head.data.vertices
        if any(group.group == shell_group.index for group in vertex.groups)
    }
    if not shell_indices:
        raise RuntimeError("Generated head shell vertex ledger is empty")
    key_blocks = head.data.shape_keys.key_blocks
    basis = key_blocks.get("Basis")
    morph_keys = [key_blocks.get(name) for name in visible_morphs.values()]
    if basis is None or any(key is None for key in morph_keys):
        raise RuntimeError("Generated head shell lacks its exact five morph keys")
    required_minimum = Vector((
        -(LEGACY_HEAD_HALF.x + MIN_CLEARANCE),
        -(LEGACY_HEAD_HALF.y + MIN_CLEARANCE),
        HEAD_CENTER_Z - (LEGACY_HEAD_HALF.z + MIN_CLEARANCE),
    ))
    required_maximum = Vector((
        LEGACY_HEAD_HALF.x + MIN_CLEARANCE,
        LEGACY_HEAD_HALF.y + MIN_CLEARANCE,
        HEAD_CENTER_Z + (LEGACY_HEAD_HALF.z + MIN_CLEARANCE),
    ))
    minimum_clearances = [float("inf"), float("inf"), float("inf")]
    for weights in product((-1.0, 1.0), repeat=5):
        points = []
        for index in shell_indices:
            base = basis.data[index].co
            coordinate = base.copy()
            for weight, key in zip(weights, morph_keys):
                coordinate += weight * (key.data[index].co - base)
            points.append(head.matrix_world @ coordinate)
        minimum = [min(point[axis] for point in points) for axis in range(3)]
        maximum = [max(point[axis] for point in points) for axis in range(3)]
        for axis in range(3):
            minimum_clearances[axis] = min(
                minimum_clearances[axis],
                float(required_minimum[axis] - minimum[axis]),
                float(maximum[axis] - required_maximum[axis]),
            )
        if any(minimum[axis] > required_minimum[axis] + 1.0e-6
               or maximum[axis] < required_maximum[axis] - 1.0e-6
               for axis in range(3)):
            raise RuntimeError(
                "Actual rounded head shell fails an additive morph endpoint enclosure: "
                f"weights={weights} minimum={minimum} maximum={maximum} "
                f"required_minimum={list(required_minimum)} "
                f"required_maximum={list(required_maximum)}")
    return {
        "status": "PASS_ACTUAL_SHELL_VERTEX_ENDPOINT_ENCLOSURE",
        "shell_vertex_count": len(shell_indices),
        "extreme_combination_count": 32,
        "minimum_axis_clearance_beyond_required_m": minimum_clearances,
    }


def _cosmetic_parts(item_id, collection, material):
    box = lambda label, loc, half, rotation=(0.0, 0.0, 0.0): _box(
        collection, material, f"{item_id}_{label}", loc, half, rotation)
    if item_id == "hair_short":
        return [
            box("top", (0.0, 0.015, 1.875), (0.172, 0.145, 0.038)),
            box("rear", (0.0, 0.125, 1.795), (0.172, 0.026, 0.080)),
            box("side_l", (0.155, 0.02, 1.81), (0.024, 0.105, 0.065)),
            box("side_r", (-0.155, 0.02, 1.81), (0.024, 0.105, 0.065)),
        ]
    if item_id == "hair_medium":
        return [
            box("top", (0.0, 0.020, 1.890), (0.158, 0.135, 0.045)),
            box("rear", (0.0, 0.145, 1.745), (0.162, 0.035, 0.155)),
            box("side_l", (0.155, 0.025, 1.745), (0.028, 0.120, 0.145)),
            box("side_r", (-0.155, 0.025, 1.745), (0.028, 0.120, 0.145)),
        ]
    if item_id == "hair_mohawk":
        return [
            box("ridge_front", (0.0, -0.075, 1.925), (0.032, 0.055, 0.095)),
            box("ridge_mid", (0.0, 0.015, 1.950), (0.034, 0.055, 0.115)),
            box("ridge_rear", (0.0, 0.105, 1.920), (0.032, 0.050, 0.090)),
        ]
    if item_id == "facialhair_stubble":
        return [
            box("chin", (0.0, -0.144, 1.575), (0.100, 0.010, 0.045)),
            box("jaw_l", (0.105, -0.105, 1.610), (0.040, 0.010, 0.045)),
            box("jaw_r", (-0.105, -0.105, 1.610), (0.040, 0.010, 0.045)),
        ]
    if item_id == "facialhair_beard":
        return [
            box("chin", (0.0, -0.157, 1.555), (0.110, 0.028, 0.080)),
            box("jaw_l", (0.110, -0.115, 1.610), (0.045, 0.025, 0.060)),
            box("jaw_r", (-0.110, -0.115, 1.610), (0.045, 0.025, 0.060)),
            box("mustache", (0.0, -0.158, 1.635), (0.060, 0.018, 0.012)),
        ]
    if item_id == "brow_default":
        return [
            box("left", (0.052, -0.154, 1.770), (0.047, 0.010, 0.010)),
            box("right", (-0.052, -0.154, 1.770), (0.047, 0.010, 0.010)),
        ]
    if item_id == "brow_alt":
        return [
            box("left", (0.052, -0.154, 1.775), (0.048, 0.010, 0.010),
                (0.0, 0.0, math.radians(12.0))),
            box("right", (-0.052, -0.154, 1.775), (0.048, 0.010, 0.010),
                (0.0, 0.0, math.radians(-12.0))),
        ]
    raise RuntimeError(f"No generic geometry recipe for {item_id}")


def _create_static_cosmetic(item, collection, arm, material):
    parts = _cosmetic_parts(item["item_id"], collection, material)
    output_name = Path(item["source_fbx"]).stem
    obj = _join_parts(parts, output_name, material)
    if obj.vertex_groups:
        raise RuntimeError(f"Static cosmetic unexpectedly has vertex groups: {item['item_id']}")
    bone = arm.data.bones.get("head")
    if bone is None:
        raise RuntimeError("Accepted armature has no head bone")
    # Geometry was authored in accepted armature space.  Convert vertices to
    # exact head-rest-local space so data assets can use Identity and runtime
    # can follow translation/rotation while ignoring the legacy socket scale.
    obj.data.transform(bone.matrix_local.inverted())
    obj.data.update()
    _mark_non_production(obj, item["item_id"])
    _mark_non_production(obj.data, item["item_id"])
    return obj


def _validate_head_local_cosmetic_design(cosmetics, arm) -> dict:
    bone = arm.data.bones.get("head")
    if bone is None:
        raise RuntimeError("Accepted armature has no head bone for seam validation")
    bounds = {}
    for item_id, obj in cosmetics.items():
        points = [bone.matrix_local @ vertex.co for vertex in obj.data.vertices]
        minimum = [min(point[axis] for point in points) for axis in range(3)]
        maximum = [max(point[axis] for point in points) for axis in range(3)]
        bounds[item_id] = {"minimum_m": minimum, "maximum_m": maximum}

    maximum_upper_head_x = HEAD_HALF.x * (1.0 + HEAD_WIDTH_GAIN)
    maximum_head_top = HEAD_CENTER_Z + HEAD_HALF.z * (1.0 + HEAD_HEIGHT_GAIN)
    for item_id in ("hair_short", "hair_medium"):
        minimum = bounds[item_id]["minimum_m"]
        maximum = bounds[item_id]["maximum_m"]
        if minimum[0] > -maximum_upper_head_x or maximum[0] < maximum_upper_head_x \
                or maximum[2] < maximum_head_top \
                or minimum[2] > maximum_head_top - 0.02:
            raise RuntimeError(
                f"{item_id} no longer overlaps the proxy shell at supported head extremes: "
                f"minimum={minimum} maximum={maximum} "
                f"required_half_x={maximum_upper_head_x} required_top={maximum_head_top}")
    mohawk = bounds["hair_mohawk"]
    if mohawk["minimum_m"][2] >= maximum_head_top \
            or mohawk["maximum_m"][2] <= maximum_head_top:
        raise RuntimeError("Mohawk root no longer intersects the supported head-top envelope")
    return {
        "status": "PASS_HEAD_LOCAL_OVERLAP_AT_SUPPORTED_VISIBLE_EXTREMES",
        "maximum_upper_head_half_width_m": maximum_upper_head_x,
        "maximum_head_top_m": maximum_head_top,
        "rest_armature_space_bounds": bounds,
        "hat_hair_policy":
            "HAT_HAIR_COVERAGE_HIDES_COMPONENT;STORED_HAIR_ID_UNCHANGED;REMOVE_RESTORES",
    }


def _export_fbx(asset, arm, path: Path) -> None:
    # Blender's stock FBX exporter otherwise writes wall-clock timestamps and
    # derives FBX IDs from Python's per-process randomized string hash.  The
    # pinned seed plus fixed header time make byte hashes repeatable, not just
    # the geometry recipe.  Reset exporter UUID maps so each file is also
    # independent of which other proxy files were exported earlier.
    if os.environ.get("PYTHONHASHSEED") != "0":
        raise RuntimeError(
            "Deterministic FBX export requires PYTHONHASHSEED=0 in Blender's "
            "launch environment")
    from io_scene_fbx import export_fbx_bin, fbx_utils
    original_header = export_fbx_bin.fbx_header_elements
    fixed_time = datetime.datetime(2000, 1, 1, 0, 0, 0)

    def fixed_header(root, scene_data, time=None):
        del time
        return original_header(root, scene_data, fixed_time)

    fbx_utils._keys_to_uuids.clear()
    fbx_utils._uuids_to_keys.clear()
    export_fbx_bin.fbx_header_elements = fixed_header
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    if arm:
        arm.select_set(True)
    asset.select_set(True)
    bpy.context.view_layer.objects.active = arm or asset
    try:
        bpy.ops.export_scene.fbx(
            filepath=str(path), use_selection=True,
            object_types={"ARMATURE", "MESH"} if arm else {"MESH"},
            apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
            use_space_transform=True, bake_space_transform=False,
            use_mesh_modifiers=True, add_leaf_bones=False, bake_anim=False,
            axis_forward="-Y", axis_up="Z", primary_bone_axis="Y",
            secondary_bone_axis="X")
    finally:
        export_fbx_bin.fbx_header_elements = original_header


def _world_bounds(mesh) -> list[float]:
    corners = [mesh.matrix_world @ Vector(corner) for corner in mesh.bound_box]
    return [max(point[i] for point in corners) - min(point[i] for point in corners)
            for i in range(3)]


def _roundtrip(path: Path, expected_skeletal: bool, expected_morphs: list[str],
               expected_bones: list[str]) -> dict:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path), automatic_bone_orientation=False)
    meshes = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    arms = [obj for obj in bpy.data.objects if obj.type == "ARMATURE"]
    if len(meshes) != 1 or len(arms) != (1 if expected_skeletal else 0):
        raise RuntimeError(
            f"FBX round-trip object counts differ: {path} meshes={len(meshes)} arms={len(arms)}")
    mesh = meshes[0]
    if len(mesh.data.materials) != 1:
        raise RuntimeError(f"FBX lost its single source material slot: {path}")
    if len(mesh.data.vertices) == 0 or len(mesh.data.polygons) == 0:
        raise RuntimeError(f"FBX has no renderable geometry: {path}")
    morphs = []
    if mesh.data.shape_keys:
        morphs = [key.name for key in mesh.data.shape_keys.key_blocks if key.name != "Basis"]
    if morphs != expected_morphs:
        raise RuntimeError(f"FBX morph ledger differs: {path} {morphs} != {expected_morphs}")
    bone_names = [bone.name for bone in arms[0].data.bones] if arms else []
    if bone_names != expected_bones:
        raise RuntimeError(f"FBX skeleton ledger differs: {path}")
    if expected_skeletal:
        head_group = mesh.vertex_groups.get("head")
        if head_group is None:
            raise RuntimeError("Head FBX lost rigid head vertex group")
        for vertex in mesh.data.vertices:
            weights = [group for group in vertex.groups if group.group == head_group.index]
            if len(weights) != 1 or abs(float(weights[0].weight) - 1.0) > 1.0e-6:
                raise RuntimeError(f"Head FBX lost rigid unit head weight: vertex {vertex.index}")
    color_names = sorted(attribute.name for attribute in mesh.data.color_attributes)
    if expected_skeletal and SURFACE_MASK_ATTRIBUTE not in color_names:
        raise RuntimeError("Head FBX lost surface-mask vertex colors")
    bounds = _world_bounds(mesh)
    if not all(math.isfinite(value) and value > 0.0 for value in bounds):
        raise RuntimeError(f"FBX bounds invalid: {path} {bounds}")
    return {
        "status": "PASS_BLENDER_5_2_FBX_ROUNDTRIP",
        "mesh_count": len(meshes),
        "armature_count": len(arms),
        "bone_count": len(bone_names),
        "vertex_count": len(mesh.data.vertices),
        "polygon_count": len(mesh.data.polygons),
        "material_slot_count": len(mesh.data.materials),
        "vertex_color_attributes": color_names,
        "morph_targets": morphs,
        "bounds_blender_units": bounds,
    }


def _generated_outputs_are_current(project_root: Path, source_root: Path,
                                   spec: dict, manifest_path: Path,
                                   generator_path: Path) -> bool:
    """Return true only for the exact complete hash-validated generated set."""
    if not manifest_path.is_file():
        return False
    try:
        manifest = _load_json(manifest_path)
        if manifest.get("schema") != \
                "DiscGolfTour.Session7ProxyCustomizationSourceManifest.v1" \
                or manifest.get("status") != "PASS_GENERATED_SOURCE_FIXTURE" \
                or manifest.get("generator_sha256") != _sha256(generator_path) \
                or manifest.get("catalog_spec_sha256") != FROZEN_CATALOG_SPEC_SHA256 \
                or manifest.get("generation_idempotence_policy") != \
                "MANIFEST_HASH_GATED_ALREADY_CURRENT_NO_WRITE":
            return False
        blend_relative = "SourceArt/DiscGolf/Characters/Customization/Proxy/" \
            "DG_Session7_ProxyCustomization.blend"
        if manifest.get("blend_source") != blend_relative:
            return False
        blend = project_root / blend_relative
        if not blend.is_file() or blend.stat().st_size != manifest.get("blend_source_bytes") \
                or _sha256(blend) != manifest.get("blend_source_sha256"):
            return False
        expected_morphs = list(spec["visible_morphs"].values())
        head = manifest.get("head_output", {})
        head_path = source_root / spec["head_source_fbx"]
        if head.get("source_fbx") != spec["head_source_fbx"] \
                or not head_path.is_file() \
                or head_path.stat().st_size != head.get("bytes") \
                or _sha256(head_path) != head.get("sha256") \
                or head.get("roundtrip_validation", {}).get("morph_targets") != expected_morphs:
            return False
        mesh_items = [item for item in spec["items"] if item["mesh_kind"] == "Static"]
        records = manifest.get("fbx_outputs", [])
        if len(records) != len(mesh_items):
            return False
        for item, record in zip(mesh_items, records):
            path = source_root / item["source_fbx"]
            if record.get("item_id") != item["item_id"] \
                    or record.get("source_fbx") != item["source_fbx"] \
                    or not path.is_file() \
                    or path.stat().st_size != record.get("bytes") \
                    or _sha256(path) != record.get("sha256") \
                    or record.get("roundtrip_validation", {}).get("status") != \
                    "PASS_BLENDER_5_2_FBX_ROUNDTRIP":
                return False
        expected_files = {
            generator_path.resolve(),
            (source_root / "proxy_customization_catalog_spec.json").resolve(),
            manifest_path.resolve(), blend.resolve(), head_path.resolve(),
            *(source_root / item["source_fbx"] for item in mesh_items),
        }
        actual_files = {path.resolve() for path in source_root.rglob("*") if path.is_file()}
        return actual_files == {path.resolve() for path in expected_files}
    except (KeyError, OSError, TypeError, ValueError, json.JSONDecodeError):
        return False


def main() -> None:
    args = _parse_args()
    if tuple(bpy.app.version[:2]) != (5, 2):
        raise RuntimeError(
            f"Session 7 source is pinned to Blender 5.2, found {bpy.app.version_string}")
    project_root = args.project_root.resolve()
    source_root = project_root / "SourceArt/DiscGolf/Characters/Customization/Proxy"
    spec_path = source_root / "proxy_customization_catalog_spec.json"
    manifest_path = source_root / "proxy_customization_source_manifest.json"
    blend_path = source_root / "DG_Session7_ProxyCustomization.blend"
    generator_path = Path(__file__).resolve()
    if _sha256(spec_path) != FROZEN_CATALOG_SPEC_SHA256:
        raise RuntimeError(
            "Frozen Session 7 customization catalog spec hash changed; "
            "refusing generation")
    spec = _load_json(spec_path)
    _validate_spec(spec, project_root)
    enclosure = _validate_enclosure_design()

    if _generated_outputs_are_current(
            project_root, source_root, spec, manifest_path, generator_path):
        print(
            "SESSION 7 PROXY CUSTOMIZATION: "
            "PASS_ALREADY_GENERATED_NO_WRITE head=1 static=7 morphs=5")
        return

    master_blend = _require_hash(
        project_root, spec["master_source_blend"], spec["master_source_blend_sha256"])
    contract_path = _require_hash(
        project_root, spec["skeleton_contract"], spec["skeleton_contract_sha256"])
    contract = _load_json(contract_path)
    bpy.ops.wm.open_mainfile(filepath=str(master_blend))
    arm = bpy.data.objects.get("Armature")
    if not arm or arm.type != "ARMATURE":
        raise RuntimeError("Accepted master Armature object not found")
    _validate_armature(arm, contract)
    expected_bones = [bone.name for bone in arm.data.bones]

    collection = _remove_prior_outputs()
    head_material = _ensure_material(HEAD_SOURCE_MATERIAL, (0.55, 0.35, 0.24, 1.0))
    hair_material = _ensure_material(HAIR_SOURCE_MATERIAL, (0.05, 0.03, 0.02, 1.0))
    _mark_non_production(head_material)
    _mark_non_production(hair_material)
    head = _create_head(collection, arm, head_material, spec["visible_morphs"])
    actual_head_enclosure = _validate_actual_head_shell(head, spec["visible_morphs"])
    mesh_items = [item for item in spec["items"] if item["mesh_kind"] == "Static"]
    cosmetics = {
        item["item_id"]: _create_static_cosmetic(item, collection, arm, hair_material)
        for item in mesh_items
    }
    cosmetic_design = _validate_head_local_cosmetic_design(cosmetics, arm)

    # The source fixture owns exactly one .blend.  Disable Blender's numbered
    # backup sidecar and remove only the prior exact owned backup if an earlier
    # diagnostic regeneration produced one.
    bpy.context.preferences.filepaths.save_version = 0
    blend_backup = blend_path.with_name(blend_path.name + "1")
    if blend_backup.is_file():
        blend_backup.unlink()
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path), check_existing=False)
    head_fbx = source_root / spec["head_source_fbx"]
    _export_fbx(head, arm, head_fbx)
    for item in mesh_items:
        _export_fbx(cosmetics[item["item_id"]], None, source_root / item["source_fbx"])

    expected_morphs = list(spec["visible_morphs"].values())
    head_roundtrip = _roundtrip(head_fbx, True, expected_morphs, expected_bones)
    output_records = []
    for item in mesh_items:
        path = source_root / item["source_fbx"]
        output_records.append({
            "item_id": item["item_id"],
            "kind": item["kind"],
            "mesh_kind": item["mesh_kind"],
            "mesh_asset": item["mesh_asset"],
            "source_fbx": item["source_fbx"],
            "attach_socket": item["attach_socket"],
            "relative_attachment_transform": "Identity",
            "bytes": path.stat().st_size,
            "sha256": _sha256(path),
            "roundtrip_validation": _roundtrip(path, False, [], []),
        })

    manifest = {
        "schema": "DiscGolfTour.Session7ProxyCustomizationSourceManifest.v1",
        "status": "PASS_GENERATED_SOURCE_FIXTURE",
        "content_status": spec["content_status"],
        "shipping_status": spec["shipping_status"],
        "brand_id": spec["brand_id"],
        "brand_status": spec["brand_status"],
        "external_sources": [],
        "logos": [],
        "rights_status": spec["rights_status"],
        "production_license_claim": "NONE",
        "geometry_recipe_id": spec["geometry_recipe_id"],
        "deterministic_export_contract":
            "PYTHONHASHSEED_0;FBX_HEADER_2000_01_01;RESET_EXPORTER_UUID_MAPS",
        "generation_idempotence_policy":
            "MANIFEST_HASH_GATED_ALREADY_CURRENT_NO_WRITE",
        "blender_version": bpy.app.version_string,
        "generator": generator_path.relative_to(project_root).as_posix(),
        "generator_sha256": _sha256(generator_path),
        "catalog_spec": spec_path.relative_to(project_root).as_posix(),
        "catalog_spec_sha256": _sha256(spec_path),
        "blend_source": blend_path.relative_to(project_root).as_posix(),
        "blend_source_bytes": blend_path.stat().st_size,
        "blend_source_sha256": _sha256(blend_path),
        "master_source_blend": spec["master_source_blend"],
        "master_source_blend_sha256": spec["master_source_blend_sha256"],
        "master_source_fbx": spec["master_source_fbx"],
        "master_source_fbx_sha256": spec["master_source_fbx_sha256"],
        "skeleton_contract": spec["skeleton_contract"],
        "skeleton_contract_sha256": spec["skeleton_contract_sha256"],
        "bone_count": 69,
        "head_attachment_policy": spec["head_attachment_policy"],
        "head_skinning_policy": spec["head_skinning_policy"],
        "static_attachment_policy": spec["static_attachment_policy"],
        "legacy_head_policy": spec["legacy_head_policy"],
        "legacy_head_enclosure_validation": enclosure,
        "actual_head_shell_validation": actual_head_enclosure,
        "head_local_cosmetic_validation": cosmetic_design,
        "visible_morphs": spec["visible_morphs"],
        "deferred_visual_morphs": spec["deferred_visual_morphs"],
        "head_source_material": HEAD_SOURCE_MATERIAL,
        "hair_source_material": HAIR_SOURCE_MATERIAL,
        "head_output": {
            "mesh_asset": spec["head_mesh_asset"],
            "source_fbx": spec["head_source_fbx"],
            "bytes": head_fbx.stat().st_size,
            "sha256": _sha256(head_fbx),
            "roundtrip_validation": head_roundtrip,
        },
        "fbx_outputs": output_records,
        "static_mesh_count": len(output_records),
        "catalog_item_count": len(spec["items"]),
        "network_access": "NONE",
        "asset_acquisition": "NONE",
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(
        "SESSION 7 PROXY CUSTOMIZATION GENERATED: "
        f"head=1 static={len(output_records)} morphs={len(expected_morphs)} "
        f"manifest={manifest_path}")


if __name__ == "__main__":
    main()
