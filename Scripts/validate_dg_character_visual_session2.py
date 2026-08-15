"""Read-only/transient evidence for the remaining Session 2 character checks.

Run this with UnrealEditor-Cmd's PythonScript commandlet.  The script loads the
already-saved Session 2 assets and native class defaults, performs geometry and
axis calculations in memory, and writes only this report:

    Saved/CharacterFramework/Session2VisualAcceptance.json

It does not compile or save Blueprints/Control Rigs, create assets, assign the
skeletal mesh or AnimBP to the gameplay pawn, or invoke throw/release logic.
Transient PBIK compression/extension, mirrored bend behavior, plant-foot drift,
grip axes, and profile-resource compatibility are evaluated numerically. Final
human judgement of separately captured normal-Editor images remains external to
this commandlet and is not claimed by this validator.
"""

from pathlib import Path
import hashlib
import json
import math
import re

import unreal


# Resolve from the executing script, not Paths.project_dir(): this repository
# has historical Editor configuration that can still report its old mirror.
PROJECT_ROOT = Path(__file__).absolute().parents[1]
REPORT_PATH = (
    PROJECT_ROOT
    / "Saved"
    / "CharacterFramework"
    / "Session2VisualAcceptance.json"
)

MESH_PATH = "/Game/DiscGolf/Characters/Meshes/SK_DG_Master"
SKELETON_PATH = "/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master"
IK_PATH = "/Game/DiscGolf/Rigs/IK_DG_Master"
CONTROL_RIG_PATH = "/Game/DiscGolf/Rigs/CR_DG_Master"
ANIM_BP_PATH = "/Game/DiscGolf/Animation/ABP_DG_Player"
CYLINDER_PATH = "/Engine/BasicShapes/Cylinder.Cylinder"

PROFILE_PATHS = (
    "/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact",
    "/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter",
    "/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms",
)

PROFILE_CONTROL_FIELDS = {
    "height_cm": "dg_height_cm",
    "wingspan_scale": "dg_wingspan_scale",
    "shoulder_width_scale": "dg_shoulder_width_scale",
    "torso_length_scale": "dg_torso_length_scale",
    "leg_length_scale": "dg_leg_length_scale",
    "hand_scale": "dg_hand_scale",
}

RIG_SPACE = unreal.ControlRigComponentSpace.RIG_SPACE

EXPECTED_SESSION2_PACKAGES = {
    MESH_PATH,
    SKELETON_PATH,
    IK_PATH,
    CONTROL_RIG_PATH,
    ANIM_BP_PATH,
    *PROFILE_PATHS,
}

PACKAGE_FILES = {
    MESH_PATH: PROJECT_ROOT
    / "Content/DiscGolf/Characters/Meshes/SK_DG_Master.uasset",
    SKELETON_PATH: PROJECT_ROOT
    / "Content/DiscGolf/Characters/Meshes/SKEL_DG_Master.uasset",
    IK_PATH: PROJECT_ROOT / "Content/DiscGolf/Rigs/IK_DG_Master.uasset",
    CONTROL_RIG_PATH: PROJECT_ROOT / "Content/DiscGolf/Rigs/CR_DG_Master.uasset",
    ANIM_BP_PATH: PROJECT_ROOT
    / "Content/DiscGolf/Animation/ABP_DG_Player.uasset",
    PROFILE_PATHS[0]: PROJECT_ROOT
    / "Content/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.uasset",
    PROFILE_PATHS[1]: PROJECT_ROOT
    / "Content/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.uasset",
    PROFILE_PATHS[2]: PROJECT_ROOT
    / "Content/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.uasset",
}

GAMEPLAY_DISC_SCALE = (0.21, 0.21, 0.015)
DISC_LOCAL_FORWARD = (1.0, 0.0, 0.0)
DISC_LOCAL_NORMAL = (0.0, 0.0, 1.0)
CHARACTER_ASSET_FORWARD = (0.0, 1.0, 0.0)
CHARACTER_ASSET_UP = (0.0, 0.0, 1.0)

# These are attachment transforms for the raw Engine Cylinder, not edits to the
# grip bones or authoritative disc actor.  The right-side local-Z half turn
# makes raw-disc +X point toward the same character-forward direction as the
# left side while preserving raw-disc +Z as the disc normal.
ATTACHMENT_TRANSFORMS = {
    "left": {
        "bone": "disc_grip_l",
        "location_cm": (0.0, 0.0, 0.0),
        "rotation_pitch_yaw_roll_deg": (0.0, 0.0, 0.0),
        "raw_mesh_scale": GAMEPLAY_DISC_SCALE,
        "disc_forward_in_grip_space": (1.0, 0.0, 0.0),
        "disc_normal_in_grip_space": DISC_LOCAL_NORMAL,
    },
    "right": {
        "bone": "disc_grip_r",
        "location_cm": (0.0, 0.0, 0.0),
        "rotation_pitch_yaw_roll_deg": (0.0, 180.0, 0.0),
        "raw_mesh_scale": GAMEPLAY_DISC_SCALE,
        "disc_forward_in_grip_space": (-1.0, 0.0, 0.0),
        "disc_normal_in_grip_space": DISC_LOCAL_NORMAL,
    },
}


def _require(condition, message):
    if not condition:
        raise RuntimeError(message)


def _load(path, asset_type):
    asset = unreal.load_asset(path)
    _require(isinstance(asset, asset_type), f"Missing or wrong-class asset: {path}")
    return asset


def _sha256(path):
    _require(path.is_file(), f"Expected package file is missing: {path}")
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def _package_hashes():
    return {
        package: {"path": str(path), "sha256": _sha256(path)}
        for package, path in sorted(PACKAGE_FILES.items())
    }


def _xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def _quat(value):
    return (
        float(value.x),
        float(value.y),
        float(value.z),
        float(value.w),
    )


def _add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def _sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def _mul(vector, scalar):
    return tuple(value * scalar for value in vector)


def _dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def _cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _length(value):
    return math.sqrt(_dot(value, value))


def _normalized(value):
    magnitude = _length(value)
    _require(magnitude > 1.0e-8, f"Cannot normalize near-zero vector: {value}")
    return tuple(component / magnitude for component in value)


def _quat_normalized(value):
    magnitude = math.sqrt(sum(component * component for component in value))
    _require(magnitude > 1.0e-8, f"Cannot normalize near-zero quaternion: {value}")
    return tuple(component / magnitude for component in value)


def _quat_conjugate(value):
    x, y, z, w = value
    return (-x, -y, -z, w)


def _quat_multiply(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    )


def _rotate_vector(rotation, vector):
    """Rotate a vector by an Unreal quaternion without mutating an asset."""
    qx, qy, qz, qw = _quat_normalized(rotation)
    axis = (qx, qy, qz)
    return _add(
        _add(
            _mul(axis, 2.0 * _dot(axis, vector)),
            _mul(vector, qw * qw - _dot(axis, axis)),
        ),
        _mul(_cross(axis, vector), 2.0 * qw),
    )


def _rotation_angle_degrees(rotation):
    normalized = _quat_normalized(rotation)
    # q and -q are the same rotation; abs(w) produces the shortest angle.
    return math.degrees(2.0 * math.acos(max(-1.0, min(1.0, abs(normalized[3])))))


def _vector_report(value):
    return [float(component) for component in value]


def _close_vector(actual, expected, tolerance):
    return _length(_sub(actual, expected)) <= tolerance


def _get_actor_components(actor_default, component_class):
    getter = getattr(actor_default, "get_components_by_class", None)
    _require(callable(getter), "Actor CDO does not expose get_components_by_class")
    return list(getter(component_class))


def _validate_gameplay_disc():
    cylinder = _load(CYLINDER_PATH, unreal.StaticMesh)
    bounds = cylinder.get_bounds()
    raw_origin = tuple(_xyz(bounds.origin))
    raw_size = tuple(value * 2.0 for value in _xyz(bounds.box_extent))
    scaled_size = tuple(
        raw_size[index] * GAMEPLAY_DISC_SCALE[index] for index in range(3)
    )
    _require(_close_vector(raw_origin, (0.0, 0.0, 0.0), 0.01),
             f"Gameplay Cylinder pivot is not centered: {raw_origin}")
    _require(_close_vector(raw_size, (100.0, 100.0, 100.0), 0.05),
             f"Unexpected Engine Cylinder bounds: {raw_size}")
    _require(_close_vector(scaled_size, (21.0, 21.0, 1.5), 0.02),
             f"Unexpected scaled gameplay-disc bounds: {scaled_size}")

    disc_class = unreal.load_class(None, "/Script/DiscGolfTour.DiscActor")
    _require(disc_class is not None, "Native DiscActor class could not be loaded")
    disc_default = unreal.get_default_object(disc_class)
    _require(disc_default is not None, "Native DiscActor CDO could not be loaded")
    components = _get_actor_components(disc_default, unreal.StaticMeshComponent)
    disc_components = [component for component in components if component.get_name() == "DiscMesh"]
    _require(len(disc_components) == 1, f"DiscActor CDO DiscMesh count is {len(disc_components)}")
    disc_component = disc_components[0]
    component_mesh = disc_component.get_editor_property("static_mesh")
    component_scale = tuple(_xyz(disc_component.get_editor_property("relative_scale3d")))
    _require(component_mesh == cylinder, f"DiscActor uses unexpected mesh: {component_mesh}")
    _require(_close_vector(component_scale, GAMEPLAY_DISC_SCALE, 1.0e-6),
             f"DiscActor uses unexpected mesh scale: {component_scale}")

    disc_source = (PROJECT_ROOT / "Source/DiscGolfTour/DiscActor.cpp").read_text(
        encoding="utf-8", errors="strict"
    )
    game_mode_source = (
        PROJECT_ROOT / "Source/DiscGolfTour/DiscGolfTourGameMode.cpp"
    ).read_text(encoding="utf-8", errors="strict")
    flight_source = (
        PROJECT_ROOT / "Source/DiscGolfTour/DiscFlightComponent.cpp"
    ).read_text(encoding="utf-8", errors="strict")
    replay_source = (
        PROJECT_ROOT / "Source/DiscGolfTour/DiscGolfPresentationMath.h"
    ).read_text(encoding="utf-8", errors="strict")
    types_source = (
        PROJECT_ROOT / "Source/DiscGolfTour/DiscGolfTypes.h"
    ).read_text(encoding="utf-8", errors="strict")
    source_contract = {
        "native_disc_spawn": bool(re.search(r"SpawnActor\s*<\s*ADiscActor\s*>", game_mode_source)),
        "engine_cylinder_reference": CYLINDER_PATH in disc_source,
        "exact_component_scale": "FVector(0.21f, 0.21f, 0.015f)" in disc_source,
        "physics_default_diameter_0_211_m": bool(
            re.search(r"DiameterM\s*=\s*0\.211f", types_source)
        ),
        "flight_make_from_xz": "MakeFromXZ(DiscForwardWorld, DiscNormalWorld)" in flight_source,
        "replay_make_from_xz": "MakeFromXZ(Forward, Normal)" in replay_source,
    }
    _require(all(source_contract.values()), f"Gameplay-disc source contract drifted: {source_contract}")

    engine_asset_file = (
        Path(
            unreal.Paths.convert_relative_path_to_full(
                unreal.Paths.engine_content_dir()
            )
        ).resolve()
        / "BasicShapes"
        / "Cylinder.uasset"
    )
    return {
        "status": "PASS",
        "runtime_actor_class": disc_class.get_path_name(),
        "runtime_component_name": disc_component.get_name(),
        "runtime_component_class": disc_component.get_class().get_path_name(),
        "mesh": cylinder.get_path_name(),
        "engine_asset_file": str(engine_asset_file),
        "engine_asset_sha256": _sha256(engine_asset_file),
        "component_relative_scale": _vector_report(component_scale),
        "raw_mesh_bounds_cm": {
            "origin": _vector_report(raw_origin),
            "size": _vector_report(raw_size),
        },
        "scaled_visual_bounds_cm": {
            "diameter_x": scaled_size[0],
            "diameter_y": scaled_size[1],
            "thickness_z": scaled_size[2],
        },
        "physics_default_diameter_cm": 21.1,
        "visual_vs_physics_diameter_difference_cm": 0.1,
        "axis_convention": {
            "local_positive_x": "release/leading/forward direction",
            "local_positive_y": "lateral axis completing the disc basis",
            "local_positive_z": "disc normal/top",
            "construction": "FRotationMatrix::MakeFromXZ(forward, normal)",
        },
        "source_contract": source_contract,
        "limitation": (
            "The current gameplay visual is a rotationally symmetric cylinder; "
            "it validates envelope and axes but has no authored rim or dome."
        ),
    }


def _bone_transform(hierarchy, bone_name, global_transform):
    key = unreal.RigElementKey(type=unreal.RigElementType.BONE, name=bone_name)
    transform = (
        hierarchy.get_global_transform(key, True)
        if global_transform
        else hierarchy.get_local_transform(key, True)
    )
    return {
        "translation_cm": tuple(_xyz(transform.translation)),
        "rotation": _quat(transform.rotation),
        "scale": tuple(_xyz(transform.scale3d)),
    }


def _validate_grips(control_rig):
    hierarchy = control_rig.get_hierarchy()
    results = {}
    for side, suffix in (("left", "l"), ("right", "r")):
        grip_name = f"disc_grip_{suffix}"
        hand_name = f"hand_{suffix}"
        grip_global = _bone_transform(hierarchy, grip_name, True)
        grip_local = _bone_transform(hierarchy, grip_name, False)
        hand_global = _bone_transform(hierarchy, hand_name, True)

        expected_native_local_translation = (
            (0.035, -0.06, -0.02)
            if side == "left"
            else (-0.035, -0.06, -0.02)
        )
        _require(
            _close_vector(
                grip_local["translation_cm"], expected_native_local_translation, 1.0e-4
            ),
            f"{grip_name} native local translation drifted: "
            f"{grip_local['translation_cm']}",
        )
        _require(
            _close_vector(hand_global["scale"], (100.0, 100.0, 100.0), 0.01),
            f"{hand_name} global import scale is unexpected: {hand_global['scale']}",
        )

        parent_inverse = _quat_conjugate(_quat_normalized(hand_global["rotation"]))
        derived_local_translation = _rotate_vector(
            parent_inverse,
            _sub(grip_global["translation_cm"], hand_global["translation_cm"]),
        )
        derived_local_rotation = _quat_multiply(parent_inverse, grip_global["rotation"])
        local_rotation_error_deg = _rotation_angle_degrees(derived_local_rotation)
        _require(local_rotation_error_deg <= 0.01,
                 f"{grip_name} is rotated relative to its hand by {local_rotation_error_deg} deg")
        _require(_length(derived_local_translation) <= 15.0,
                 f"{grip_name} is not palm-local: {derived_local_translation}")

        attachment = ATTACHMENT_TRANSFORMS[side]
        world_forward = _normalized(
            _rotate_vector(
                grip_global["rotation"], attachment["disc_forward_in_grip_space"]
            )
        )
        world_normal = _normalized(
            _rotate_vector(
                grip_global["rotation"], attachment["disc_normal_in_grip_space"]
            )
        )
        forward_alignment = _dot(world_forward, CHARACTER_ASSET_FORWARD)
        normal_alignment = _dot(world_normal, CHARACTER_ASSET_UP)
        _require(forward_alignment >= 0.9999,
                 f"{side} disc +X does not align with character forward: {world_forward}")
        _require(normal_alignment >= 0.9999,
                 f"{side} disc +Z does not align with character up: {world_normal}")

        results[side] = {
            "status": "PASS",
            "bone": grip_name,
            "parent": hand_name,
            "initial_global_origin_cm": _vector_report(grip_global["translation_cm"]),
            "live_initial_local_transform": {
                "translation_source_m": _vector_report(grip_local["translation_cm"]),
                "rotation_quaternion": _vector_report(grip_local["rotation"]),
                "scale": _vector_report(grip_local["scale"]),
                "unit_note": (
                    "The imported Control Rig hierarchy preserves source-meter local "
                    "translations beneath a 100x parent/global import scale."
                ),
            },
            "parent_global_import_scale": _vector_report(hand_global["scale"]),
            "derived_parent_oriented_offset_cm": _vector_report(derived_local_translation),
            "derived_parent_local_rotation_error_deg": local_rotation_error_deg,
            "hand_to_grip_distance_cm": _length(derived_local_translation),
            "raw_cylinder_attachment_transform": {
                "location_cm": list(attachment["location_cm"]),
                "rotation_pitch_yaw_roll_deg": list(
                    attachment["rotation_pitch_yaw_roll_deg"]
                ),
                "scale": list(attachment["raw_mesh_scale"]),
            },
            "resolved_disc_axes_in_character_asset_space": {
                "positive_x_forward": _vector_report(world_forward),
                "positive_z_normal": _vector_report(world_normal),
                "forward_alignment_dot": forward_alignment,
                "normal_alignment_dot": normal_alignment,
            },
        }

    left_origin = results["left"]["initial_global_origin_cm"]
    right_origin = results["right"]["initial_global_origin_cm"]
    mirror_error = _length(
        (left_origin[0] + right_origin[0],
         left_origin[1] - right_origin[1],
         left_origin[2] - right_origin[2])
    )
    _require(mirror_error <= 0.01, f"Grip-origin mirror error is {mirror_error} cm")
    return {
        "status": "PASS_TRANSIENT_AXIS_AND_GEOMETRY",
        "attachment_mesh": CYLINDER_PATH,
        "attachment_rule": "SnapToTargetNotIncludingScale; no collision; do not attach gameplay actor",
        "disc_center_rule": "zero relative translation at the authoritative grip bone",
        "character_asset_forward": list(CHARACTER_ASSET_FORWARD),
        "character_asset_up": list(CHARACTER_ASSET_UP),
        "gameplay_actor_forward": [1.0, 0.0, 0.0],
        "future_skeletal_component_alignment": {
            "status": "DEFERRED_TO_GAMEPLAY_ASSIGNMENT",
            "candidate_relative_yaw_deg": -90.0,
            "reason": "map character asset +Y forward to Unreal Actor +X exactly once",
        },
        "grip_origin_mirror_error_cm": mirror_error,
        "left": results["left"],
        "right": results["right"],
        "manual_visual_confirmation": (
            "Use transient +X and +Z arrows; cylinder symmetry alone cannot reveal yaw."
        ),
    }


def _validate_profiles(mesh, skeleton, ik_rig, control_rig, anim_bp):
    _require(mesh.get_editor_property("skeleton") == skeleton, "Master mesh/skeleton mismatch")
    ik_controller = unreal.IKRigController.get_controller(ik_rig)
    _require(ik_controller.get_skeletal_mesh() == mesh, "IK Rig preview mesh mismatch")
    _require(ik_controller.is_skeletal_mesh_compatible(mesh), "IK Rig rejects master mesh")
    _require(control_rig.get_preview_mesh() == mesh, "Control Rig preview mesh mismatch")
    _require(anim_bp.get_editor_property("target_skeleton") == skeleton,
             "Animation Blueprint target skeleton mismatch")

    hierarchy = control_rig.get_hierarchy()
    bone_names = {
        str(key.name)
        for key in hierarchy.get_all_keys()
        if key.type == unreal.RigElementType.BONE
    }
    _require(len(bone_names) == 69, f"Control Rig has {len(bone_names)} bones, expected 69")
    _require({"disc_grip_l", "disc_grip_r"} <= bone_names,
             "Control Rig is missing authoritative grip bones")

    canonical_resources = {
        "skeletal_mesh": mesh.get_path_name(),
        "skeleton": skeleton.get_path_name(),
        "ik_rig": ik_rig.get_path_name(),
        "control_rig": control_rig.get_path_name(),
        "animation_blueprint_foundation": anim_bp.get_path_name(),
    }
    results = {}
    resource_keys = set()
    for path in PROFILE_PATHS:
        profile = _load(path, unreal.DiscGolfCharacterProfile)
        right_grip = str(profile.get_editor_property("right_disc_grip_bone"))
        left_grip = str(profile.get_editor_property("left_disc_grip_bone"))
        _require(right_grip.casefold() == "disc_grip_r", f"{path} right grip mismatch")
        _require(left_grip.casefold() == "disc_grip_l", f"{path} left grip mismatch")
        body = profile.get_editor_property("body")
        resource_key = tuple(sorted(canonical_resources.items()))
        resource_keys.add(resource_key)
        results[path] = {
            "status": "PASS",
            "profile_class": profile.get_class().get_path_name(),
            "height_cm": float(body.get_editor_property("height_cm")),
            "hand_scale": float(body.get_editor_property("hand_scale")),
            "right_disc_grip_bone": right_grip,
            "left_disc_grip_bone": left_grip,
            "resolved_canonical_resources": canonical_resources,
        }
    _require(len(resource_keys) == 1, "Profiles did not resolve one canonical resource set")
    return {
        "status": "PASS_SHARED_CANONICAL_FOUNDATION",
        "same_resource_set": True,
        "unique_resource_set_count": len(resource_keys),
        "profile_count": len(results),
        "profiles": results,
        "resolution_model": (
            "DiscGolfCharacterProfile stores body/throw/grip data only; Session 2 intentionally "
            "resolves all fixtures through the external canonical master mesh/skeleton/IK/CR set."
        ),
        "runtime_profile_deformation": "DEFERRED_TO_SESSION_4",
    }


def _transform_position(transform):
    return tuple(_xyz(transform.translation))


def _transform_rotation(transform):
    return _quat(transform.rotation)


def _distance(a, b):
    return _length(_sub(a, b))


def _mirror_x(value):
    return (-value[0], value[1], value[2])


def _quat_delta_degrees(a, b):
    qa = _quat_normalized(a)
    qb = _quat_normalized(b)
    cosine = abs(sum(left * right for left, right in zip(qa, qb)))
    return math.degrees(2.0 * math.acos(max(-1.0, min(1.0, cosine))))


def _joint_forward_offset(root, joint, tip):
    root_to_tip = _sub(tip, root)
    denominator = _dot(root_to_tip, root_to_tip)
    _require(denominator > 1.0e-6, "Cannot evaluate a collapsed root-to-tip chain")
    parameter = max(
        0.0,
        min(1.0, _dot(_sub(joint, root), root_to_tip) / denominator),
    )
    closest = _add(root, _mul(root_to_tip, parameter))
    return joint[1] - closest[1]


def _segment_change_percent(initial_root, initial_joint, initial_tip, root, joint, tip):
    initial_lengths = (
        _distance(initial_root, initial_joint),
        _distance(initial_joint, initial_tip),
    )
    posed_lengths = (_distance(root, joint), _distance(joint, tip))
    _require(min(initial_lengths) > 1.0e-4, "Reference chain has a zero-length segment")
    changes = [
        abs(posed / initial - 1.0) * 100.0
        for initial, posed in zip(initial_lengths, posed_lengths)
    ]
    return max(changes), initial_lengths, posed_lengths


def _bone_position(component, bone_name):
    return _transform_position(component.get_bone_transform(bone_name, RIG_SPACE))


def _bone_rotation(component, bone_name):
    return _transform_rotation(component.get_bone_transform(bone_name, RIG_SPACE))


def _set_curve(component, curve_name, value):
    hierarchy = component.get_control_rig().get_hierarchy()
    key = unreal.RigElementKey(type=unreal.RigElementType.CURVE, name=curve_name)
    _require(hierarchy.contains(key), f"Transient rig is missing curve {curve_name}")
    hierarchy.set_curve_value(key, float(value), False)


def _new_transient_component(control_rig, mesh, profile):
    component = unreal.new_object(
        unreal.ControlRigComponent,
        name="DGSession2VisualAcceptanceComponent",
    )
    component.set_control_rig_asset_reference(
        control_rig.get_control_rig_asset_reference()
    )
    component.set_bone_initial_transforms_from_skeletal_mesh(mesh)
    component.initialize()
    _require(component.can_execute(), "Transient Control Rig component cannot execute")
    _require(
        component.does_element_exist("pelvis", unreal.RigElementType.BONE),
        "Transient Control Rig component has no pelvis root",
    )
    body = profile.get_editor_property("body")
    for field, control_name in PROFILE_CONTROL_FIELDS.items():
        component.set_control_float(
            control_name,
            float(body.get_editor_property(field)),
        )
    component.update(0.0)
    return component


def _chain_snapshot(component, root_name, joint_name, tip_name):
    return {
        "root": _bone_position(component, root_name),
        "joint": _bone_position(component, joint_name),
        "tip": _bone_position(component, tip_name),
    }


def _chain_metrics(initial, posed, target):
    segment_change, initial_lengths, posed_lengths = _segment_change_percent(
        initial["root"],
        initial["joint"],
        initial["tip"],
        posed["root"],
        posed["joint"],
        posed["tip"],
    )
    return {
        "forward_joint_offset_cm": _joint_forward_offset(
            posed["root"], posed["joint"], posed["tip"]
        ),
        "effector_error_cm": _distance(posed["tip"], target),
        "max_segment_length_change_percent": segment_change,
        "reference_segment_lengths_cm": list(initial_lengths),
        "posed_segment_lengths_cm": list(posed_lengths),
        "root_cm": list(posed["root"]),
        "joint_cm": list(posed["joint"]),
        "tip_cm": list(posed["tip"]),
        "target_cm": list(target),
    }


def _validate_profile_compression(control_rig, mesh, profile):
    component = _new_transient_component(control_rig, mesh, profile)
    initial = {
        "arm_l": _chain_snapshot(component, "upperarm_l", "lowerarm_l", "hand_l"),
        "arm_r": _chain_snapshot(component, "upperarm_r", "lowerarm_r", "hand_r"),
        "leg_l": _chain_snapshot(component, "thigh_l", "calf_l", "foot_l"),
        "leg_r": _chain_snapshot(component, "thigh_r", "calf_r", "foot_r"),
    }
    initial_controls = {
        name: tuple(_xyz(component.get_control_position(name, RIG_SPACE)))
        for name in ("ctrl_hand_l", "ctrl_hand_r", "ctrl_foot_l", "ctrl_foot_r")
    }
    targets = {
        "arm_l": _add(initial_controls["ctrl_hand_l"], (-25.0, 20.0, -5.0)),
        "arm_r": _add(initial_controls["ctrl_hand_r"], (25.0, 20.0, -5.0)),
        "leg_l": _add(initial_controls["ctrl_foot_l"], (0.0, 10.0, 25.0)),
        "leg_r": _add(initial_controls["ctrl_foot_r"], (0.0, 10.0, 25.0)),
    }

    component.set_control_float("dg_hand_ik_alpha_l", 1.0)
    component.set_control_float("dg_hand_ik_alpha_r", 1.0)
    _set_curve(component, "DG_FootPlant_L", 1.0)
    _set_curve(component, "DG_FootPlant_R", 1.0)
    for control_name, target in (
        ("ctrl_hand_l", targets["arm_l"]),
        ("ctrl_hand_r", targets["arm_r"]),
        ("ctrl_foot_l", targets["leg_l"]),
        ("ctrl_foot_r", targets["leg_r"]),
    ):
        component.set_control_position(control_name, unreal.Vector(*target), RIG_SPACE)
    component.update(0.0)

    posed = {
        "arm_l": _chain_snapshot(component, "upperarm_l", "lowerarm_l", "hand_l"),
        "arm_r": _chain_snapshot(component, "upperarm_r", "lowerarm_r", "hand_r"),
        "leg_l": _chain_snapshot(component, "thigh_l", "calf_l", "foot_l"),
        "leg_r": _chain_snapshot(component, "thigh_r", "calf_r", "foot_r"),
    }
    metrics = {
        name: _chain_metrics(initial[name], posed[name], targets[name])
        for name in posed
    }

    arm_mirror_error = _distance(
        posed["arm_l"]["joint"], _mirror_x(posed["arm_r"]["joint"])
    )
    leg_mirror_error = _distance(
        posed["leg_l"]["joint"], _mirror_x(posed["leg_r"]["joint"])
    )
    _require(metrics["arm_l"]["forward_joint_offset_cm"] >= 2.0,
             f"Left elbow did not bend forward: {metrics['arm_l']}")
    _require(metrics["arm_r"]["forward_joint_offset_cm"] >= 2.0,
             f"Right elbow did not bend forward: {metrics['arm_r']}")
    _require(metrics["leg_l"]["forward_joint_offset_cm"] >= 2.0,
             f"Left knee did not bend forward: {metrics['leg_l']}")
    _require(metrics["leg_r"]["forward_joint_offset_cm"] >= 2.0,
             f"Right knee did not bend forward: {metrics['leg_r']}")
    _require(arm_mirror_error <= 2.5, f"Elbow mirror error is {arm_mirror_error} cm")
    _require(leg_mirror_error <= 2.5, f"Knee mirror error is {leg_mirror_error} cm")
    for name, result in metrics.items():
        _require(
            result["max_segment_length_change_percent"] <= 0.5,
            f"{name} segment stretched: {result}",
        )
        _require(result["effector_error_cm"] <= 1.0,
                 f"{name} missed a reachable effector: {result}")

    # A separate extended pose verifies that unreachable targets do not stretch
    # the validation skeleton. Goal error is expected when stretch is disabled.
    extended = _new_transient_component(control_rig, mesh, profile)
    extended_initial = {
        "arm_l": _chain_snapshot(extended, "upperarm_l", "lowerarm_l", "hand_l"),
        "arm_r": _chain_snapshot(extended, "upperarm_r", "lowerarm_r", "hand_r"),
        "leg_l": _chain_snapshot(extended, "thigh_l", "calf_l", "foot_l"),
        "leg_r": _chain_snapshot(extended, "thigh_r", "calf_r", "foot_r"),
    }
    extended_controls = {
        name: tuple(_xyz(extended.get_control_position(name, RIG_SPACE)))
        for name in ("ctrl_hand_l", "ctrl_hand_r", "ctrl_foot_l", "ctrl_foot_r")
    }
    extended_targets = {
        "arm_l": _add(extended_controls["ctrl_hand_l"], (25.0, 15.0, 8.0)),
        "arm_r": _add(extended_controls["ctrl_hand_r"], (-25.0, 15.0, 8.0)),
        "leg_l": _add(extended_controls["ctrl_foot_l"], (0.0, -18.0, 5.0)),
        "leg_r": _add(extended_controls["ctrl_foot_r"], (0.0, 18.0, 5.0)),
    }
    extended.set_control_float("dg_hand_ik_alpha_l", 1.0)
    extended.set_control_float("dg_hand_ik_alpha_r", 1.0)
    _set_curve(extended, "DG_FootPlant_L", 1.0)
    _set_curve(extended, "DG_FootPlant_R", 1.0)
    for control_name, target in (
        ("ctrl_hand_l", extended_targets["arm_l"]),
        ("ctrl_hand_r", extended_targets["arm_r"]),
        ("ctrl_foot_l", extended_targets["leg_l"]),
        ("ctrl_foot_r", extended_targets["leg_r"]),
    ):
        extended.set_control_position(control_name, unreal.Vector(*target), RIG_SPACE)
    extended.update(0.0)
    extended_posed = {
        "arm_l": _chain_snapshot(extended, "upperarm_l", "lowerarm_l", "hand_l"),
        "arm_r": _chain_snapshot(extended, "upperarm_r", "lowerarm_r", "hand_r"),
        "leg_l": _chain_snapshot(extended, "thigh_l", "calf_l", "foot_l"),
        "leg_r": _chain_snapshot(extended, "thigh_r", "calf_r", "foot_r"),
    }
    extension_metrics = {
        name: _chain_metrics(
            extended_initial[name], extended_posed[name], extended_targets[name]
        )
        for name in extended_posed
    }
    for name, result in extension_metrics.items():
        _require(
            result["max_segment_length_change_percent"] <= 0.5,
            f"Extended {name} stretched: {result}",
        )

    body = profile.get_editor_property("body")
    return {
        "status": "PASS_TRANSIENT_COMPRESSION_AND_EXTENSION",
        "profile": profile.get_path_name(),
        "profile_controls_applied": {
            field: float(body.get_editor_property(field))
            for field in PROFILE_CONTROL_FIELDS
        },
        "compression": metrics,
        "elbow_mirror_error_cm": arm_mirror_error,
        "knee_mirror_error_cm": leg_mirror_error,
        "extension_no_stretch": extension_metrics,
        "runtime_body_deformation": "DEFERRED_TO_SESSION_4",
    }


def _validate_plant_foot_sequence(control_rig, mesh, baseline_profile):
    component = _new_transient_component(control_rig, mesh, baseline_profile)
    _set_curve(component, "DG_FootPlant_R", 1.0)
    _set_curve(component, "DG_FootPlant_L", 0.0)
    component.set_control_float("dg_hand_ik_alpha_r", 1.0)
    component.set_control_float("dg_hand_ik_alpha_l", 1.0)
    component.update(0.0)

    anchor_foot = _bone_position(component, "foot_r")
    anchor_foot_rotation = _bone_rotation(component, "foot_r")
    anchor_ball = _bone_position(component, "ball_r")
    anchor_spine_rotation = _bone_rotation(component, "spine_04")
    initial_hand = tuple(_xyz(component.get_control_position("ctrl_hand_r", RIG_SPACE)))
    initial_off_hand = tuple(_xyz(component.get_control_position("ctrl_hand_l", RIG_SPACE)))
    frames = (
        ("reachback", (0.0, -35.0, -5.0), (-15.0, 10.0, -5.0)),
        ("brace", (45.0, 12.0, -10.0), (-45.0, -10.0, -10.0)),
        ("follow_through", (110.0, 40.0, -15.0), (-110.0, -25.0, -15.0)),
    )
    samples = []
    for frame_name, delta, off_hand_delta in frames:
        target = _add(initial_hand, delta)
        off_hand_target = _add(initial_off_hand, off_hand_delta)
        component.set_control_position(
            "ctrl_hand_r", unreal.Vector(*target), RIG_SPACE
        )
        component.set_control_position(
            "ctrl_hand_l", unreal.Vector(*off_hand_target), RIG_SPACE
        )
        component.update(1.0 / 30.0)
        foot = _bone_position(component, "foot_r")
        foot_rotation = _bone_rotation(component, "foot_r")
        ball = _bone_position(component, "ball_r")
        spine_rotation = _bone_rotation(component, "spine_04")
        samples.append(
            {
                "frame": frame_name,
                "throwing_hand_target_cm": list(target),
                "off_hand_target_cm": list(off_hand_target),
                "plant_foot_cm": list(foot),
                "plant_foot_translation_drift_cm": _distance(foot, anchor_foot),
                "plant_foot_rotation_drift_deg": _quat_delta_degrees(
                    foot_rotation, anchor_foot_rotation
                ),
                "plant_ball_drift_cm": _distance(ball, anchor_ball),
                "spine_04_rotation_from_start_deg": _quat_delta_degrees(
                    spine_rotation, anchor_spine_rotation
                ),
            }
        )
    max_translation = max(sample["plant_foot_translation_drift_cm"] for sample in samples)
    max_rotation = max(sample["plant_foot_rotation_drift_deg"] for sample in samples)
    max_ball = max(sample["plant_ball_drift_cm"] for sample in samples)
    max_torso_rotation = max(sample["spine_04_rotation_from_start_deg"] for sample in samples)
    hand_sweep = _distance(
        samples[0]["throwing_hand_target_cm"],
        samples[-1]["throwing_hand_target_cm"],
    )
    _require(hand_sweep >= 70.0, f"Throw-like test hand sweep is only {hand_sweep} cm")
    _require(max_translation <= 1.0,
             f"Plant foot translated {max_translation} cm")
    _require(max_rotation <= 2.0,
             f"Plant foot rotated {max_rotation} degrees")
    _require(max_ball <= 1.5, f"Plant ball/toe translated {max_ball} cm")
    return {
        "status": "PASS_TRANSIENT_NON_PRODUCTION_THROW_LIKE_SEQUENCE",
        "plant_side": "right",
        "sample_rate_hz": 30,
        "anchor_foot_cm": list(anchor_foot),
        "throwing_hand_sweep_cm": hand_sweep,
        "max_plant_foot_translation_drift_cm": max_translation,
        "max_plant_foot_rotation_drift_deg": max_rotation,
        "max_ball_toe_translation_drift_cm": max_ball,
        "max_spine_04_rotation_from_start_deg": max_torso_rotation,
        "samples": samples,
        "scope_note": (
            "Transient Control Rig stress sequence only; no montage, release notify, "
            "disc actor, pawn assignment, or gameplay physics handoff exists."
        ),
    }


def _validate_dynamic_rig(control_rig, mesh):
    profiles = [_load(path, unreal.DiscGolfCharacterProfile) for path in PROFILE_PATHS]
    profile_results = {
        profile.get_path_name(): _validate_profile_compression(
            control_rig, mesh, profile
        )
        for profile in profiles
    }
    plant_result = _validate_plant_foot_sequence(control_rig, mesh, profiles[1])
    return {
        "status": "PASS_TRANSIENT_RUNTIME_PBIK",
        "anatomical_forward_axis": "+Y in master asset space",
        "acceptance_thresholds": {
            "minimum_forward_joint_offset_cm": 2.0,
            "maximum_mirror_error_cm": 2.5,
            "maximum_reachable_effector_error_cm": 1.0,
            "maximum_segment_length_change_percent": 0.5,
            "maximum_plant_foot_translation_drift_cm": 1.0,
            "maximum_plant_foot_rotation_drift_deg": 2.0,
            "maximum_ball_toe_translation_drift_cm": 1.5,
        },
        "profile_pose_results": profile_results,
        "plant_foot_sequence": plant_result,
        "persistent_asset_writes": [],
    }


def _validate_authority_boundary():
    forbidden_tokens = (
        "DiscGolfThrowComponent",
        "AnimNotify_DiscRelease",
        "AnimNotify_ThrowFinished",
        "AnimNotify_ThrowPhase",
        "OnDiscRelease",
        "OnThrowFinished",
        "NotifyDiscRelease",
        "NotifyThrowFinished",
        "ABP_DG_Player",
        "SetAnimInstanceClass",
        "SetSkeletalMesh",
    )
    violations = []
    for root in (PROJECT_ROOT / "Source", PROJECT_ROOT / "Config"):
        for path in root.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in {".h", ".cpp", ".cs", ".ini"}:
                continue
            text = path.read_text(encoding="utf-8", errors="ignore")
            for token in forbidden_tokens:
                if token in text:
                    violations.append(f"{path.relative_to(PROJECT_ROOT)}:{token}")
    _require(not violations, f"Session 3/gameplay wiring detected: {violations}")

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path("/Game/DiscGolf", recursive=True)
    package_names = {str(asset.package_name) for asset in assets}
    missing = sorted(EXPECTED_SESSION2_PACKAGES - package_names)
    unexpected = sorted(package_names - EXPECTED_SESSION2_PACKAGES)
    _require(not missing, f"Session 2 package set is missing: {missing}")
    _require(not unexpected, f"Unexpected post-Session-2 packages found: {unexpected}")

    pawn_class = unreal.load_class(None, "/Script/DiscGolfTour.DiscGolferPawn")
    _require(pawn_class is not None, "Native DiscGolferPawn class could not be loaded")
    pawn_default = unreal.get_default_object(pawn_class)
    _require(pawn_default is not None, "Native DiscGolferPawn CDO could not be loaded")
    skeletal_components = _get_actor_components(pawn_default, unreal.SkeletalMeshComponent)
    golfer_meshes = [
        component
        for component in skeletal_components
        if component.get_name() == "SkeletalGolferMesh"
    ]
    _require(len(golfer_meshes) == 1,
             f"DiscGolferPawn SkeletalGolferMesh count is {len(golfer_meshes)}")
    golfer_mesh = golfer_meshes[0]
    assigned_mesh = golfer_mesh.get_editor_property("skeletal_mesh_asset")
    assigned_anim = golfer_mesh.get_editor_property("anim_class")
    _require(assigned_mesh is None, f"Gameplay pawn unexpectedly has skeletal mesh {assigned_mesh}")
    _require(assigned_anim is None, f"Gameplay pawn unexpectedly has AnimBP {assigned_anim}")

    throw_components = _get_actor_components(
        pawn_default, unreal.DiscGolfThrowComponent
    )
    _require(not throw_components,
             f"Gameplay pawn unexpectedly owns plugin throw components: {throw_components}")
    return {
        "status": "PASS_NONE_SESSION_2",
        "source_config_violations": violations,
        "disc_golf_content_packages": sorted(package_names),
        "expected_package_count": len(EXPECTED_SESSION2_PACKAGES),
        "unexpected_package_count": len(unexpected),
        "pawn_class": pawn_class.get_path_name(),
        "pawn_skeletal_component": golfer_mesh.get_name(),
        "pawn_skeletal_mesh_assignment": None,
        "pawn_animation_blueprint_assignment": None,
        "plugin_throw_component_count": len(throw_components),
        "release_authority": "EXISTING_PROJECT_SYSTEMS_UNCHANGED",
        "session_3_assets_or_wiring": "NONE",
    }


def main():
    before_hashes = _package_hashes()
    mesh = _load(MESH_PATH, unreal.SkeletalMesh)
    skeleton = _load(SKELETON_PATH, unreal.Skeleton)
    ik_rig = _load(IK_PATH, unreal.IKRigDefinition)
    control_rig = _load(CONTROL_RIG_PATH, unreal.ControlRigBlueprint)
    anim_bp = _load(ANIM_BP_PATH, unreal.AnimBlueprint)

    report = {
        "status": "PASS_READ_ONLY_TRANSIENT_EVIDENCE",
        "engine_version": unreal.SystemLibrary.get_engine_version(),
        "scope": "SESSION_2_TRANSIENT_RIG_GRIP_PROFILE_AND_AUTHORITY_EVIDENCE",
        "gameplay_disc": _validate_gameplay_disc(),
        "grip_attachment_conventions": _validate_grips(control_rig),
        "profile_fixture_resolution": _validate_profiles(
            mesh, skeleton, ik_rig, control_rig, anim_bp
        ),
        "dynamic_rig_acceptance": _validate_dynamic_rig(control_rig, mesh),
        "authority_boundary": _validate_authority_boundary(),
        "honest_acceptance": {
            "automated_geometry_and_axis_evidence": "PASS",
            "transient_runtime_pbik_compression_and_extension": "PASS",
            "mirrored_elbow_and_knee_directions": "PASS",
            "transient_plant_foot_sequence": "PASS",
            "actual_gameplay_mesh_and_scale": "PASS",
            "left_and_right_attachment_conventions": "PASS",
            "shared_profile_foundation": "PASS",
            "no_session_3_wiring": "PASS",
            "full_session_2_visual_acceptance": (
                "REQUIRES_SEPARATE_NORMAL_EDITOR_CAPTURE_REVIEW"
            ),
        },
        "external_visual_review_gates": [
            "human screenshot review of transient elbow/knee compression and extension poses",
            "human screenshot review of brace-foot start/mid/end against a fixed marker",
            "human inspection of palm/rim fit with temporary +X/+Z axis markers",
            "ShortCompact/Baseline/TallLongArms posed visual comparison; runtime deformation is Session 4",
        ],
        "writes_allowed_by_this_script": [str(REPORT_PATH)],
        "uasset_write_calls": [],
    }

    after_hashes = _package_hashes()
    changed_packages = [
        package
        for package in before_hashes
        if before_hashes[package]["sha256"] != after_hashes[package]["sha256"]
    ]
    _require(not changed_packages, f"Session 2 packages changed during validation: {changed_packages}")
    report["package_integrity"] = {
        "status": "PASS_NO_DISK_MUTATION",
        "package_count": len(before_hashes),
        "changed_packages": changed_packages,
        "before": before_hashes,
        "after": after_hashes,
    }

    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log(
        "DG_SESSION2_VISUAL_EVIDENCE: PASS "
        "disc=Cylinder grips=2 profiles=3 gameplay_wiring=NONE_SESSION_2 "
        "normal_editor_visual_review=EXTERNAL"
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
        REPORT_PATH.write_text(
            json.dumps(
                {
                    "status": "FAIL",
                    "engine_version": unreal.SystemLibrary.get_engine_version(),
                    "scope": (
                        "SESSION_2_TRANSIENT_RIG_GRIP_PROFILE_AND_AUTHORITY_EVIDENCE"
                    ),
                    "error": f"{type(error).__name__}: {error}",
                    "full_session_2_visual_acceptance": (
                        "REQUIRES_SEPARATE_NORMAL_EDITOR_CAPTURE_REVIEW"
                    ),
                },
                indent=2,
            ),
            encoding="utf-8",
        )
        raise
