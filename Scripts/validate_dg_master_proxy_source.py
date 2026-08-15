"""Validate the generated Blender proxy source against the v1.5 contract.

Run with Blender's Python module (``bpy``), not Unreal's bundled Python. The
script is read-only with respect to the .blend/FBX and writes one Saved report.
"""

from pathlib import Path
import json
import math

import bpy


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BLEND_PATH = PROJECT_ROOT / "SourceArt" / "DiscGolf" / "Characters" / "SK_DG_Master_Proxy.blend"
CONTRACT_PATH = (
    PROJECT_ROOT
    / "_BuildKit"
    / "DiscGolfCorePlayabilityKit_v1.5"
    / "Config"
    / "DG_MasterSkeletonContract.json"
)
REPORT_PATH = PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session2ProxySourceValidation.json"


def _require(condition, message):
    if not condition:
        raise RuntimeError(message)


def _vec(vector):
    return [float(vector.x), float(vector.y), float(vector.z)]


def _distance(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def _hierarchy_from_contract(contract):
    hierarchy = [(name, parent) for name, parent in contract["hierarchy"]]
    finger = contract["finger_pattern"]
    for side in ("l", "r"):
        for finger_name in finger["each_hand"]:
            parent = f"hand_{side}"
            for segment in range(1, int(finger["segments"]) + 1):
                name = finger["name_template"].format(
                    finger=finger_name, segment=segment, side=side
                )
                hierarchy.append((name, parent))
                parent = name
    return hierarchy


def main():
    _require(BLEND_PATH.is_file(), f"Missing generated Blender source: {BLEND_PATH}")
    _require(CONTRACT_PATH.is_file(), f"Missing v1.5 skeleton contract: {CONTRACT_PATH}")
    contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
    expected_hierarchy = _hierarchy_from_contract(contract)
    expected_names = {name for name, _parent in expected_hierarchy}

    bpy.ops.wm.open_mainfile(filepath=str(BLEND_PATH))
    armatures = [obj for obj in bpy.data.objects if obj.type == "ARMATURE"]
    meshes = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    _require(len(armatures) == 1, f"Expected one armature object, found {len(armatures)}")
    _require(len(meshes) == 1, f"Expected one proxy mesh object, found {len(meshes)}")
    armature = armatures[0]
    mesh = meshes[0]
    bones = armature.data.bones
    names = {bone.name for bone in bones}
    _require(len(bones) == 69, f"Expected 69 bones, found {len(bones)}")
    _require(names == expected_names, "Blender source bone set differs from the v1.5 contract")

    parent_mismatches = []
    for name, expected_parent in expected_hierarchy:
        bone = bones[name]
        actual_parent = bone.parent.name if bone.parent else None
        if actual_parent != expected_parent:
            parent_mismatches.append(
                {"bone": name, "expected": expected_parent, "actual": actual_parent}
            )
    _require(not parent_mismatches, f"Blender hierarchy mismatch: {parent_mismatches}")

    helper_names = {
        name
        for name in names
        if name == "root" or name.startswith("ik_") or name.startswith("disc_grip_")
    }
    helper_deform = {name: bool(bones[name].use_deform) for name in sorted(helper_names)}
    _require(
        not any(helper_deform.values()),
        f"Helper/grip bones unexpectedly deform the mesh: {helper_deform}",
    )
    vertex_groups = {group.name for group in mesh.vertex_groups}
    weighted_helpers = sorted(vertex_groups & helper_names)
    _require(not weighted_helpers, f"Helper bones have proxy vertex groups: {weighted_helpers}")

    grips = {}
    for side in ("l", "r"):
        bone = bones[f"disc_grip_{side}"]
        direction = (bone.tail_local - bone.head_local).normalized()
        grips[side] = {
            "parent": bone.parent.name,
            "head_m": _vec(bone.head_local),
            "tail_m": _vec(bone.tail_local),
            "primary_axis_world": _vec(direction),
            "length_cm": float((bone.tail_local - bone.head_local).length * 100.0),
            "deform": bool(bone.use_deform),
        }
    left = grips["l"]["head_m"]
    right = grips["r"]["head_m"]
    mirrored_error_cm = _distance(left, [-right[0], right[1], right[2]]) * 100.0
    _require(mirrored_error_cm <= 0.01, f"Disc grip origins are not mirrored: {mirrored_error_cm} cm")

    world_points = [mesh.matrix_world @ vertex.co for vertex in mesh.data.vertices]
    mins = [min(point[index] for point in world_points) for index in range(3)]
    maxs = [max(point[index] for point in world_points) for index in range(3)]
    bounds_cm = [(maxs[index] - mins[index]) * 100.0 for index in range(3)]

    report = {
        "status": "PASS",
        "blender_version": bpy.app.version_string,
        "source_blend": str(BLEND_PATH),
        "contract": str(CONTRACT_PATH),
        "armature_object": armature.name,
        "armature_data": armature.data.name,
        "proxy_mesh": mesh.name,
        "bone_count": len(bones),
        "parent_relationships_validated": len(expected_hierarchy),
        "root_bones": [bone.name for bone in bones if bone.parent is None],
        "helper_deform_flags": helper_deform,
        "weighted_helper_vertex_groups": weighted_helpers,
        "vertex_group_count": len(vertex_groups),
        "disc_grips": {
            "convention": (
                "origin at palm center; primary bone axis points palm-outward "
                "toward the fingertips in the neutral T-pose"
            ),
            "left": grips["l"],
            "right": grips["r"],
            "mirror_error_cm": mirrored_error_cm,
        },
        "visible_mesh_bounds_cm": {"x": bounds_cm[0], "y": bounds_cm[1], "z": bounds_cm[2]},
        "limitations": [
            "blocky rigid-weighted validation proxy, not final production character art",
            "visual mesh is about 171.25 cm tall while the head bone reaches about 179 cm",
        ],
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(
        "DG_SESSION2_PROXY_SOURCE: PASS "
        f"bones={len(bones)} helpers_unweighted={len(weighted_helpers) == 0} "
        f"mirror_error_cm={mirrored_error_cm:.6f}"
    )


if __name__ == "__main__":
    main()
