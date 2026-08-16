"""Strict, read-only acceptance validation for character-framework Session 2.

The validator loads the saved assets in a fresh UE 5.8 process and checks the
master hierarchy, IK/Control Rig topology, profile contract, AnimBP foundation,
and the explicit no-Session-3 authority boundary. It never assigns these assets
to the gameplay pawn and never invokes throw/release logic.
"""

from pathlib import Path
import hashlib
import json
import math

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
MESH_PATH = "/Game/DiscGolf/Characters/Meshes/SK_DG_Master"
SKELETON_PATH = "/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master"
IK_PATH = "/Game/DiscGolf/Rigs/IK_DG_Master"
CONTROL_RIG_PATH = "/Game/DiscGolf/Rigs/CR_DG_Master"
ANIM_BP_PATH = "/Game/DiscGolf/Animation/ABP_DG_Player"
REPORT_PATH = PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session2Validation.json"
CONTRACT_PATH = (
    PROJECT_ROOT
    / "_BuildKit"
    / "DiscGolfCorePlayabilityKit_v1.5"
    / "Config"
    / "DG_MasterSkeletonContract.json"
)

CURVES = {
    "DG_FootPlant_L",
    "DG_FootPlant_R",
    "DG_ReachbackAlpha",
    "DG_BraceAlpha",
    "DG_ReleaseApproachAlpha",
    "DG_FollowThroughAlpha",
}

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
            name = f"{finger}_{segment:02d}_{side}"
            HIERARCHY.append((name, parent))
            parent = name

EFFECTORS = {
    "hand_l_Goal": "hand_l",
    "hand_r_Goal": "hand_r",
    "foot_l_Goal": "foot_l",
    "foot_r_Goal": "foot_r",
}

BEND_BONE_SETTINGS = (
    ("lowerarm_l", (0.0, 0.0, -45.0)),
    ("lowerarm_r", (0.0, 0.0, 45.0)),
    ("calf_l", (45.0, 0.0, 0.0)),
    ("calf_r", (45.0, 0.0, 0.0)),
)

PBIK_ITERATIONS = 20
PBIK_SUB_ITERATIONS = 10

CHAINS = {
    "Root": ("root", "root", None),
    "Spine": ("spine_01", "spine_04", None),
    "Neck": ("neck_01", "head", None),
    "Arm_L": ("upperarm_l", "hand_l", "hand_l_Goal"),
    "Arm_R": ("upperarm_r", "hand_r", "hand_r_Goal"),
    "Leg_L": ("thigh_l", "foot_l", "foot_l_Goal"),
    "Leg_R": ("thigh_r", "foot_r", "foot_r_Goal"),
    "Foot_L": ("foot_l", "ball_l", None),
    "Foot_R": ("foot_r", "ball_r", None),
}

PROFILE_SPECS = {
    "/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact": {
        "height_cm": 155.0,
        "wingspan_scale": 0.94,
        "shoulder_width_scale": 0.94,
        "torso_length_scale": 0.96,
        "leg_length_scale": 0.95,
        "hand_scale": 0.95,
    },
    "/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter": {
        "height_cm": 183.0,
        "wingspan_scale": 1.0,
        "shoulder_width_scale": 1.0,
        "torso_length_scale": 1.0,
        "leg_length_scale": 1.0,
        "hand_scale": 1.0,
    },
    "/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms": {
        "height_cm": 205.0,
        "wingspan_scale": 1.07,
        "shoulder_width_scale": 1.05,
        "torso_length_scale": 1.04,
        "leg_length_scale": 1.05,
        "hand_scale": 1.05,
    },
}

PROFILE_CONTROLS = {
    "dg_height_cm": 183.0,
    "dg_wingspan_scale": 1.0,
    "dg_shoulder_width_scale": 1.0,
    "dg_torso_length_scale": 1.0,
    "dg_leg_length_scale": 1.0,
    "dg_hand_scale": 1.0,
    "dg_hand_ik_alpha_l": 0.0,
    "dg_hand_ik_alpha_r": 0.0,
}


def _norm(value):
    if value is None:
        return None
    text = str(value).strip()
    if len(text) >= 2 and text[0] == text[-1] and text[0] in ("'", '"'):
        text = text[1:-1].strip()
    folded = text.casefold()
    return None if not folded or folded == "none" else folded


def _require(condition, message):
    if not condition:
        raise RuntimeError(message)


def _load(path, asset_type):
    asset = unreal.load_asset(path)
    _require(isinstance(asset, asset_type), f"Missing or wrong-class asset: {path}")
    return asset


def _vector(transform):
    value = transform.translation
    return [float(value.x), float(value.y), float(value.z)]


def _distance(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def _source_paths(pin):
    if pin is None:
        return []
    return sorted(str(value.get_pin_path()) for value in pin.get_linked_source_pins(False))


def _vector3(value):
    return [float(value.x), float(value.y), float(value.z)]


def _angles_match(actual, expected, tolerance=1.0e-5):
    return all(abs(component - target) <= tolerance for component, target in zip(actual, expected))


def _pin_float(pin, message):
    _require(pin is not None, message)
    try:
        return float(pin.get_default_value())
    except (TypeError, ValueError) as error:
        raise RuntimeError(f"{message}: {pin.get_default_value()!r}") from error


def _validate_master(mesh, skeleton):
    mesh_editor = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    _require(mesh_editor is not None, "SkeletalMeshEditorSubsystem is unavailable")
    _require(mesh.get_editor_property("skeleton") == skeleton, "Mesh/skeleton linkage mismatch")
    names = [str(name) for name in skeleton.get_reference_pose().get_bone_names()]
    expected = [name for name, _parent in HIERARCHY]
    _require(len(names) == 69, f"Expected exactly 69 master bones, found {len(names)}")
    _require(set(names) == set(expected), "Master bone-name set differs from the v1.5 contract")

    parents = {}
    root_bones = []
    for bone_name, expected_parent in HIERARCHY:
        actual_parent = str(mesh_editor.get_bone_parent(mesh, bone_name))
        actual_parent = None if _norm(actual_parent) is None else actual_parent
        parents[bone_name] = actual_parent
        _require(
            _norm(actual_parent) == _norm(expected_parent),
            f"Parent mismatch for {bone_name}: {actual_parent} != {expected_parent}",
        )
        if actual_parent is None:
            root_bones.append(bone_name)
    _require(root_bones == ["root"], f"Master skeleton roots are {root_bones}, expected root only")
    _require("Armature" not in names, "Unexpected Blender Armature wrapper bone imported")

    size = mesh.get_bounds().box_extent * 2.0
    height_cm = float(size.z)
    _require(165.0 <= height_cm <= 190.0, f"Proxy bounds height is implausible: {height_cm}")
    curves = {str(name) for name in skeleton.get_curve_meta_data_names()}
    _require(CURVES <= curves, f"Master skeleton is missing curves: {sorted(CURVES - curves)}")
    return {
        "bone_count": len(names),
        "root_bones": root_bones,
        "parents_validated": len(parents),
        "bounds_cm": {"x": float(size.x), "y": float(size.y), "z": height_cm},
        "registered_curves": sorted(curves),
        "required_curves": sorted(CURVES),
        "missing_curves": sorted(CURVES - curves),
        "extra_curves": sorted(curves - CURVES),
    }


def _validate_ik(ik_rig, mesh):
    controller = unreal.IKRigController.get_controller(ik_rig)
    _require(controller.get_skeletal_mesh() == mesh, "IK Rig preview mesh mismatch")
    _require(controller.is_skeletal_mesh_compatible(mesh), "IK Rig rejects the master mesh")
    _require(controller.get_num_solvers() == 1, "IK Rig must contain exactly one solver")
    _require(controller.get_solver_enabled(0), "IK Rig FBIK solver is disabled")
    solver = controller.get_solver_controller(0)
    _require(isinstance(solver, unreal.IKRigFBIKController), f"Wrong IK solver: {solver}")
    _require(_norm(controller.get_start_bone(0)) == "pelvis", "IK solver root is not pelvis")
    _require(_norm(controller.get_retarget_root()) == "pelvis", "Retarget root is not pelvis")
    _require(_norm(controller.get_root_motion_bone()) == "root", "Root-motion bone is not root")

    actual_goals = {
        str(goal.get_editor_property("goal_name")): str(goal.get_editor_property("bone_name"))
        for goal in controller.get_all_goals()
    }
    _require(
        {_norm(k): _norm(v) for k, v in actual_goals.items()}
        == {_norm(k): _norm(v) for k, v in EFFECTORS.items()},
        f"IK goals differ from contract: {actual_goals}",
    )
    for goal_name in EFFECTORS:
        _require(
            controller.is_goal_connected_to_solver(goal_name, 0),
            f"IK goal is disconnected from FBIK: {goal_name}",
        )
        goal_settings = solver.get_goal_settings(goal_name)
        _require(
            int(goal_settings.get_editor_property("chain_depth")) == 2,
            f"IK goal {goal_name} chain depth is not 2",
        )

    solver_settings = solver.get_solver_settings()
    _require(
        _norm(solver_settings.get_editor_property("root_bone")) == "pelvis",
        "IK FBIK settings root bone is not pelvis",
    )
    _require(
        solver_settings.get_editor_property("root_behavior") == unreal.PBIKRootBehavior.FREE,
        "IK FBIK root behavior is not Free",
    )
    _require(
        not bool(solver_settings.get_editor_property("allow_stretch")),
        "IK FBIK unexpectedly allows stretch",
    )
    _require(
        int(solver_settings.get_editor_property("iterations")) == PBIK_ITERATIONS,
        f"IK FBIK iterations changed from the Session 2 baseline of {PBIK_ITERATIONS}",
    )
    _require(
        int(solver_settings.get_editor_property("sub_iterations")) == PBIK_SUB_ITERATIONS,
        f"IK FBIK sub-iterations are not {PBIK_SUB_ITERATIONS}",
    )
    _require(
        abs(float(solver_settings.get_editor_property("global_pull_chain_alpha"))) <= 1.0e-6,
        "IK FBIK global pull-chain alpha is not zero",
    )

    expected_bones = {bone_name for bone_name, _angles in BEND_BONE_SETTINGS}
    bone_settings = {}
    for bone_name, expected_angles in BEND_BONE_SETTINGS:
        setting = solver.get_bone_settings(bone_name)
        _require(
            setting is not None
            and _norm(setting.get_editor_property("bone")) == _norm(bone_name),
            f"IK FBIK bone setting is unavailable for {bone_name}",
        )
        _require(
            bool(setting.get_editor_property("use_preferred_angles")),
            f"IK FBIK preferred angles are disabled for {bone_name}",
        )
        actual_angles = _vector3(setting.get_editor_property("preferred_angles"))
        _require(
            _angles_match(actual_angles, expected_angles),
            f"IK FBIK {bone_name} preferred angles {actual_angles} != {expected_angles}",
        )
        _require(
            abs(float(setting.get_editor_property("position_stiffness"))) <= 1.0e-6
            and abs(float(setting.get_editor_property("rotation_stiffness"))) <= 1.0e-6,
            f"IK FBIK {bone_name} unexpectedly changes stiffness",
        )
        for axis in ("x", "y", "z"):
            _require(
                setting.get_editor_property(axis) == unreal.PBIKLimitType.FREE,
                f"IK FBIK {bone_name} {axis.upper()} limit is not Free",
            )
            _require(
                abs(float(setting.get_editor_property(f"min_{axis}"))) <= 1.0e-6
                and abs(float(setting.get_editor_property(f"max_{axis}"))) <= 1.0e-6,
                f"IK FBIK {bone_name} {axis.upper()} limit range is not zero/default",
            )
        bone_settings[bone_name] = {
            "use_preferred_angles": True,
            "preferred_angles": actual_angles,
            "position_stiffness": float(
                setting.get_editor_property("position_stiffness")
            ),
            "rotation_stiffness": float(
                setting.get_editor_property("rotation_stiffness")
            ),
            "limits": {axis: "Free" for axis in ("x", "y", "z")},
        }
    _require(set(bone_settings) == expected_bones, "IK FBIK bend-setting set is incomplete")

    actual_chains = {}
    for chain in controller.get_retarget_chains():
        name = str(chain.chain_name)
        key = _norm(name)
        _require(key not in actual_chains, f"Duplicate retarget chain: {name}")
        actual_chains[key] = (
            _norm(controller.get_retarget_chain_start_bone(chain.chain_name)),
            _norm(controller.get_retarget_chain_end_bone(chain.chain_name)),
            _norm(controller.get_retarget_chain_goal(chain.chain_name)),
        )
    expected_chains = {
        _norm(name): (_norm(start), _norm(end), _norm(goal))
        for name, (start, end, goal) in CHAINS.items()
    }
    _require(actual_chains == expected_chains, f"Retarget chains differ: {actual_chains}")

    return {
        "solver_class": solver.get_class().get_path_name(),
        "solver_root": "pelvis",
        "solver_settings": {
            "root_bone": str(solver_settings.get_editor_property("root_bone")),
            "root_behavior": str(solver_settings.get_editor_property("root_behavior")),
            "allow_stretch": bool(solver_settings.get_editor_property("allow_stretch")),
            "iterations": int(solver_settings.get_editor_property("iterations")),
            "sub_iterations": int(solver_settings.get_editor_property("sub_iterations")),
            "global_pull_chain_alpha": float(
                solver_settings.get_editor_property("global_pull_chain_alpha")
            ),
        },
        "retarget_root": "pelvis",
        "root_motion_bone": "root",
        "goals": actual_goals,
        "bone_settings": {name: bone_settings[name] for name in sorted(bone_settings)},
        "chains": {
            name: {"start": values[0], "end": values[1], "goal": values[2]}
            for name, values in sorted(actual_chains.items())
        },
    }


def _execute_control_rig_probe(rig, mesh):
    """Instantiate and execute the saved Control Rig once in a transient component."""
    component = unreal.new_object(
        unreal.ControlRigComponent,
        name="DGSession2ValidationControlRigComponent",
    )
    _require(component is not None, "Could not create transient ControlRigComponent")
    if hasattr(rig, "get_control_rig_asset_reference") and hasattr(
        component, "set_control_rig_asset_reference"
    ):
        component.set_control_rig_asset_reference(rig.get_control_rig_asset_reference())
        binding_method = "ControlRigAssetReference"
    else:
        component.set_control_rig_class(rig.generated_class())
        binding_method = "GeneratedClassFallback"
    component.set_bone_initial_transforms_from_skeletal_mesh(mesh)
    component.initialize()
    instance = component.get_control_rig()
    _require(instance is not None, "Transient Control Rig instance was not created")
    hierarchy = instance.get_hierarchy()
    _require(hierarchy is not None, "Transient Control Rig hierarchy is unavailable")
    _require(
        component.does_element_exist("pelvis", unreal.RigElementType.BONE),
        "Transient Control Rig cannot resolve pelvis",
    )
    component.update(0.0)
    return {
        "status": "EXECUTED",
        "binding_method": binding_method,
        "hierarchy_key_count": len(hierarchy.get_all_keys()),
        "pelvis_resolved": True,
        "required_log_condition": "NO_PBIK_INITIALIZATION_WARNING",
    }


def _validate_control_rig(rig, mesh):
    _require(rig.get_preview_mesh() == mesh, "Control Rig preview mesh mismatch")
    rig.request_auto_vm_recompilation()
    rig.recompile_vm()
    _require(
        rig.get_editor_property("status") == unreal.BlueprintStatus.BS_UP_TO_DATE,
        f"Control Rig compile status is {rig.get_editor_property('status')}",
    )
    hierarchy = rig.get_hierarchy()
    keys = list(hierarchy.get_all_keys())
    bone_names = {
        str(key.name) for key in keys if key.type == unreal.RigElementType.BONE
    }
    _require(bone_names == {name for name, _parent in HIERARCHY}, "CR bone import mismatch")
    for bone_name, expected_parent in HIERARCHY:
        key = unreal.RigElementKey(type=unreal.RigElementType.BONE, name=bone_name)
        parent_key = hierarchy.get_first_parent(key)
        actual_parent = str(parent_key.name) if parent_key else None
        if _norm(actual_parent) is None:
            actual_parent = None
        _require(
            _norm(actual_parent) == _norm(expected_parent),
            f"Control Rig parent mismatch for {bone_name}: {actual_parent} != {expected_parent}",
        )
    control_names = {
        str(key.name) for key in keys if key.type == unreal.RigElementType.CONTROL
    }
    required_transform_controls = {"ctrl_pelvis"} | {
        f"ctrl_{bone}" for bone in EFFECTORS.values()
    }
    _require(
        required_transform_controls | set(PROFILE_CONTROLS) <= control_names,
        f"Control Rig controls are incomplete: {sorted(control_names)}",
    )
    for name in required_transform_controls:
        key = unreal.RigElementKey(type=unreal.RigElementType.CONTROL, name=name)
        settings = hierarchy.get_control_settings(key)
        _require(
            settings.control_type == unreal.RigControlType.EULER_TRANSFORM,
            f"Transform control {name} has type {settings.control_type}",
        )
    for name in PROFILE_CONTROLS:
        key = unreal.RigElementKey(type=unreal.RigElementType.CONTROL, name=name)
        settings = hierarchy.get_control_settings(key)
        _require(
            settings.control_type == unreal.RigControlType.FLOAT,
            f"Scalar control {name} has type {settings.control_type}",
        )
    rig_curves = {str(key.name) for key in keys if key.type == unreal.RigElementType.CURVE}
    _require(CURVES <= rig_curves, f"Control Rig curves are incomplete: {sorted(rig_curves)}")

    defaults = {}
    for name, expected in PROFILE_CONTROLS.items():
        key = unreal.RigElementKey(type=unreal.RigElementType.CONTROL, name=name)
        value = hierarchy.get_control_value(key, unreal.RigControlValueType.INITIAL)
        actual = float(unreal.RigHierarchy.get_float_from_control_value(value))
        _require(abs(actual - expected) <= 1.0e-4, f"Control {name} default {actual} != {expected}")
        defaults[name] = actual

    grips = {}
    hands = {}
    for side in ("l", "r"):
        grip_key = unreal.RigElementKey(
            type=unreal.RigElementType.BONE, name=f"disc_grip_{side}"
        )
        hand_key = unreal.RigElementKey(type=unreal.RigElementType.BONE, name=f"hand_{side}")
        grip_location = _vector(hierarchy.get_global_transform(grip_key, True))
        hand_location = _vector(hierarchy.get_global_transform(hand_key, True))
        offset_cm = _distance(grip_location, hand_location)
        _require(2.0 <= offset_cm <= 15.0, f"disc_grip_{side} is not palm-local: {offset_cm} cm")
        grips[side] = grip_location
        hands[side] = hand_location
    _require(
        abs(grips["l"][0] + grips["r"][0]) <= 0.25
        and abs(grips["l"][1] - grips["r"][1]) <= 0.25
        and abs(grips["l"][2] - grips["r"][2]) <= 0.25,
        f"Disc grip origins are not mirrored: {grips}",
    )

    model = rig.get_default_model()
    nodes = {node.get_name(): node for node in model.get_nodes()}
    _require("BeginExecution" in nodes and "DGFullBodyIK" in nodes, "Core PBIK graph nodes missing")
    pbik = nodes["DGFullBodyIK"]
    expected_structs = {
        "BeginExecution": "/Script/ControlRig.RigUnit_BeginExecution",
        "DGFullBodyIK": "/Script/PBIK.RigUnit_PBIK",
    }
    for node_name, expected_struct in expected_structs.items():
        actual_struct = nodes[node_name].get_script_struct().get_path_name()
        _require(actual_struct == expected_struct, f"{node_name} has wrong unit struct {actual_struct}")
    for node in nodes.values():
        _require(not node.has_orphaned_pins(), f"Control Rig node has orphaned pins: {node.get_name()}")
    root_pin = pbik.find_pin("Root")
    _require(root_pin is not None, "PBIK root pin is missing")
    _require(
        root_pin.get_default_value() == "pelvis",
        "PBIK root must be the raw runtime-safe FName pelvis (quotes are invalid)",
    )
    _require(pbik.find_pin("Effectors").get_array_size() == 4, "PBIK must have exactly four effectors")
    _require(
        pbik.find_pin("Settings.RootBehavior").get_default_value().casefold() == "free",
        "Control Rig PBIK root behavior is not Free",
    )
    _require(
        int(pbik.find_pin("Settings.Iterations").get_default_value()) == PBIK_ITERATIONS,
        f"Control Rig PBIK iterations are not {PBIK_ITERATIONS}",
    )
    _require(
        int(pbik.find_pin("Settings.SubIterations").get_default_value())
        == PBIK_SUB_ITERATIONS,
        f"Control Rig PBIK sub-iterations are not {PBIK_SUB_ITERATIONS}",
    )
    _require(
        abs(float(pbik.find_pin("Settings.GlobalPullChainAlpha").get_default_value())) <= 1.0e-6,
        "Control Rig PBIK global pull-chain alpha is not zero",
    )
    _require(
        pbik.find_pin("Settings.bAllowStretch").get_default_value().casefold() == "false",
        "Control Rig PBIK unexpectedly allows stretch",
    )
    exec_sources = _source_paths(pbik.find_pin("ExecuteContext"))
    if any("BeginExecution" in path for path in exec_sources):
        execution_contract = "BeginExecution -> DGFullBodyIK"
    else:
        _require(
            "DGApplyCharacterProfile" in nodes,
            "PBIK execution is neither the accepted Session 2 direct path nor the Session 4 profile path",
        )
        profile_unit = nodes["DGApplyCharacterProfile"]
        _require(
            profile_unit.get_script_struct().get_path_name()
            == "/Script/DiscGolfTour.RigUnit_DGApplyCharacterProfile",
            "DGApplyCharacterProfile has the wrong unit struct",
        )
        _require(
            any("DGApplyCharacterProfile" in path for path in exec_sources),
            "PBIK is not driven by the Session 4 profile unit",
        )
        _require(
            any(
                "BeginExecution" in path
                for path in _source_paths(profile_unit.find_pin("ExecuteContext"))
            ),
            "Session 4 profile unit is not driven by BeginExecution",
        )
        execution_contract = "BeginExecution -> DGApplyCharacterProfile -> DGFullBodyIK"

    pbik_bone_settings_pin = pbik.find_pin("BoneSettings")
    _require(pbik_bone_settings_pin is not None, "PBIK BoneSettings pin is missing")
    _require(
        pbik_bone_settings_pin.get_array_size() == len(BEND_BONE_SETTINGS),
        f"PBIK must have exactly {len(BEND_BONE_SETTINGS)} bend bone settings",
    )
    pbik_bone_settings = {}
    for index, (bone_name, expected_angles) in enumerate(BEND_BONE_SETTINGS):
        prefix = f"BoneSettings.{index}"
        bone_pin = pbik.find_pin(f"{prefix}.Bone")
        preferred_pin = pbik.find_pin(f"{prefix}.bUsePreferredAngles")
        _require(bone_pin is not None, f"PBIK bone setting {index} has no Bone pin")
        _require(
            bone_pin.get_default_value() == bone_name,
            f"PBIK bone setting {index} must use raw runtime-safe {bone_name}",
        )
        _require(
            preferred_pin is not None
            and preferred_pin.get_default_value().strip().casefold() == "true",
            f"PBIK preferred angles are disabled for {bone_name}",
        )
        actual_angles = [
            _pin_float(
                pbik.find_pin(f"{prefix}.PreferredAngles.{axis}"),
                f"PBIK {bone_name} preferred-angle {axis} pin is invalid",
            )
            for axis in "XYZ"
        ]
        _require(
            _angles_match(actual_angles, expected_angles),
            f"PBIK {bone_name} preferred angles {actual_angles} != {expected_angles}",
        )
        _require(
            abs(
                _pin_float(
                    pbik.find_pin(f"{prefix}.PositionStiffness"),
                    f"PBIK {bone_name} position stiffness pin is invalid",
                )
            )
            <= 1.0e-6
            and abs(
                _pin_float(
                    pbik.find_pin(f"{prefix}.RotationStiffness"),
                    f"PBIK {bone_name} rotation stiffness pin is invalid",
                )
            )
            <= 1.0e-6,
            f"PBIK {bone_name} unexpectedly changes stiffness",
        )
        for axis in "XYZ":
            limit_pin = pbik.find_pin(f"{prefix}.{axis}")
            _require(
                limit_pin is not None
                and limit_pin.get_default_value().strip().casefold() == "free",
                f"PBIK {bone_name} {axis} limit is not Free",
            )
            for bound in ("Min", "Max"):
                _require(
                    abs(
                        _pin_float(
                            pbik.find_pin(f"{prefix}.{bound}{axis}"),
                            f"PBIK {bone_name} {bound}{axis} pin is invalid",
                        )
                    )
                    <= 1.0e-6,
                    f"PBIK {bone_name} {bound}{axis} is not zero/default",
                )
        pbik_bone_settings[bone_name] = {
            "use_preferred_angles": True,
            "preferred_angles": actual_angles,
            "position_stiffness": 0.0,
            "rotation_stiffness": 0.0,
            "limits": {axis.casefold(): "Free" for axis in "XYZ"},
        }

    effector_links = {}
    ordered_effectors = ["hand_l", "hand_r", "foot_l", "foot_r"]
    for index, bone_name in enumerate(ordered_effectors):
        bone_pin = pbik.find_pin(f"Effectors.{index}.Bone")
        transform_pin = pbik.find_pin(f"Effectors.{index}.Transform")
        position_pin = pbik.find_pin(f"Effectors.{index}.PositionAlpha")
        rotation_pin = pbik.find_pin(f"Effectors.{index}.RotationAlpha")
        _require(
            all(pin is not None for pin in (bone_pin, transform_pin, position_pin, rotation_pin)),
            f"PBIK effector pins missing at index {index}",
        )
        _require(
            bone_pin.get_default_value() == bone_name,
            f"PBIK effector {index} must use raw runtime-safe {bone_name}",
        )
        _require(
            int(pbik.find_pin(f"Effectors.{index}.ChainDepth").get_default_value()) == 2,
            f"PBIK {bone_name} chain depth is not 2",
        )
        transform_sources = _source_paths(transform_pin)
        _require(len(transform_sources) == 1, f"PBIK {bone_name} transform must have one source")
        _require(
            any(f"Get_{bone_name}_Control" in path for path in transform_sources),
            f"PBIK {bone_name} transform is not driven by its control",
        )
        transform_source_node = transform_pin.get_linked_source_pins(False)[0].get_node()
        _require(
            transform_source_node.get_script_struct().get_path_name()
            == "/Script/ControlRig.RigUnit_GetTransform",
            f"PBIK {bone_name} transform source has wrong unit type",
        )
        _require(
            _norm(transform_source_node.find_pin("Item.Name").get_default_value())
            == _norm(f"ctrl_{bone_name}"),
            f"PBIK {bone_name} transform getter targets the wrong control",
        )
        alpha_sources = _source_paths(position_pin)
        rotation_sources = _source_paths(rotation_pin)
        _require(
            len(alpha_sources) == 1 and len(rotation_sources) == 1,
            f"PBIK {bone_name} alpha inputs must each have exactly one source",
        )
        expected_source = (
            f"Get_DG_FootPlant_{'L' if bone_name.endswith('_l') else 'R'}"
            if bone_name.startswith("foot_")
            else f"Get_dg_hand_ik_alpha_{'l' if bone_name.endswith('_l') else 'r'}"
        )
        _require(
            any(expected_source in path for path in alpha_sources),
            f"PBIK {bone_name} position alpha source is wrong: {alpha_sources}",
        )
        _require(
            any(expected_source in path for path in rotation_sources),
            f"PBIK {bone_name} rotation alpha source is wrong: {rotation_sources}",
        )
        alpha_source_node = position_pin.get_linked_source_pins(False)[0].get_node()
        if bone_name.startswith("foot_"):
            expected_curve = f"DG_FootPlant_{'L' if bone_name.endswith('_l') else 'R'}"
            _require(
                alpha_source_node.get_script_struct().get_path_name()
                == "/Script/ControlRig.RigUnit_GetCurveValue",
                f"PBIK {bone_name} alpha source has wrong unit type",
            )
            _require(
                _norm(alpha_source_node.find_pin("Curve").get_default_value())
                == _norm(expected_curve),
                f"PBIK {bone_name} reads the wrong plant curve",
            )
        else:
            expected_control = f"dg_hand_ik_alpha_{'l' if bone_name.endswith('_l') else 'r'}"
            _require(
                alpha_source_node.get_script_struct().get_path_name()
                == "/Script/ControlRig.RigUnit_GetControlFloat",
                f"PBIK {bone_name} alpha source has wrong unit type",
            )
            _require(
                _norm(alpha_source_node.find_pin("Control").get_default_value())
                == _norm(expected_control),
                f"PBIK {bone_name} reads the wrong alpha control",
            )
        effector_links[bone_name] = {
            "transform": transform_sources,
            "position_alpha": alpha_sources,
            "rotation_alpha": rotation_sources,
        }

    runtime_probe = _execute_control_rig_probe(rig, mesh)
    return {
        "compile_status": str(rig.get_editor_property("status")),
        "bone_count": len(bone_names),
        "controls": sorted(control_names),
        "control_defaults": defaults,
        "curves": sorted(rig_curves),
        "nodes": sorted(nodes),
        "pbik_root": "pelvis",
        "pbik_settings": {
            "root_behavior": "Free",
            "allow_stretch": False,
            "iterations": PBIK_ITERATIONS,
            "sub_iterations": PBIK_SUB_ITERATIONS,
            "execution": execution_contract,
            "global_pull_chain_alpha": 0.0,
        },
        "pbik_bone_settings": {
            name: pbik_bone_settings[name] for name in sorted(pbik_bone_settings)
        },
        "runtime_probe": runtime_probe,
        "effectors": effector_links,
        "disc_grips": {
            "live_validation": "mirrored palm-local origins and hand parenting",
            "source_proxy_convention": (
                "primary bone axis points palm-outward toward the fingertips in the neutral T-pose"
            ),
            "production_disc_fit": "PENDING_VISUAL_ACCEPTANCE",
            "left_origin_cm": grips["l"],
            "right_origin_cm": grips["r"],
            "mirrored": True,
        },
    }


def _validate_profiles():
    results = {}
    for path, expected in PROFILE_SPECS.items():
        profile = _load(path, unreal.DiscGolfCharacterProfile)
        body = profile.get_editor_property("body")
        actual = {name: float(body.get_editor_property(name)) for name in expected}
        for name, value in expected.items():
            _require(abs(actual[name] - value) <= 1.0e-5, f"{path} {name} mismatch")
        _require(
            _norm(profile.get_editor_property("right_disc_grip_bone")) == "disc_grip_r"
            and _norm(profile.get_editor_property("left_disc_grip_bone")) == "disc_grip_l",
            f"{path} disc-grip defaults are wrong",
        )
        results[path] = actual

    ordered = sorted(values["height_cm"] for values in results.values())
    _require(ordered == [155.0, 183.0, 205.0], "Body-profile height ordering is invalid")
    return {
        "profiles": results,
        "shared_master_mesh": MESH_PATH,
        "shared_master_skeleton": SKELETON_PATH,
        "shared_control_rig": CONTROL_RIG_PATH,
        "separate_skeletons_created_by_session_2": False,
        "control_mapping": {
            "height_cm": "dg_height_cm",
            "wingspan_scale": "dg_wingspan_scale",
            "shoulder_width_scale": "dg_shoulder_width_scale",
            "torso_length_scale": "dg_torso_length_scale",
            "leg_length_scale": "dg_leg_length_scale",
            "hand_scale": "dg_hand_scale",
        },
        "status": "DATA_AND_RIG_INPUT_FOUNDATION_PASS",
        "runtime_profile_deformation": "DEFERRED_TO_SESSION_4",
        "throw_style_tuning": "DEFERRED_TO_SESSION_4",
    }


def _validate_anim_bp(anim_bp, skeleton):
    compile_ok = unreal.BlueprintEditorLibrary.compile_blueprint(anim_bp)
    _require(compile_ok, "ABP_DG_Player compilation returned false")
    _require(
        anim_bp.get_editor_property("status") == unreal.BlueprintStatus.BS_UP_TO_DATE,
        f"ABP compile status is {anim_bp.get_editor_property('status')}",
    )
    parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(anim_bp)
    _require(parent == unreal.DiscGolfAnimInstance.static_class(), f"Wrong ABP parent: {parent}")
    _require(anim_bp.get_editor_property("target_skeleton") == skeleton, "ABP skeleton mismatch")
    generated_class = anim_bp.generated_class()
    _require(generated_class is not None, "ABP_DG_Player has no generated class")
    graph_names = [str(value) for value in unreal.BlueprintEditorLibrary.list_graph_names(anim_bp)]
    _require(any(name.casefold() == "animgraph" for name in graph_names), "ABP has no AnimGraph")
    return {
        "compile_status": str(anim_bp.get_editor_property("status")),
        "parent_class": parent.get_path_name(),
        "target_skeleton": skeleton.get_path_name(),
        "graphs": graph_names,
        "generated_class": generated_class.get_path_name(),
        "gameplay_assignment_scan": "NO_PROJECT_SOURCE_OR_CONFIG_ASSIGNMENT",
    }


def _validate_authority_boundary():
    forbidden = (
        "DiscGolfThrowComponent",
        "AnimNotify_DiscRelease",
        "OnDiscRelease",
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
            for token in forbidden:
                if token in text:
                    violations.append(f"{path.relative_to(PROJECT_ROOT)}:{token}")
    _require(not violations, f"Session 3/gameplay wiring detected: {violations}")
    return {
        "gameplay_wiring": "NONE_SESSION_2",
        "project_source_config_violations": violations,
        "release_authority": "EXISTING_PROJECT_SYSTEMS_UNCHANGED",
    }


def _validate_source_contract():
    _require(CONTRACT_PATH.is_file(), f"Missing v1.5 contract: {CONTRACT_PATH}")
    raw = CONTRACT_PATH.read_bytes()
    contract = json.loads(raw.decode("utf-8"))
    contract_hierarchy = [(name, parent) for name, parent in contract["hierarchy"]]
    finger = contract["finger_pattern"]
    for side in ("l", "r"):
        for finger_name in finger["each_hand"]:
            parent = f"hand_{side}"
            for segment in range(1, int(finger["segments"]) + 1):
                name = finger["name_template"].format(
                    finger=finger_name, segment=segment, side=side
                )
                contract_hierarchy.append((name, parent))
                parent = name
    _require(contract_hierarchy == HIERARCHY, "Validator hierarchy drifted from v1.5 contract")
    _require(
        set(contract["required_animation_curves"]) == CURVES,
        "Validator curve contract drifted from v1.5",
    )
    return {
        "path": str(CONTRACT_PATH),
        "version": int(contract["version"]),
        "sha256": hashlib.sha256(raw).hexdigest().upper(),
        "required_bones": len(contract_hierarchy),
        "required_curves": len(contract["required_animation_curves"]),
    }


def main():
    mesh = _load(MESH_PATH, unreal.SkeletalMesh)
    skeleton = _load(SKELETON_PATH, unreal.Skeleton)
    ik_rig = _load(IK_PATH, unreal.IKRigDefinition)
    control_rig = _load(CONTROL_RIG_PATH, unreal.ControlRigBlueprint)
    anim_bp = _load(ANIM_BP_PATH, unreal.AnimBlueprint)

    report = {
        "status": "PASS",
        "engine_version": unreal.SystemLibrary.get_engine_version(),
        "source_contract": _validate_source_contract(),
        "assets": {
            "mesh": MESH_PATH,
            "skeleton": SKELETON_PATH,
            "ik_rig": IK_PATH,
            "control_rig": CONTROL_RIG_PATH,
            "animation_blueprint": ANIM_BP_PATH,
        },
        "master": _validate_master(mesh, skeleton),
        "ik_rig": _validate_ik(ik_rig, mesh),
        "control_rig": _validate_control_rig(control_rig, mesh),
        "body_profiles": _validate_profiles(),
        "animation_blueprint": _validate_anim_bp(anim_bp, skeleton),
        "authority_boundary": _validate_authority_boundary(),
        "deferred_to_session_3": [
            "throw montage and animation",
            "release notifies",
            "held-disc attachment/release handoff",
            "gameplay pawn assignment",
        ],
        "deferred_to_session_4": [
            "runtime body-profile deformation",
            "profile-to-rig runtime plumbing",
            "throw-style tuning and creator integration",
        ],
        "manual_visual_gates": [
            "visually confirm the authored mirrored elbow/knee bend directions",
            "PBIK compression test for both hands and both feet",
            "real disc fit/orientation check at disc_grip_l and disc_grip_r",
        ],
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log(
        "DG_SESSION2_VALIDATION: PASS bones=69 goals=4 effectors=4 profiles=3 "
        "gameplay_wiring=NONE_SESSION_2"
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
                    "error": f"{type(error).__name__}: {error}",
                },
                indent=2,
            ),
            encoding="utf-8",
        )
        raise
