"""Author or validate the one-way MetaHuman-to-DGMaster IK Retargeter.

This script is intentionally fail-closed.  Validation is read-only.  Authoring
requires ``DG_METAHUMAN_TO_DGMASTER_AUTHORING=1``, refuses to overwrite any
existing package, saves exactly one new ``.uasset``, and rolls that newly
created package back if validation or persistence fails.

Run while the project is closed, with an external ``-UserDir`` and log:

    UnrealEditor-Cmd.exe DiscGolfTour.uproject -run=pythonscript \
      -script=Scripts/author_dg_metahuman_to_dgmaster_retargeter.py ...

The asset is an editor pipeline tool.  It does not approve source footage,
performer rights, animation quality, disc contact, or production promotion.
"""

from __future__ import annotations

import hashlib
import json
import math
import os
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).absolute().parents[1]
AUTHOR_ENV = "DG_METAHUMAN_TO_DGMASTER_AUTHORING"

RETARGETER_PACKAGE = (
    "/Game/DiscGolf/Animation/Mocap/Rigs/RTG_MetaHuman_To_DGMaster"
)
RETARGETER_OBJECT = RETARGETER_PACKAGE + ".RTG_MetaHuman_To_DGMaster"
RETARGETER_DISK = (
    PROJECT_ROOT
    / "Content/DiscGolf/Animation/Mocap/Rigs/RTG_MetaHuman_To_DGMaster.uasset"
)

METAHUMAN_BODY = (
    "/Game/DiscGolf/Characters/MetaHuman/Generated/MHC_DG_Golfer_Default/"
    "Body/SKM_MHC_DG_Golfer_Default_BodyMesh."
    "SKM_MHC_DG_Golfer_Default_BodyMesh"
)
METAHUMAN_SKELETON = (
    "/Game/DiscGolf/Characters/MetaHuman/Common/Female/Medium/NormalWeight/"
    "Body/metahuman_base_skel.metahuman_base_skel"
)
METAHUMAN_IK_RIG = (
    "/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig.IK_MH_IKRig"
)
DG_MASTER_MESH = "/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master"
DG_MASTER_SKELETON = (
    "/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master.SKEL_DG_Master"
)
DG_MASTER_IK_RIG = "/Game/DiscGolf/Rigs/IK_DG_Master.IK_DG_Master"
TARGET_RETARGET_POSE = "MetaHumanAligned"

# Each pair is (source MetaHuman chain, target DGMaster chain).  The installed
# MetaHuman IK Rig has no independent foot/toe chains: LeftLeg/RightLeg end at
# ball_l/ball_r.  DGMaster Foot_L/Foot_R therefore remain deliberately unmapped
# and must be handled in the non-destructive foot/contact cleanup stage.
CHAIN_MAPPINGS = (
    ("Root", "Root"),
    ("Spine", "Spine"),
    ("Neck", "Neck"),
    ("LeftArm", "Arm_L"),
    ("RightArm", "Arm_R"),
    ("LeftLeg", "Leg_L"),
    ("RightLeg", "Leg_R"),
)
UNMAPPED_TARGET_CHAINS = ("Foot_L", "Foot_R")

EXPECTED_DG_CHAINS = {
    "Root": ("root", "root", "None"),
    "Spine": ("spine_01", "spine_04", "None"),
    "Neck": ("neck_01", "head", "None"),
    "Arm_L": ("upperarm_l", "hand_l", "hand_l_Goal"),
    "Arm_R": ("upperarm_r", "hand_r", "hand_r_Goal"),
    "Leg_L": ("thigh_l", "foot_l", "foot_l_Goal"),
    "Leg_R": ("thigh_r", "foot_r", "foot_r_Goal"),
    "Foot_L": ("foot_l", "ball_l", "None"),
    "Foot_R": ("foot_r", "ball_r", "None"),
}
EXPECTED_METAHUMAN_CHAINS = {
    "Root": ("root", "root", "None"),
    "Spine": ("spine_01", "spine_05", "None"),
    "Neck": ("neck_01", "neck_02", "None"),
    "Head": ("head", "head", "None"),
    "LeftArm": ("upperarm_l", "hand_l", "LeftHandIK"),
    "RightArm": ("upperarm_r", "hand_r", "RightHandIK"),
    "LeftLeg": ("thigh_l", "ball_l", "LeftFootIK"),
    "RightLeg": ("thigh_r", "ball_r", "RightFootIK"),
}

EXPECTED_OP_CONTROLLERS = (
    "IKRetargetPelvisMotionController",
    "IKRetargetFKChainsController",
    "IKRetargetRunIKRigController",
    "IKRetargetRootMotionController",
    "IKRetargetCurveRemapController",
)

PROTECTED_FILES = (
    PROJECT_ROOT
    / "Content/DiscGolf/Characters/MetaHuman/Generated/MHC_DG_Golfer_Default/"
    "Body/SKM_MHC_DG_Golfer_Default_BodyMesh.uasset",
    PROJECT_ROOT
    / "Content/DiscGolf/Characters/MetaHuman/Common/Female/Medium/NormalWeight/"
    "Body/metahuman_base_skel.uasset",
    PROJECT_ROOT / "Content/DiscGolf/Characters/Meshes/SK_DG_Master.uasset",
    PROJECT_ROOT / "Content/DiscGolf/Characters/Meshes/SKEL_DG_Master.uasset",
    PROJECT_ROOT / "Content/DiscGolf/Rigs/IK_DG_Master.uasset",
    Path(
        "C:/Program Files/Epic Games/UE_5.8/Engine/Plugins/MetaHuman/"
        "MetaHumanCharacter/Content/Animation/Retargeting/IK_MH_IKRig.uasset"
    ),
)


class RetargeterError(RuntimeError):
    """Raised for a fail-closed contract violation."""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def file_record(path: Path) -> dict:
    if not path.is_file():
        raise RetargeterError(f"required exact file is missing: {path}")
    return {
        "path": str(path),
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
    }


def protected_snapshot() -> dict[str, dict]:
    return {str(path): file_record(path) for path in PROTECTED_FILES}


def require_asset(path: str, expected_type):
    asset = unreal.load_asset(path)
    if asset is None or not isinstance(asset, expected_type):
        raise RetargeterError(f"required exact asset/class did not load: {path}")
    if str(asset.get_path_name()) != path:
        raise RetargeterError(f"loaded object path differs from contract: {path}")
    return asset


def require_mesh_contract(path: str, skeleton_path: str):
    mesh = require_asset(path, unreal.SkeletalMesh)
    skeleton = mesh.get_editor_property("skeleton")
    if skeleton is None or str(skeleton.get_path_name()) != skeleton_path:
        raise RetargeterError(f"skeletal mesh uses an unexpected skeleton: {path}")
    return mesh


def rig_chains(controller) -> dict[str, dict[str, str]]:
    result = {}
    for chain in controller.get_retarget_chains():
        # Unreal Python struct members must be read through the reflected
        # property API.  Direct attribute access on FBoneChain can resolve to
        # the struct proxy itself in UE 5.8, silently turning every lookup into
        # an invalid chain name.
        name = str(chain.get_editor_property("chain_name"))
        if not name or name == "None" or name in result:
            raise RetargeterError("IK Rig has a missing or duplicate chain name")
        result[name] = {
            "start_bone": str(controller.get_retarget_chain_start_bone(name)),
            "end_bone": str(controller.get_retarget_chain_end_bone(name)),
            "goal": str(chain.get_editor_property("ik_goal_name")),
        }
    return result


def load_inputs() -> dict:
    source_mesh = require_mesh_contract(METAHUMAN_BODY, METAHUMAN_SKELETON)
    target_mesh = require_mesh_contract(DG_MASTER_MESH, DG_MASTER_SKELETON)
    source_rig = require_asset(METAHUMAN_IK_RIG, unreal.IKRigDefinition)
    target_rig = require_asset(DG_MASTER_IK_RIG, unreal.IKRigDefinition)
    source_controller = unreal.IKRigController.get_controller(source_rig)
    target_controller = unreal.IKRigController.get_controller(target_rig)
    if source_controller is None or target_controller is None:
        raise RetargeterError("an IK Rig controller is unavailable")

    source_chains = rig_chains(source_controller)
    target_chains = rig_chains(target_controller)
    missing_source = sorted(set(EXPECTED_METAHUMAN_CHAINS) - set(source_chains))
    if missing_source:
        raise RetargeterError(f"MetaHuman IK Rig is missing required chains: {missing_source}")
    for name, (start, end, goal) in EXPECTED_METAHUMAN_CHAINS.items():
        row = source_chains[name]
        if (row["start_bone"], row["end_bone"], row["goal"]) != (
            start,
            end,
            goal,
        ):
            raise RetargeterError(
                f"MetaHuman chain {name} differs from {start} -> {end} / {goal}"
            )
    if set(target_chains) != set(EXPECTED_DG_CHAINS):
        raise RetargeterError("DGMaster IK Rig chain set differs from the frozen contract")
    for name, (start, end, goal) in EXPECTED_DG_CHAINS.items():
        row = target_chains[name]
        if (row["start_bone"], row["end_bone"], row["goal"]) != (
            start,
            end,
            goal,
        ):
            raise RetargeterError(
                f"DGMaster chain {name} differs from {start} -> {end} / {goal}"
            )
    if str(source_controller.get_retarget_root()) != "pelvis":
        raise RetargeterError("MetaHuman IK retarget root is not pelvis")
    if str(source_controller.get_root_motion_bone()) != "pelvis":
        raise RetargeterError("MetaHuman IK root-motion bone is not pelvis")
    if str(target_controller.get_retarget_root()) != "pelvis":
        raise RetargeterError("DGMaster IK retarget root is not pelvis")
    if str(target_controller.get_root_motion_bone()) != "root":
        raise RetargeterError("DGMaster IK root-motion bone is not root")
    if not source_controller.is_skeletal_mesh_compatible(source_mesh):
        raise RetargeterError("MetaHuman IK Rig rejects the assembled body mesh")
    if not target_controller.is_skeletal_mesh_compatible(target_mesh):
        raise RetargeterError("DGMaster IK Rig rejects SK_DG_Master")

    return {
        "source_mesh": source_mesh,
        "source_rig": source_rig,
        "source_chains": source_chains,
        "target_mesh": target_mesh,
        "target_rig": target_rig,
        "target_chains": target_chains,
    }


def validate_retargeter(retargeter, inputs: dict) -> dict:
    if not isinstance(retargeter, unreal.IKRetargeter):
        raise RetargeterError("candidate object is not an IKRetargeter")
    if str(retargeter.get_path_name()) != RETARGETER_OBJECT:
        raise RetargeterError("candidate retargeter path differs")
    controller = unreal.IKRetargeterController.get_controller(retargeter)
    if controller is None:
        raise RetargeterError("IK Retargeter controller is unavailable")

    source_side = unreal.RetargetSourceOrTarget.SOURCE
    target_side = unreal.RetargetSourceOrTarget.TARGET
    if controller.get_ik_rig(source_side) != inputs["source_rig"]:
        raise RetargeterError("retargeter source is not IK_MH_IKRig")
    if controller.get_ik_rig(target_side) != inputs["target_rig"]:
        raise RetargeterError("retargeter target is not IK_DG_Master")
    if controller.get_preview_mesh(source_side) != inputs["source_mesh"]:
        raise RetargeterError("retargeter source preview is not the assembled body")
    if controller.get_preview_mesh(target_side) != inputs["target_mesh"]:
        raise RetargeterError("retargeter target preview is not SK_DG_Master")

    op_count = int(controller.get_num_retarget_ops())
    if op_count != len(EXPECTED_OP_CONTROLLERS):
        raise RetargeterError(
            f"retargeter has {op_count} ops, expected {len(EXPECTED_OP_CONTROLLERS)}"
        )
    op_controllers = []
    op_enabled = []
    for index, expected_name in enumerate(EXPECTED_OP_CONTROLLERS):
        op_controller = controller.get_op_controller(index)
        actual_name = (
            op_controller.get_class().get_name() if op_controller is not None else ""
        )
        if actual_name != expected_name:
            raise RetargeterError(
                f"retarget op {index} controller is {actual_name}, expected {expected_name}"
            )
        if not controller.get_retarget_op_enabled(index):
            raise RetargeterError(f"offline retarget op {index} is disabled")
        op_controllers.append(actual_name)
        op_enabled.append(True)

    mappings = []
    for source_chain, target_chain in CHAIN_MAPPINGS:
        actual_source = str(controller.get_source_chain(target_chain))
        if actual_source != source_chain:
            raise RetargeterError(
                f"target chain {target_chain} maps from {actual_source}, "
                f"expected {source_chain}"
            )
        mappings.append({"source": source_chain, "target": target_chain})
    for target_chain in UNMAPPED_TARGET_CHAINS:
        if str(controller.get_source_chain(target_chain)) not in ("", "None"):
            raise RetargeterError(
                f"target foot chain {target_chain} must remain explicitly unmapped"
            )
    for assigned_rig in controller.get_all_target_ik_rigs():
        if assigned_rig != inputs["target_rig"]:
            raise RetargeterError("a retarget op uses an unexpected target IK Rig")

    current_pose = str(controller.get_current_retarget_pose_name(target_side))
    if current_pose != TARGET_RETARGET_POSE:
        raise RetargeterError(
            f"target retarget pose is {current_pose}, expected {TARGET_RETARGET_POSE}"
        )
    root_offset = controller.get_root_offset_in_retarget_pose(target_side)
    root_values = (float(root_offset.x), float(root_offset.y), float(root_offset.z))
    if not all(math.isfinite(value) and abs(value) <= 10000.0 for value in root_values):
        raise RetargeterError("target retarget-pose root offset is non-finite/unbounded")

    return {
        "retargeter": RETARGETER_OBJECT,
        "source_ik_rig": METAHUMAN_IK_RIG,
        "source_preview_mesh": METAHUMAN_BODY,
        "target_ik_rig": DG_MASTER_IK_RIG,
        "target_preview_mesh": DG_MASTER_MESH,
        "target_retarget_pose": TARGET_RETARGET_POSE,
        "op_controllers": op_controllers,
        "op_enabled": op_enabled,
        "explicit_chain_mappings": mappings,
        "unmapped_target_chains": list(UNMAPPED_TARGET_CHAINS),
        "foot_policy": "UNMAPPED_TOE_CHAINS_REQUIRE_NONDESTRUCTIVE_CONTACT_CLEANUP",
        "root_offset_cm": {
            "x": root_values[0],
            "y": root_values[1],
            "z": root_values[2],
        },
    }


def target_sidecars() -> list[Path]:
    stem = RETARGETER_DISK.with_suffix("")
    return [stem.with_suffix(suffix) for suffix in (".uasset", ".uexp", ".ubulk", ".uptnl")]


def assert_target_absent() -> None:
    existing = [str(path) for path in target_sidecars() if path.exists()]
    if existing or unreal.load_asset(RETARGETER_OBJECT) is not None:
        raise RetargeterError(
            "authoring refuses to overwrite/repair an existing target: "
            + repr(existing or [RETARGETER_OBJECT])
        )


def rollback_created_target() -> None:
    # The preflight proves the target did not exist before this process.  This
    # exact deletion can therefore affect only the failed asset created here.
    if unreal.EditorAssetLibrary.does_asset_exist(RETARGETER_PACKAGE):
        if not unreal.EditorAssetLibrary.delete_asset(RETARGETER_PACKAGE):
            raise RetargeterError("failed to roll back the newly created target asset")
    leftovers = [str(path) for path in target_sidecars() if path.exists()]
    if leftovers:
        raise RetargeterError(f"new target rollback left files behind: {leftovers}")


def author(inputs: dict):
    assert_target_absent()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    retargeter = asset_tools.create_asset(
        "RTG_MetaHuman_To_DGMaster",
        "/Game/DiscGolf/Animation/Mocap/Rigs",
        unreal.IKRetargeter,
        unreal.IKRetargetFactory(),
    )
    if retargeter is None or str(retargeter.get_path_name()) != RETARGETER_OBJECT:
        raise RetargeterError("UE failed to create the exact retargeter object")

    controller = unreal.IKRetargeterController.get_controller(retargeter)
    if controller is None:
        raise RetargeterError("new retargeter controller is unavailable")
    retargeter.modify()
    source_side = unreal.RetargetSourceOrTarget.SOURCE
    target_side = unreal.RetargetSourceOrTarget.TARGET
    controller.set_ik_rig(source_side, inputs["source_rig"])
    controller.set_ik_rig(target_side, inputs["target_rig"])
    controller.set_preview_mesh(source_side, inputs["source_mesh"])
    controller.set_preview_mesh(target_side, inputs["target_mesh"])
    controller.add_default_ops()
    controller.assign_ik_rig_to_all_ops(source_side, inputs["source_rig"])
    controller.assign_ik_rig_to_all_ops(target_side, inputs["target_rig"])
    controller.auto_map_chains(unreal.AutoMapChainType.CLEAR, True)
    for source_chain, target_chain in CHAIN_MAPPINGS:
        if not controller.set_source_chain(source_chain, target_chain):
            raise RetargeterError(
                f"UE rejected explicit mapping {source_chain} -> {target_chain}"
            )
    created_pose = str(
        controller.create_retarget_pose(TARGET_RETARGET_POSE, target_side)
    )
    if created_pose != TARGET_RETARGET_POSE:
        raise RetargeterError(
            f"UE created unexpected target retarget pose: {created_pose}"
        )
    if not controller.set_current_retarget_pose(TARGET_RETARGET_POSE, target_side):
        raise RetargeterError("UE rejected the target retarget pose selection")
    controller.auto_align_all_bones(target_side)

    validation = validate_retargeter(retargeter, inputs)
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        retargeter, only_if_is_dirty=False
    ):
        raise RetargeterError("UE refused the exact one-asset save")
    if not RETARGETER_DISK.is_file():
        raise RetargeterError("saved retargeter package is absent on disk")
    unexpected_sidecars = [
        str(path)
        for path in target_sidecars()
        if path != RETARGETER_DISK and path.exists()
    ]
    if unexpected_sidecars:
        raise RetargeterError(
            f"retargeter save created unexpected sidecars: {unexpected_sidecars}"
        )
    validation["package"] = file_record(RETARGETER_DISK)
    return retargeter, validation


def emit(payload: dict) -> None:
    unreal.log(
        "DG_METAHUMAN_TO_DGMASTER_RETARGETER="
        + json.dumps(payload, sort_keys=True)
    )


def main() -> None:
    before = protected_snapshot()
    inputs = load_inputs()
    existing = unreal.load_asset(RETARGETER_OBJECT)
    if existing is not None:
        validation = validate_retargeter(existing, inputs)
        if protected_snapshot() != before:
            raise RetargeterError("read-only validation changed a protected input")
        emit(
            {
                "status": "PASS_ALREADY_CURRENT_NO_ASSET_WRITES",
                "disk_mutation": "NONE",
                "production_approval": False,
                "validation": validation,
            }
        )
        return

    if os.environ.get(AUTHOR_ENV) != "1":
        emit(
            {
                "status": "BLOCKED_ASSET_ABSENT_AUTHORING_NOT_AUTHORIZED",
                "disk_mutation": "NONE",
                "required_environment": f"{AUTHOR_ENV}=1",
                "planned_asset": RETARGETER_OBJECT,
                "production_approval": False,
            }
        )
        raise RetargeterError(
            f"{RETARGETER_OBJECT} is absent; set exact {AUTHOR_ENV}=1 to author it"
        )

    created = False
    try:
        retargeter, validation = author(inputs)
        created = True
        after = protected_snapshot()
        if after != before:
            raise RetargeterError("authoring changed a protected input")
        emit(
            {
                "status": "PASS_AUTHORED_METAHUMAN_TO_DGMASTER_RETARGETER",
                "disk_mutation": "EXACTLY_ONE_NEW_RETARGETER_PACKAGE",
                "production_approval": False,
                "validation": validation,
            }
        )
        # Keep the UAsset alive through result emission.
        _ = retargeter
    except Exception:
        if created or unreal.EditorAssetLibrary.does_asset_exist(RETARGETER_PACKAGE):
            rollback_created_target()
        if protected_snapshot() != before:
            raise RetargeterError(
                "authoring failure also changed a protected input; manual audit required"
            )
        raise


main()
