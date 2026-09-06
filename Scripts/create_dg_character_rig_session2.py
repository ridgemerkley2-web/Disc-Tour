"""Create the non-gameplay DG master rig foundation for Session 2.

This script is intentionally limited to project-authored character content.  It
does not assign the assets to the gameplay pawn, bind release delegates, create
throw animations, or touch disc-flight physics.

Once every expected output exists, the default rerun is validation-only so UE
controller setters cannot churn unchanged packages. Set the task-specific
DISC_GOLF_SESSION2_FORCE_REAUTHOR environment variable to 1 only after an
intentional review when the generated assets truly need to be rewritten.
"""

from pathlib import Path
import json
import os
import runpy

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
MESH_PATH = "/Game/DiscGolf/Characters/Meshes/SK_DG_Master"
SKELETON_PATH = "/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master"
IK_PATH = "/Game/DiscGolf/Rigs/IK_DG_Master"
CONTROL_RIG_PATH = "/Game/DiscGolf/Rigs/CR_DG_Master"
ANIM_BP_PATH = "/Game/DiscGolf/Animation/ABP_DG_Player"
PROFILE_ROOT = "/Game/DiscGolf/Characters/Profiles"
TEST_PROFILE_ROOT = "/Game/DiscGolf/Tests/Profiles"
REPORT_PATH = PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session2RigCreation.json"

CURVES = (
    "DG_FootPlant_L",
    "DG_FootPlant_R",
    "DG_ReachbackAlpha",
    "DG_BraceAlpha",
    "DG_ReleaseApproachAlpha",
    "DG_FollowThroughAlpha",
    "DG_WeightShiftAlpha",
    "DG_BraceCompressionAlpha",
    "DG_HipDriveAlpha",
    "DG_TorsoDriveAlpha",
    "DG_ShoulderDriveAlpha",
    "DG_ElbowLeadAlpha",
    "DG_WristLagAlpha",
    "DG_FingerReleaseAlpha",
    "DG_OffArmCounterbalanceAlpha",
    "DG_GazeTargetAlpha",
    "DG_DiscPlaneAlpha",
    "DG_BraceExtensionAlpha",
    "DG_RecoveryBeatAlpha",
)

CHAINS = (
    ("Root", "root", "root", ""),
    ("Spine", "spine_01", "spine_04", ""),
    ("Neck", "neck_01", "head", ""),
    ("Arm_L", "upperarm_l", "hand_l", "hand_l_Goal"),
    ("Arm_R", "upperarm_r", "hand_r", "hand_r_Goal"),
    ("Leg_L", "thigh_l", "foot_l", "foot_l_Goal"),
    ("Leg_R", "thigh_r", "foot_r", "foot_r_Goal"),
    ("Foot_L", "foot_l", "ball_l", ""),
    ("Foot_R", "foot_r", "ball_r", ""),
)

EFFECTORS = (
    ("hand_l_Goal", "hand_l"),
    ("hand_r_Goal", "hand_r"),
    ("foot_l_Goal", "foot_l"),
    ("foot_r_Goal", "foot_r"),
)

# UE's PBIK preferred angles are expressed in each bone's local solver axes.
# The proxy's arms are mirrored across X, so the elbow Z preference mirrors;
# both calves share the same local X bend direction after FBX import. Keep this
# ordered contract identical in IK_DG_Master and CR_DG_Master.
BEND_BONE_SETTINGS = (
    ("lowerarm_l", (0.0, 0.0, -45.0)),
    ("lowerarm_r", (0.0, 0.0, 45.0)),
    ("calf_l", (45.0, 0.0, 0.0)),
    ("calf_r", (45.0, 0.0, 0.0)),
)

PBIK_ITERATIONS = 20
PBIK_SUB_ITERATIONS = 10

PROFILES = (
    (
        "DA_DG_Test_ShortCompact",
        "ShortCompact",
        dict(
            height_cm=155.0,
            wingspan_scale=0.94,
            shoulder_width_scale=0.94,
            torso_length_scale=0.96,
            leg_length_scale=0.95,
            hand_scale=0.95,
        ),
    ),
    (
        "DA_DG_DefaultCharacter",
        "Baseline",
        dict(
            height_cm=183.0,
            wingspan_scale=1.0,
            shoulder_width_scale=1.0,
            torso_length_scale=1.0,
            leg_length_scale=1.0,
            hand_scale=1.0,
        ),
    ),
    (
        "DA_DG_Test_TallLongArms",
        "TallLongArms",
        dict(
            height_cm=205.0,
            wingspan_scale=1.07,
            shoulder_width_scale=1.05,
            torso_length_scale=1.04,
            leg_length_scale=1.05,
            hand_scale=1.05,
        ),
    ),
)

PROFILE_CONTROLS = (
    ("dg_height_cm", 183.0),
    ("dg_wingspan_scale", 1.0),
    ("dg_shoulder_width_scale", 1.0),
    ("dg_torso_length_scale", 1.0),
    ("dg_leg_length_scale", 1.0),
    ("dg_hand_scale", 1.0),
)

HAND_IK_CONTROLS = (
    ("dg_hand_ik_alpha_l", 0.0),
    ("dg_hand_ik_alpha_r", 0.0),
)

EXPECTED_OUTPUTS = (
    MESH_PATH,
    SKELETON_PATH,
    IK_PATH,
    CONTROL_RIG_PATH,
    ANIM_BP_PATH,
    f"{PROFILE_ROOT}/DA_DG_DefaultCharacter",
    f"{TEST_PROFILE_ROOT}/DA_DG_Test_ShortCompact",
    f"{TEST_PROFILE_ROOT}/DA_DG_Test_TallLongArms",
)


def _log(message):
    unreal.log(f"DG_SESSION2_RIG: {message}")


def _should_run_validation_only():
    force_value = os.environ.get("DISC_GOLF_SESSION2_FORCE_REAUTHOR", "").strip().casefold()
    force_reauthor = force_value in {"1", "true", "yes", "on"}
    return not force_reauthor and all(
        unreal.EditorAssetLibrary.does_asset_exist(path) for path in EXPECTED_OUTPUTS
    )


def _run_validation_only():
    validator_path = PROJECT_ROOT / "Scripts" / "validate_dg_character_session2.py"
    if not validator_path.is_file():
        raise RuntimeError(f"Missing Session 2 strict validator: {validator_path}")
    _log(
        "all expected outputs already exist; running strict validation without "
        "reauthoring packages"
    )
    runpy.run_path(str(validator_path), run_name="__main__")
    _log("PASS validation-only rerun; no generated package save requested")


def _require_asset(path, asset_type):
    asset = unreal.load_asset(path)
    if not isinstance(asset, asset_type):
        raise RuntimeError(f"Required {asset_type.__name__} did not load: {path}")
    return asset


def _ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"Could not create content folder: {path}")


def _save(asset):
    # Validation reruns must not rewrite an unchanged package. Asset creation
    # and authoring operations mark their packages dirty when persistence is
    # actually required, so the normal dirty-only save is the safe boundary.
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=True):
        raise RuntimeError(f"Could not save asset: {asset.get_path_name()}")


def _ensure_curves(skeleton):
    existing = {str(name) for name in skeleton.get_curve_meta_data_names()}
    changed = False
    for curve in CURVES:
        if curve not in existing:
            if not skeleton.add_curve_meta_data(curve):
                raise RuntimeError(f"Could not register skeleton curve: {curve}")
            changed = True
    if changed:
        _save(skeleton)
    actual = {str(name) for name in skeleton.get_curve_meta_data_names()}
    missing = sorted(set(CURVES) - actual)
    if missing:
        raise RuntimeError(f"Skeleton curve registration incomplete: {missing}")
    return sorted(set(CURVES) & actual)


def _create_profile(asset_name, display_name, values, folder):
    path = f"{folder}/{asset_name}"
    profile = unreal.load_asset(path)
    created = profile is None
    if created:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.DiscGolfCharacterProfile.static_class())
        profile = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            folder,
            unreal.DiscGolfCharacterProfile,
            factory,
        )
    if not isinstance(profile, unreal.DiscGolfCharacterProfile):
        raise RuntimeError(f"Character profile path has the wrong class: {path}")

    changed = created
    current_body = profile.get_editor_property("body")
    body_changed = any(
        abs(float(current_body.get_editor_property(name)) - float(value)) > 1.0e-5
        for name, value in values.items()
    )
    if body_changed:
        body = unreal.DGBodyProfile()
        for name, value in values.items():
            body.set_editor_property(name, value)
        profile.set_editor_property("body", body)
        changed = True
    if str(profile.get_editor_property("display_name")) != display_name:
        profile.set_editor_property(
            "display_name", unreal.TextLibrary.conv_string_to_text(display_name)
        )
        changed = True
    if str(profile.get_editor_property("right_disc_grip_bone")) != "disc_grip_r":
        profile.set_editor_property("right_disc_grip_bone", "disc_grip_r")
        changed = True
    if str(profile.get_editor_property("left_disc_grip_bone")) != "disc_grip_l":
        profile.set_editor_property("left_disc_grip_bone", "disc_grip_l")
        changed = True
    if changed:
        _save(profile)
    return profile


def _ensure_profiles():
    created = []
    for asset_name, display_name, values in PROFILES:
        folder = PROFILE_ROOT if display_name == "Baseline" else TEST_PROFILE_ROOT
        profile = _create_profile(asset_name, display_name, values, folder)
        body = profile.get_editor_property("body")
        for name, expected in values.items():
            actual = float(body.get_editor_property(name))
            if abs(actual - expected) > 1.0e-5:
                raise RuntimeError(
                    f"Profile {display_name} property {name} is {actual}, expected {expected}"
                )
        created.append(profile.get_path_name().split(".", 1)[0])
    return created


def _clear_ik_definition(controller):
    for chain in list(controller.get_retarget_chains()):
        controller.remove_retarget_chain(str(chain.chain_name))
    # IKRigEffectorGoal.get_fname() is the UObject instance name (for example
    # IKRigEffectorGoal_0), not the logical goal name accepted by remove_goal.
    for goal in list(controller.get_all_goals()):
        logical_name = str(goal.get_editor_property("goal_name"))
        if logical_name and logical_name != "None":
            controller.remove_goal(logical_name)
    while controller.get_num_solvers() > 0:
        if not controller.remove_solver(0):
            raise RuntimeError("Could not clear the IK Rig solver stack")


def _float_matches(actual, expected, tolerance=1.0e-5):
    return abs(float(actual) - float(expected)) <= tolerance


def _vector_matches(actual, expected, tolerance=1.0e-5):
    return all(
        _float_matches(component, target, tolerance)
        for component, target in zip((actual.x, actual.y, actual.z), expected)
    )


def _ensure_ik_bend_settings(controller, solver_controller):
    """Author the four explicit FBIK bend preferences without touching limits."""
    changed = False
    # The generic controller's GetBoneSettings is not reflected correctly for
    # the UE 5.8 FBIK struct (it returns None even for registered settings).
    # The solver-specific getter does expose saved values, so use the complete
    # preferred-angle contract as the no-write/no-warning guard.
    pending_bones = []
    for bone_name, preferred_angles in BEND_BONE_SETTINGS:
        setting = solver_controller.get_bone_settings(bone_name)
        if (
            setting is not None
            and str(setting.get_editor_property("bone")).casefold()
            == bone_name.casefold()
            and bool(setting.get_editor_property("use_preferred_angles"))
            and _vector_matches(
                setting.get_editor_property("preferred_angles"), preferred_angles
            )
        ):
            continue
        pending_bones.append(bone_name)

    if not pending_bones:
        return False

    # AddBoneSetting creates missing entries and safely refuses an existing
    # wrong entry, after which SetBoneSettings repairs that copied struct.
    for bone_name in pending_bones:
        controller.add_bone_setting(bone_name, 0)

    # Adding settings rebuilds the solver-controller view in UE 5.8. Reacquire
    # it before reading and writing the copied FIKRigFBIKBoneSettings structs.
    solver_controller = controller.get_solver_controller(0)
    if not isinstance(solver_controller, unreal.IKRigFBIKController):
        raise RuntimeError("Could not reacquire IK FBIK controller after adding settings")

    desired_angles = dict(BEND_BONE_SETTINGS)
    for bone_name in pending_bones:
        preferred_angles = desired_angles[bone_name]
        setting = solver_controller.get_bone_settings(bone_name)
        if (
            setting is None
            or str(setting.get_editor_property("bone")).casefold()
            != bone_name.casefold()
        ):
            raise RuntimeError(f"IK FBIK bone setting did not persist for {bone_name}")
        setting_changed = False
        if not bool(setting.get_editor_property("use_preferred_angles")):
            setting.set_editor_property("use_preferred_angles", True)
            setting_changed = True
        actual_angles = setting.get_editor_property("preferred_angles")
        if not _vector_matches(actual_angles, preferred_angles):
            setting.set_editor_property("preferred_angles", unreal.Vector(*preferred_angles))
            setting_changed = True
        if setting_changed:
            solver_controller.set_bone_settings(bone_name, setting)
            changed = True
    return changed


def _create_ik_rig(mesh):
    ik_rig = unreal.load_asset(IK_PATH)
    created = ik_rig is None
    if created:
        ik_rig = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "IK_DG_Master",
            "/Game/DiscGolf/Rigs",
            unreal.IKRigDefinition,
            unreal.IKRigDefinitionFactory(),
        )
    if not isinstance(ik_rig, unreal.IKRigDefinition):
        raise RuntimeError(f"Could not create/load IK Rig: {IK_PATH}")

    controller = unreal.IKRigController.get_controller(ik_rig)
    changed = created
    if controller.get_skeletal_mesh() != mesh:
        if not controller.set_skeletal_mesh(mesh):
            raise RuntimeError("IK_DG_Master rejected SK_DG_Master")
        changed = True

    if created:
        _clear_ik_definition(controller)
        solver_index = controller.add_solver("/Script/IKRig.IKRigFullBodyIKSolver")
        if solver_index < 0:
            raise RuntimeError("Could not add the UE 5.8 Full Body IK solver")
        if not controller.set_start_bone("pelvis", solver_index):
            raise RuntimeError("Could not set IK solver root to pelvis")

        for goal_name, bone_name in EFFECTORS:
            result = str(controller.add_new_goal(goal_name, bone_name))
            if not result or result == "None":
                raise RuntimeError(f"Could not add IK goal {goal_name} for {bone_name}")
            if result.casefold() != goal_name.casefold():
                raise RuntimeError(
                    f"IK goal {goal_name} was unexpectedly renamed to {result}; "
                    "the asset likely contains a stale duplicate"
                )
            if not controller.connect_goal_to_solver(result, solver_index):
                raise RuntimeError(f"Could not connect IK goal {result} to FBIK solver")

        for chain_name, start, end, goal in CHAINS:
            result = str(controller.add_retarget_chain(chain_name, start, end, goal))
            if not result or result == "None":
                raise RuntimeError(f"Could not add retarget chain {chain_name}")
        changed = True

    if str(controller.get_retarget_root()) != "pelvis":
        if not controller.set_retarget_root("pelvis"):
            raise RuntimeError("Could not set IK retarget root to pelvis")
        changed = True
    if str(controller.get_root_motion_bone()) != "root":
        if not controller.set_root_motion_bone("root"):
            raise RuntimeError("Could not set IK root-motion bone to root")
        changed = True
    if controller.get_num_solvers() != 1:
        raise RuntimeError(f"IK_DG_Master has {controller.get_num_solvers()} solvers, expected 1")
    if not controller.get_solver_enabled(0):
        raise RuntimeError("IK_DG_Master FBIK solver is disabled")
    if str(controller.get_start_bone(0)) != "pelvis":
        if not controller.set_start_bone("pelvis", 0):
            raise RuntimeError("Could not set IK solver root to pelvis")
        changed = True
    solver_controller = controller.get_solver_controller(0)
    if not isinstance(solver_controller, unreal.IKRigFBIKController):
        raise RuntimeError(f"IK_DG_Master solver has wrong controller class: {solver_controller}")

    # Match Epic's UE 5.8 auto-FBIK baseline while keeping stretch disabled.
    # Do not rewrite the struct unless a field in our contract differs; notably,
    # this preserves the existing Iterations value.
    solver_settings = solver_controller.get_solver_settings()
    solver_settings_changed = False
    if solver_settings.get_editor_property("root_behavior") != unreal.PBIKRootBehavior.FREE:
        solver_settings.set_editor_property("root_behavior", unreal.PBIKRootBehavior.FREE)
        solver_settings_changed = True
    if not _float_matches(solver_settings.get_editor_property("global_pull_chain_alpha"), 0.0):
        solver_settings.set_editor_property("global_pull_chain_alpha", 0.0)
        solver_settings_changed = True
    if int(solver_settings.get_editor_property("sub_iterations")) != PBIK_SUB_ITERATIONS:
        solver_settings.set_editor_property("sub_iterations", PBIK_SUB_ITERATIONS)
        solver_settings_changed = True
    if bool(solver_settings.get_editor_property("allow_stretch")):
        solver_settings.set_editor_property("allow_stretch", False)
        solver_settings_changed = True
    if solver_settings_changed:
        solver_controller.set_solver_settings(solver_settings)
        changed = True
    for goal_name, _bone_name in EFFECTORS:
        goal_settings = solver_controller.get_goal_settings(goal_name)
        if int(goal_settings.get_editor_property("chain_depth")) != 2:
            goal_settings.set_editor_property("chain_depth", 2)
            solver_controller.set_goal_settings(goal_name, goal_settings)
            changed = True
    if _ensure_ik_bend_settings(controller, solver_controller):
        changed = True
    solver_controller = controller.get_solver_controller(0)
    if not isinstance(solver_controller, unreal.IKRigFBIKController):
        raise RuntimeError("Could not reacquire IK FBIK controller for verification")
    if changed:
        _save(ik_rig)

    actual_solver_settings = solver_controller.get_solver_settings()
    if actual_solver_settings.get_editor_property("root_behavior") != unreal.PBIKRootBehavior.FREE:
        raise RuntimeError("IK_DG_Master FBIK root behavior is not Free")
    if abs(float(actual_solver_settings.get_editor_property("global_pull_chain_alpha"))) > 1.0e-6:
        raise RuntimeError("IK_DG_Master global pull-chain alpha is not zero")
    if int(actual_solver_settings.get_editor_property("sub_iterations")) != 10:
        raise RuntimeError("IK_DG_Master FBIK sub-iterations are not 10")
    if bool(actual_solver_settings.get_editor_property("allow_stretch")):
        raise RuntimeError("IK_DG_Master unexpectedly allows stretch")
    if int(actual_solver_settings.get_editor_property("iterations")) != PBIK_ITERATIONS:
        raise RuntimeError(
            "IK_DG_Master FBIK iterations changed from the Session 2 baseline of "
            f"{PBIK_ITERATIONS}"
        )
    if str(controller.get_start_bone(0)) != "pelvis":
        raise RuntimeError(f"IK solver root is {controller.get_start_bone(0)}, expected pelvis")
    if str(controller.get_retarget_root()) != "pelvis":
        raise RuntimeError(f"Retarget root is {controller.get_retarget_root()}, expected pelvis")
    if str(controller.get_root_motion_bone()) != "root":
        raise RuntimeError(
            f"Root motion bone is {controller.get_root_motion_bone()}, expected root"
        )
    for bone_name, preferred_angles in BEND_BONE_SETTINGS:
        setting = solver_controller.get_bone_settings(bone_name)
        if (
            setting is None
            or str(setting.get_editor_property("bone")).casefold()
            != bone_name.casefold()
        ):
            raise RuntimeError(f"IK_DG_Master is missing bone setting {bone_name}")
        if not bool(setting.get_editor_property("use_preferred_angles")):
            raise RuntimeError(f"IK_DG_Master {bone_name} preferred angles are disabled")
        actual_angles = setting.get_editor_property("preferred_angles")
        if not _vector_matches(actual_angles, preferred_angles):
            raise RuntimeError(
                f"IK_DG_Master {bone_name} preferred angles "
                f"({actual_angles.x}, {actual_angles.y}, {actual_angles.z}) do not match "
                f"{preferred_angles}"
            )

    expected_goals = {name: bone for name, bone in EFFECTORS}
    if len(controller.get_all_goals()) != len(expected_goals):
        raise RuntimeError(
            f"IK_DG_Master has {len(controller.get_all_goals())} goals, "
            f"expected {len(expected_goals)}"
        )
    goals = {
        goal_name: str(controller.get_bone_for_goal(goal_name))
        for goal_name in expected_goals
    }
    if goals != expected_goals:
        raise RuntimeError(f"IK goal mapping mismatch: {goals} != {expected_goals}")
    disconnected = [
        goal_name
        for goal_name in expected_goals
        if not controller.is_goal_connected_to_solver(goal_name, 0)
    ]
    if disconnected:
        raise RuntimeError(f"IK goals are not connected to the FBIK solver: {disconnected}")

    chains = {
        str(chain.chain_name): {
            "start": str(controller.get_retarget_chain_start_bone(chain.chain_name)),
            "end": str(controller.get_retarget_chain_end_bone(chain.chain_name)),
            "goal": str(controller.get_retarget_chain_goal(chain.chain_name)),
        }
        for chain in controller.get_retarget_chains()
    }
    expected_chains = {
        name: {
            "start": start,
            "end": end,
            "goal": goal or "None",
        }
        for name, start, end, goal in CHAINS
    }
    if set(chains) != set(expected_chains):
        raise RuntimeError(
            f"IK retarget chain names mismatch: {sorted(chains)} != {sorted(expected_chains)}"
        )
    for chain_name, expected in expected_chains.items():
        actual = chains[chain_name]
        for field in ("start", "end", "goal"):
            if actual[field].casefold() != expected[field].casefold():
                raise RuntimeError(
                    f"IK chain {chain_name} {field} is {actual[field]}, "
                    f"expected {expected[field]}"
                )

    mesh_editor = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    skeleton_names = {
        str(name).casefold(): str(name)
        for name in mesh.get_editor_property("skeleton").get_reference_pose().get_bone_names()
    }
    for chain_name, expected in expected_chains.items():
        start = expected["start"].casefold()
        current = expected["end"].casefold()
        if start not in skeleton_names or current not in skeleton_names:
            raise RuntimeError(f"IK chain {chain_name} references an unknown master bone")
        while current != start:
            parent = str(mesh_editor.get_bone_parent(mesh, skeleton_names[current])).casefold()
            if not parent or parent == "none" or parent not in skeleton_names:
                raise RuntimeError(
                    f"IK chain {chain_name} end {expected['end']} is not a descendant "
                    f"of {expected['start']}"
                )
            current = parent
    return ik_rig, goals, chains


def _rig_key(name, element_type):
    return unreal.RigElementKey(type=element_type, name=name)


def _add_transform_control(rig, bone_name):
    hierarchy = rig.get_hierarchy()
    controller = rig.get_hierarchy_controller()
    control_name = f"ctrl_{bone_name}"
    control_key = _rig_key(control_name, unreal.RigElementType.CONTROL)
    if hierarchy.contains(control_key):
        return control_key, False

    settings = unreal.RigControlSettings()
    settings.control_type = unreal.RigControlType.EULER_TRANSFORM
    value = hierarchy.make_control_value_from_euler_transform(
        unreal.EulerTransform(scale=[1.0, 1.0, 1.0])
    )
    control_key = controller.add_control(
        control_name,
        unreal.RigElementKey(),
        settings,
        value,
        False,
        False,
    )
    if not control_key:
        raise RuntimeError(f"Could not add Control Rig control: {control_name}")
    bone_key = _rig_key(bone_name, unreal.RigElementType.BONE)
    hierarchy.set_control_offset_transform(
        control_key,
        hierarchy.get_global_transform(bone_key, True),
        True,
    )
    return control_key, True


def _add_float_control(rig, control_name, default_value):
    hierarchy = rig.get_hierarchy()
    controller = rig.get_hierarchy_controller()
    control_key = _rig_key(control_name, unreal.RigElementType.CONTROL)
    if hierarchy.contains(control_key):
        return control_key, False
    settings = unreal.RigControlSettings()
    settings.control_type = unreal.RigControlType.FLOAT
    value = hierarchy.make_control_value_from_float(default_value)
    control_key = controller.add_control(
        control_name,
        unreal.RigElementKey(),
        settings,
        value,
        False,
        False,
    )
    if not control_key:
        raise RuntimeError(f"Could not add Control Rig scalar: {control_name}")
    return control_key, True


def _find_node(model, name):
    for node in model.get_nodes():
        if node.get_name() == name:
            return node
    return None


def _pin_is_linked(pin):
    """Return whether a RigVM pin has any source or target link in UE 5.8.

    URigVMPin::IsLinked is not exported to Python in UE 5.8, while GetLinks is.
    """
    return pin is not None and len(pin.get_links()) > 0


def _pin_name_matches(pin, expected):
    if pin is None:
        return False
    actual = str(pin.get_default_value()).strip()
    if len(actual) >= 2 and actual[0] == actual[-1] and actual[0] in ("'", '"'):
        actual = actual[1:-1].strip()
    return actual.casefold() == expected.casefold()


def _pin_raw_name_matches(pin, expected):
    """FName pins consumed by PBIK must not retain export-text quotes."""
    return pin is not None and str(pin.get_default_value()).strip() == expected


def _pin_bool_matches(pin, expected):
    if pin is None:
        return False
    return str(pin.get_default_value()).strip().casefold() == str(expected).casefold()


def _pin_float_matches(pin, expected, tolerance=1.0e-5):
    if pin is None:
        return False
    try:
        return _float_matches(pin.get_default_value(), expected, tolerance)
    except (TypeError, ValueError):
        return False


def _pbik_defaults_match(pbik):
    """Compare the authored defaults that set Session 2 solver behavior."""
    if not _pin_raw_name_matches(pbik.find_pin("Root"), "pelvis"):
        return False

    effectors_pin = pbik.find_pin("Effectors")
    if effectors_pin is None or effectors_pin.get_array_size() != len(EFFECTORS):
        return False
    for index, (_goal_name, bone_name) in enumerate(EFFECTORS):
        if not _pin_raw_name_matches(
            pbik.find_pin(f"Effectors.{index}.Bone"), bone_name
        ):
            return False
        if not _pin_float_matches(pbik.find_pin(f"Effectors.{index}.ChainDepth"), 2.0):
            return False

    bone_settings_pin = pbik.find_pin("BoneSettings")
    if bone_settings_pin is None or bone_settings_pin.get_array_size() != len(
        BEND_BONE_SETTINGS
    ):
        return False
    for index, (bone_name, preferred_angles) in enumerate(BEND_BONE_SETTINGS):
        prefix = f"BoneSettings.{index}"
        if not _pin_raw_name_matches(pbik.find_pin(f"{prefix}.Bone"), bone_name):
            return False
        if not _pin_bool_matches(
            pbik.find_pin(f"{prefix}.bUsePreferredAngles"), True
        ):
            return False
        for axis, expected in zip("XYZ", preferred_angles):
            if not _pin_float_matches(
                pbik.find_pin(f"{prefix}.PreferredAngles.{axis}"), expected
            ):
                return False

    return (
        _pin_name_matches(pbik.find_pin("Settings.RootBehavior"), "Free")
        and _pin_float_matches(pbik.find_pin("Settings.Iterations"), PBIK_ITERATIONS)
        and _pin_float_matches(
            pbik.find_pin("Settings.SubIterations"), PBIK_SUB_ITERATIONS
        )
        and _pin_float_matches(pbik.find_pin("Settings.GlobalPullChainAlpha"), 0.0)
        and _pin_bool_matches(pbik.find_pin("Settings.bAllowStretch"), False)
    )


def _build_pbik_graph(rig):
    model = rig.get_default_model()
    graph_controller = rig.get_controller_by_name(model.get_name())
    if graph_controller is None:
        raise RuntimeError("Could not obtain the Control Rig VM graph controller")

    changed = False
    begin = _find_node(model, "BeginExecution")
    if begin is None:
        begin = graph_controller.add_unit_node_from_struct_path(
            "/Script/ControlRig.RigUnit_BeginExecution",
            "Execute",
            unreal.Vector2D(-650.0, 0.0),
            "BeginExecution",
            False,
        )
        changed = True
    if begin is None:
        raise RuntimeError("Could not create/find Control Rig Forward Solve event")

    hierarchy = rig.get_hierarchy()
    defaults = unreal.RigUnit_PBIK()
    defaults.set_editor_property("root", "pelvis")
    solver_settings = unreal.PBIKSolverSettings()
    solver_settings.set_editor_property("root_behavior", unreal.PBIKRootBehavior.FREE)
    solver_settings.set_editor_property("global_pull_chain_alpha", 0.0)
    solver_settings.set_editor_property("sub_iterations", 10)
    solver_settings.set_editor_property("allow_stretch", False)
    defaults.set_editor_property("settings", solver_settings)
    pbik_effectors = []
    for _goal_name, bone_name in EFFECTORS:
        effector = unreal.PBIKEffector()
        effector.set_editor_property("bone", bone_name)
        effector.set_editor_property(
            "transform",
            hierarchy.get_global_transform(
                _rig_key(bone_name, unreal.RigElementType.BONE),
                True,
            ),
        )
        effector.set_editor_property("chain_depth", 2)
        pbik_effectors.append(effector)
    defaults.set_editor_property("effectors", pbik_effectors)
    pbik_bone_settings = []
    for bone_name, preferred_angles in BEND_BONE_SETTINGS:
        bone_setting = unreal.PBIKBoneSetting()
        bone_setting.set_editor_property("bone", bone_name)
        bone_setting.set_editor_property("use_preferred_angles", True)
        bone_setting.set_editor_property(
            "preferred_angles", unreal.Vector(*preferred_angles)
        )
        pbik_bone_settings.append(bone_setting)
    defaults.set_editor_property("bone_settings", pbik_bone_settings)

    pbik = _find_node(model, "DGFullBodyIK")
    if pbik is None:
        pbik = graph_controller.add_unit_node_with_defaults(
            defaults.static_struct(),
            defaults.export_text(),
            "Execute",
            unreal.Vector2D(350.0, 0.0),
            "DGFullBodyIK",
            False,
        )
        changed = True
    elif not _pbik_defaults_match(pbik):
        if not graph_controller.set_unit_node_defaults(
            pbik, defaults.export_text(), False, False
        ):
            raise RuntimeError("Could not update the existing Control Rig PBIK defaults")
        changed = True
    if pbik is None:
        raise RuntimeError("Could not create the Control Rig PBIK node")

    # RigUnit_PBIK.export_text() quotes FName values. In UE 5.8 that
    # representation can survive as a literal runtime bone name, making
    # execution report "root bone not set or not found" even though a
    # normalized structural check passes. Force runtime-safe raw names and do
    # not propagate the values across links.
    raw_bone_pins = [("Root", "pelvis")]
    raw_bone_pins.extend(
        (f"Effectors.{index}.Bone", bone_name)
        for index, (_goal_name, bone_name) in enumerate(EFFECTORS)
    )
    raw_bone_pins.extend(
        (f"BoneSettings.{index}.Bone", bone_name)
        for index, (bone_name, _preferred_angles) in enumerate(BEND_BONE_SETTINGS)
    )
    for pin_path, bone_name in raw_bone_pins:
        bone_pin = pbik.find_pin(pin_path)
        if _pin_raw_name_matches(bone_pin, bone_name):
            continue
        if bone_pin is None or not graph_controller.set_pin_default_value(
            bone_pin.get_pin_path(),
            bone_name,
            True,
            False,
            False,
            False,
            False,
        ):
            raise RuntimeError(
                f"Could not set Control Rig PBIK {pin_path} to raw {bone_name}"
            )
        changed = True

    begin_exec = begin.find_pin("ExecuteContext")
    pbik_exec = pbik.find_pin("ExecuteContext")
    if begin_exec and pbik_exec and not _pin_is_linked(pbik_exec):
        if not graph_controller.add_link(
            begin_exec.get_pin_path(), pbik_exec.get_pin_path(), False
        ):
            raise RuntimeError("Could not connect Forward Solve to PBIK")
        changed = True

    for index, (_goal_name, bone_name) in enumerate(EFFECTORS):
        transform_pin = pbik.find_pin(f"Effectors.{index}.Transform")
        if transform_pin is None:
            raise RuntimeError(f"PBIK effectors array is missing index {index}")
        if not _pin_is_linked(transform_pin):
            defaults = unreal.RigUnit_GetTransform()
            defaults.set_editor_property(
                "item", _rig_key(f"ctrl_{bone_name}", unreal.RigElementType.CONTROL)
            )
            defaults.set_editor_property("space", unreal.RigVMTransformSpace.GLOBAL_SPACE)
            defaults.set_editor_property("initial", False)
            get_node = graph_controller.add_unit_node_with_defaults(
                defaults.static_struct(),
                defaults.export_text(),
                "Execute",
                unreal.Vector2D(-250.0, -250.0 + index * 180.0),
                f"Get_{bone_name}_Control",
                False,
            )
            if get_node is None or not graph_controller.add_link(
                get_node.find_pin("Transform").get_pin_path(),
                transform_pin.get_pin_path(),
                False,
            ):
                raise RuntimeError(f"Could not wire {bone_name} control to PBIK")
            changed = True

        if bone_name.startswith("foot_"):
            curve_name = "DG_FootPlant_L" if bone_name.endswith("_l") else "DG_FootPlant_R"
            alpha_pin = pbik.find_pin(f"Effectors.{index}.PositionAlpha")
            rotation_pin = pbik.find_pin(f"Effectors.{index}.RotationAlpha")
            if alpha_pin is None or rotation_pin is None:
                raise RuntimeError(f"PBIK foot alpha pins missing for {bone_name}")
            if not _pin_is_linked(alpha_pin):
                curve_defaults = unreal.RigUnit_GetCurveValue()
                # RigUnit_GetCurveValue.Curve is an FName in UE 5.8 rather
                # than an FRigElementKey.
                curve_defaults.set_editor_property("curve", curve_name)
                curve_node = graph_controller.add_unit_node_with_defaults(
                    curve_defaults.static_struct(),
                    curve_defaults.export_text(),
                    "Execute",
                    unreal.Vector2D(-250.0, 600.0 + index * 160.0),
                    f"Get_{curve_name}",
                    False,
                )
                if curve_node is None:
                    raise RuntimeError(f"Could not create curve reader for {curve_name}")
                value_pin = curve_node.find_pin("Value")
                if not graph_controller.add_link(
                    value_pin.get_pin_path(), alpha_pin.get_pin_path(), False
                ):
                    raise RuntimeError(f"Could not wire {curve_name} to position alpha")
                if not graph_controller.add_link(
                    value_pin.get_pin_path(), rotation_pin.get_pin_path(), False
                ):
                    raise RuntimeError(f"Could not wire {curve_name} to rotation alpha")
                changed = True
        else:
            # Keep the hand effectors dormant by default. Session 3 can drive
            # these alpha controls from authored throw animation without ever
            # introducing a second release authority.
            side = "l" if bone_name.endswith("_l") else "r"
            control_name = f"dg_hand_ik_alpha_{side}"
            alpha_pin = pbik.find_pin(f"Effectors.{index}.PositionAlpha")
            rotation_pin = pbik.find_pin(f"Effectors.{index}.RotationAlpha")
            if alpha_pin is None or rotation_pin is None:
                raise RuntimeError(f"PBIK hand alpha pins missing for {bone_name}")
            if not _pin_is_linked(alpha_pin):
                getter_defaults = unreal.RigUnit_GetControlFloat()
                getter_defaults.set_editor_property("control", control_name)
                getter_node = graph_controller.add_unit_node_with_defaults(
                    getter_defaults.static_struct(),
                    getter_defaults.export_text(),
                    "Execute",
                    unreal.Vector2D(-250.0, -700.0 + index * 160.0),
                    f"Get_{control_name}",
                    False,
                )
                if getter_node is None:
                    raise RuntimeError(f"Could not create hand IK alpha reader {control_name}")
                value_pin = getter_node.find_pin("FloatValue")
                if not graph_controller.add_link(
                    value_pin.get_pin_path(), alpha_pin.get_pin_path(), False
                ):
                    raise RuntimeError(f"Could not wire {control_name} to position alpha")
                if not graph_controller.add_link(
                    value_pin.get_pin_path(), rotation_pin.get_pin_path(), False
                ):
                    raise RuntimeError(f"Could not wire {control_name} to rotation alpha")
                changed = True

    return model, pbik, changed


def _create_control_rig(mesh, skeleton):
    rig = unreal.load_asset(CONTROL_RIG_PATH)
    created = rig is None
    if created:
        rig = unreal.ControlRigBlueprintFactory.create_new_control_rig_asset(
            CONTROL_RIG_PATH,
            False,
        )
    if not isinstance(rig, unreal.ControlRigBlueprint):
        raise RuntimeError(f"Could not create/load Control Rig: {CONTROL_RIG_PATH}")

    changed = created
    if rig.get_preview_mesh() != mesh:
        rig.set_preview_mesh(mesh, False)
        changed = True
    hierarchy_controller = rig.get_hierarchy_controller()
    hierarchy = rig.get_hierarchy()
    if created:
        imported = hierarchy_controller.import_bones_from_asset(
            mesh.get_path_name(), "", True, True, False
        )
        if len(imported) != 69:
            raise RuntimeError(f"Control Rig imported {len(imported)} bones, expected 69")
        changed = True
    existing_curves = {
        str(key.name)
        for key in hierarchy.get_all_keys()
        if key.type == unreal.RigElementType.CURVE
    }
    if not set(CURVES).issubset(existing_curves):
        hierarchy_controller.import_curves_from_asset(skeleton.get_path_name(), "", False)
        changed = True

    for _goal_name, bone_name in EFFECTORS:
        _control, control_created = _add_transform_control(rig, bone_name)
        changed = changed or control_created
    _control, control_created = _add_transform_control(rig, "pelvis")
    changed = changed or control_created
    for control_name, default_value in PROFILE_CONTROLS:
        _control, control_created = _add_float_control(rig, control_name, default_value)
        changed = changed or control_created
    for control_name, default_value in HAND_IK_CONTROLS:
        _control, control_created = _add_float_control(rig, control_name, default_value)
        changed = changed or control_created

    model, pbik, graph_changed = _build_pbik_graph(rig)
    changed = changed or graph_changed
    if changed:
        rig.request_auto_vm_recompilation()
        rig.recompile_vm()
        _save(rig)

    controls = sorted(
        str(key.name)
        for key in hierarchy.get_all_keys()
        if key.type == unreal.RigElementType.CONTROL
    )
    curves = sorted(
        str(key.name)
        for key in hierarchy.get_all_keys()
        if key.type == unreal.RigElementType.CURVE
    )
    nodes = [node.get_name() for node in model.get_nodes()]
    return rig, controls, curves, nodes, pbik.get_name()


def _create_anim_blueprint(mesh, skeleton):
    anim_bp = unreal.load_asset(ANIM_BP_PATH)
    created = anim_bp is None
    if created:
        factory = unreal.AnimBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.DiscGolfAnimInstance.static_class())
        factory.set_editor_property("target_skeleton", skeleton)
        factory.set_editor_property("preview_skeletal_mesh", mesh)
        factory.set_editor_property("template", False)
        anim_bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "ABP_DG_Player",
            "/Game/DiscGolf/Animation",
            unreal.AnimBlueprint,
            factory,
        )
    if not isinstance(anim_bp, unreal.AnimBlueprint):
        raise RuntimeError(f"Could not create/load Animation Blueprint: {ANIM_BP_PATH}")
    if created:
        unreal.BlueprintEditorLibrary.compile_blueprint(anim_bp)
        _save(anim_bp)
    parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(anim_bp)
    if parent != unreal.DiscGolfAnimInstance.static_class():
        raise RuntimeError(f"ABP_DG_Player parent class is {parent}, expected DiscGolfAnimInstance")
    if anim_bp.get_editor_property("target_skeleton") != skeleton:
        raise RuntimeError("ABP_DG_Player target skeleton mismatch")
    return anim_bp


def main():
    if _should_run_validation_only():
        _run_validation_only()
        return

    for folder in (
        "/Game/DiscGolf/Rigs",
        "/Game/DiscGolf/Animation",
        PROFILE_ROOT,
        TEST_PROFILE_ROOT,
    ):
        _ensure_folder(folder)

    mesh = _require_asset(MESH_PATH, unreal.SkeletalMesh)
    skeleton = _require_asset(SKELETON_PATH, unreal.Skeleton)
    registered_curves = _ensure_curves(skeleton)
    profile_paths = _ensure_profiles()
    ik_rig, goals, chains = _create_ik_rig(mesh)
    control_rig, controls, rig_curves, nodes, pbik_node = _create_control_rig(mesh, skeleton)
    anim_bp = _create_anim_blueprint(mesh, skeleton)

    report = {
        "status": "PASS",
        "assets": {
            "skeletal_mesh": MESH_PATH,
            "skeleton": SKELETON_PATH,
            "ik_rig": IK_PATH,
            "control_rig": CONTROL_RIG_PATH,
            "animation_blueprint": ANIM_BP_PATH,
            "profiles": profile_paths,
        },
        "registered_curves": registered_curves,
        "ik": {
            "solver_count": unreal.IKRigController.get_controller(ik_rig).get_num_solvers(),
            "solver_root": str(unreal.IKRigController.get_controller(ik_rig).get_start_bone(0)),
            "retarget_root": str(unreal.IKRigController.get_controller(ik_rig).get_retarget_root()),
            "root_motion_bone": str(unreal.IKRigController.get_controller(ik_rig).get_root_motion_bone()),
            "goals": goals,
            "chains": chains,
        },
        "control_rig": {
            "pbik_node": pbik_node,
            "controls": controls,
            "curves": rig_curves,
            "nodes": nodes,
            "status": str(control_rig.get_editor_property("status")),
        },
        "animation_blueprint": {
            "parent_class": str(
                unreal.BlueprintEditorLibrary.get_blueprint_parent_class(anim_bp).get_path_name()
            ),
            "target_skeleton": skeleton.get_path_name(),
        },
        "gameplay_wiring": "NONE_SESSION_2",
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    _log(
        f"PASS ik={IK_PATH} control_rig={CONTROL_RIG_PATH} anim_bp={ANIM_BP_PATH} "
        f"profiles={len(profile_paths)}"
    )


if __name__ == "__main__":
    main()
