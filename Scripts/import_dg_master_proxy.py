"""Import and validate the v1.5 DG master proxy skeletal mesh.

Run from UnrealEditor-Cmd with ``-run=pythonscript``.  This script owns only
the project-authored /Game/DiscGolf character foundation and deliberately does
not wire animation release events or gameplay physics.
"""

from pathlib import Path
import json

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
SOURCE_FBX = PROJECT_ROOT / "SourceArt" / "DiscGolf" / "Characters" / "SK_DG_Master_Proxy.fbx"
MESH_FOLDER = "/Game/DiscGolf/Characters/Meshes"
MESH_PATH = f"{MESH_FOLDER}/SK_DG_Master"
SKELETON_PATH = f"{MESH_FOLDER}/SKEL_DG_Master"
REPORT_PATH = PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session2MasterImport.json"

HIERARCHY = [
    ("root", None),
    ("pelvis", "root"),
    ("spine_01", "pelvis"),
    ("spine_02", "spine_01"),
    ("spine_03", "spine_02"),
    ("spine_04", "spine_03"),
    ("neck_01", "spine_04"),
    ("head", "neck_01"),
    ("clavicle_l", "spine_04"),
    ("upperarm_l", "clavicle_l"),
    ("lowerarm_l", "upperarm_l"),
    ("hand_l", "lowerarm_l"),
    ("upperarm_twist_l", "upperarm_l"),
    ("lowerarm_twist_l", "lowerarm_l"),
    ("clavicle_r", "spine_04"),
    ("upperarm_r", "clavicle_r"),
    ("lowerarm_r", "upperarm_r"),
    ("hand_r", "lowerarm_r"),
    ("upperarm_twist_r", "upperarm_r"),
    ("lowerarm_twist_r", "lowerarm_r"),
    ("thigh_l", "pelvis"),
    ("calf_l", "thigh_l"),
    ("foot_l", "calf_l"),
    ("ball_l", "foot_l"),
    ("thigh_twist_l", "thigh_l"),
    ("thigh_r", "pelvis"),
    ("calf_r", "thigh_r"),
    ("foot_r", "calf_r"),
    ("ball_r", "foot_r"),
    ("thigh_twist_r", "thigh_r"),
    ("disc_grip_l", "hand_l"),
    ("disc_grip_r", "hand_r"),
    ("ik_foot_root", "root"),
    ("ik_foot_l", "ik_foot_root"),
    ("ik_foot_r", "ik_foot_root"),
    ("ik_hand_root", "root"),
    ("ik_hand_gun", "ik_hand_root"),
    ("ik_hand_l", "ik_hand_gun"),
    ("ik_hand_r", "ik_hand_gun"),
]

for side in ("l", "r"):
    for finger in ("thumb", "index", "middle", "ring", "pinky"):
        parent = f"hand_{side}"
        for segment in (1, 2, 3):
            bone_name = f"{finger}_{segment:02d}_{side}"
            HIERARCHY.append((bone_name, parent))
            parent = bone_name


def _log(message):
    unreal.log(f"DG_SESSION2_IMPORT: {message}")


def _ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"Could not create content folder: {path}")


def _import_mesh():
    if unreal.EditorAssetLibrary.does_asset_exist(MESH_PATH):
        return unreal.load_asset(MESH_PATH), []
    if not SOURCE_FBX.is_file():
        raise RuntimeError(f"Generated proxy FBX is missing: {SOURCE_FBX}")

    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property("original_import_type", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("create_physics_asset", False)

    mesh_options = options.get_editor_property("skeletal_mesh_import_data")
    mesh_options.set_editor_property("convert_scene", True)
    mesh_options.set_editor_property("convert_scene_unit", True)
    mesh_options.set_editor_property("import_meshes_in_bone_hierarchy", False)
    mesh_options.set_editor_property("preserve_smoothing_groups", True)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(SOURCE_FBX))
    task.set_editor_property("destination_path", MESH_FOLDER)
    task.set_editor_property("destination_name", "SK_DG_Master")
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("factory", unreal.FbxFactory())
    task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    imported_paths = [str(path) for path in task.get_editor_property("imported_object_paths")]
    mesh = unreal.load_asset(MESH_PATH)
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError(f"FBX import did not create the expected SkeletalMesh: {imported_paths}")
    return mesh, imported_paths


def _normalize_skeleton_name(mesh):
    skeleton = mesh.get_editor_property("skeleton")
    if not isinstance(skeleton, unreal.Skeleton):
        raise RuntimeError("SK_DG_Master has no Skeleton asset")
    current_path = skeleton.get_path_name().split(".", 1)[0]
    if current_path == SKELETON_PATH:
        return skeleton
    if unreal.EditorAssetLibrary.does_asset_exist(SKELETON_PATH):
        raise RuntimeError(
            f"Cannot rename imported skeleton {current_path}; destination already exists: {SKELETON_PATH}"
        )
    if not unreal.EditorAssetLibrary.rename_asset(current_path, SKELETON_PATH):
        raise RuntimeError(f"Could not rename imported skeleton {current_path} to {SKELETON_PATH}")
    skeleton = unreal.load_asset(SKELETON_PATH)
    if not isinstance(skeleton, unreal.Skeleton):
        raise RuntimeError(f"Renamed Skeleton did not load: {SKELETON_PATH}")
    return skeleton


def _validate(mesh, skeleton):
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    if subsystem is None:
        raise RuntimeError("SkeletalMeshEditorSubsystem is unavailable")
    names = [str(name) for name in skeleton.get_reference_pose().get_bone_names()]
    actual = set(names)
    required = {name for name, _ in HIERARCHY}
    missing = sorted(required - actual)
    unexpected = sorted(actual - required)
    bad_parents = []
    for name, expected_parent in HIERARCHY:
        if name not in actual:
            continue
        parent = str(subsystem.get_bone_parent(mesh, name))
        if parent == "None":
            parent = ""
        if parent != (expected_parent or ""):
            bad_parents.append(
                {"bone": name, "expected": expected_parent or "", "actual": parent}
            )

    bounds = mesh.get_bounds()
    size = bounds.box_extent * 2.0
    height_cm = float(size.z)
    if missing or unexpected or bad_parents:
        raise RuntimeError(
            "Master hierarchy invalid: "
            f"missing={missing}, unexpected={unexpected}, bad_parents={bad_parents}"
        )
    if len(names) != 69:
        raise RuntimeError(f"Expected exactly 69 bones, imported {len(names)}")
    # This intentionally blocky proxy has visible geometry from 5.25 cm to
    # 176.5 cm even though its head bone reaches 179 cm.  Gate gross unit/axis
    # mistakes here; exact 183 cm Baseline proportions are a Control Rig test,
    # not an import-scale rewrite of the shared skeleton.
    if not 165.0 <= height_cm <= 190.0:
        raise RuntimeError(f"Proxy height is not human-sized: {height_cm:.3f} cm")

    return {
        "bone_count": len(names),
        "required_count": len(HIERARCHY),
        "missing": missing,
        "unexpected": unexpected,
        "bad_parents": bad_parents,
        "root_bones": [
            name
            for name in names
            if str(subsystem.get_bone_parent(mesh, name)) in ("", "None")
        ],
        "bounds_cm": {
            "x": float(size.x),
            "y": float(size.y),
            "z": height_cm,
        },
        "disc_grip_parents": {"disc_grip_l": "hand_l", "disc_grip_r": "hand_r"},
    }


def main():
    for folder in (
        "/Game/DiscGolf",
        "/Game/DiscGolf/Characters",
        MESH_FOLDER,
        "/Game/DiscGolf/Characters/Profiles",
        "/Game/DiscGolf/Animation",
        "/Game/DiscGolf/Rigs",
        "/Game/DiscGolf/Tests",
    ):
        _ensure_folder(folder)

    mesh, imported_paths = _import_mesh()
    skeleton = _normalize_skeleton_name(mesh)
    # Existing validated assets are a no-op path. Save only packages dirtied by
    # first import or the one-time skeleton-name normalization.
    for asset in (skeleton, mesh):
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=True):
            raise RuntimeError(f"Could not persist imported asset: {asset.get_path_name()}")
    validation = _validate(mesh, skeleton)

    report = {
        "status": "PASS",
        "source_fbx": str(SOURCE_FBX),
        "mesh": MESH_PATH,
        "skeleton": SKELETON_PATH,
        "imported_paths": imported_paths,
        "validation": validation,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    _log(
        f"PASS mesh={MESH_PATH} skeleton={SKELETON_PATH} "
        f"bones={validation['bone_count']} height_cm={validation['bounds_cm']['z']:.3f}"
    )


if __name__ == "__main__":
    main()
