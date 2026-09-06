"""Read-only UE 5.8 inspection of the MetaHuman-to-DGMaster retargeter.

This inspector fail-closes on its frozen bindings and per-operation chain maps,
then reports serialized operation settings and retarget-pose facts that the
author/cold-reload check intentionally does not treat as animation-quality
approval.  It never creates, modifies, saves, renames, or deletes project
content and only accepts the isolated host-runner invocation contract.
"""

from __future__ import annotations

import json
import math
import re
import uuid
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
EXPECTED_PROJECT_ROOT = Path(r"C:\DGTour").resolve()
SCRIPT_PATH = Path(__file__).resolve()
EXPECTED_USER_ROOT = Path(
    r"C:\DGTour_TestRuns\MotionRetargetInspection"
).resolve()
INSPECTION_SWITCH = "-DGMetaHumanToDGMasterRetargeterInspect"

RETARGETER_PATH = (
    "/Game/DiscGolf/Animation/Mocap/Rigs/RTG_MetaHuman_To_DGMaster."
    "RTG_MetaHuman_To_DGMaster"
)
SOURCE_IK_RIG_PATH = (
    "/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig.IK_MH_IKRig"
)
TARGET_IK_RIG_PATH = "/Game/DiscGolf/Rigs/IK_DG_Master.IK_DG_Master"
SOURCE_PREVIEW_MESH_PATH = (
    "/Game/DiscGolf/Characters/MetaHuman/Generated/MHC_DG_Golfer_Default/"
    "Body/SKM_MHC_DG_Golfer_Default_BodyMesh."
    "SKM_MHC_DG_Golfer_Default_BodyMesh"
)
TARGET_PREVIEW_MESH_PATH = (
    "/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master"
)
TARGET_CHAINS = (
    "Root",
    "Spine",
    "Neck",
    "Arm_L",
    "Arm_R",
    "Leg_L",
    "Leg_R",
    "Foot_L",
    "Foot_R",
)
EXPECTED_MAPPING_BY_TARGET = {
    "Root": "Root",
    "Spine": "Spine",
    "Neck": "Neck",
    "Arm_L": "LeftArm",
    "Arm_R": "RightArm",
    "Leg_L": "LeftLeg",
    "Leg_R": "RightLeg",
    "Foot_L": "None",
    "Foot_R": "None",
}
EXPECTED_CONTROLLERS = (
    "IKRetargetPelvisMotionController",
    "IKRetargetFKChainsController",
    "IKRetargetRunIKRigController",
    "IKRetargetRootMotionController",
    "IKRetargetCurveRemapController",
)


def exact_switch_count(command_line: str, switch: str) -> int:
    return len(
        re.findall(
            rf'(?i)(?:^|\s|"){re.escape(switch)}(?=$|\s|")',
            command_line,
        )
    )


def command_values(command_line: str, name: str) -> list[str]:
    pattern = re.compile(
        rf'(?i)(?:^|\s)(?:"-{re.escape(name)}=([^"]+)"|'
        rf'-{re.escape(name)}="([^"]+)"|-{re.escape(name)}=([^\s"]+))'
        rf'(?=$|\s)'
    )
    return [next(value for value in match if value) for match in pattern.findall(command_line)]


def validate_invocation(command_line: str) -> Path:
    if PROJECT_ROOT != EXPECTED_PROJECT_ROOT:
        raise RuntimeError(
            f"project root {PROJECT_ROOT} is not the frozen root {EXPECTED_PROJECT_ROOT}"
        )
    required_once = (INSPECTION_SWITCH, "-unattended", "-nop4", "-NullRHI")
    wrong_counts = {
        switch: exact_switch_count(command_line, switch)
        for switch in required_once
        if exact_switch_count(command_line, switch) != 1
    }
    if wrong_counts:
        raise RuntimeError(
            f"required inspection switches must occur exactly once: {wrong_counts}"
        )
    if re.search(
        r'(?i)(?:^|\s|")-ExecutePythonScript(?:=|\s|$|")', command_line
    ):
        raise RuntimeError(
            "interactive ExecutePythonScript is outside the inspection contract"
        )
    run_values = command_values(command_line, "run")
    if len(run_values) != 1 or run_values[0].casefold() != "pythonscript":
        raise RuntimeError("inspection requires exactly one -run=PythonScript")
    script_values = command_values(command_line, "script")
    if len(script_values) != 1 or Path(script_values[0]).resolve() != SCRIPT_PATH:
        raise RuntimeError(
            "inspection requires exactly one -script value naming this exact file"
        )
    user_values = command_values(command_line, "UserDir")
    if len(user_values) != 1:
        raise RuntimeError(
            "inspection requires exactly one absolute -UserDir=<UUID path>"
        )
    raw_user_dir = Path(user_values[0])
    if not raw_user_dir.is_absolute():
        raise RuntimeError("inspection UserDir must be absolute")
    user_dir = raw_user_dir.resolve()
    try:
        relative = user_dir.relative_to(EXPECTED_USER_ROOT)
    except ValueError as exc:
        raise RuntimeError(
            f"inspection UserDir is outside {EXPECTED_USER_ROOT}: {user_dir}"
        ) from exc
    if len(relative.parts) != 1:
        raise RuntimeError("inspection UserDir must be one direct UUID child")
    try:
        parsed = uuid.UUID(relative.name)
    except ValueError as exc:
        raise RuntimeError("inspection UserDir child is not a UUID") from exc
    if str(parsed) != relative.name or not user_dir.is_dir():
        raise RuntimeError(
            "inspection UserDir UUID must be canonical, lowercase, and active"
        )
    try:
        user_dir.relative_to(PROJECT_ROOT)
    except ValueError:
        pass
    else:
        raise RuntimeError("inspection UserDir must remain external to the project")
    return user_dir


def object_path(value) -> str | None:
    return str(value.get_path_name()) if value is not None else None


def require_object_path(value, expected: str, label: str) -> str:
    actual = object_path(value)
    if actual != expected:
        raise RuntimeError(f"{label} is {actual!r}, expected {expected!r}")
    return actual


def require_finite_numbers(value, label: str = "payload") -> None:
    if isinstance(value, bool) or value is None or isinstance(value, str):
        return
    if isinstance(value, (int, float)):
        if not math.isfinite(float(value)):
            raise RuntimeError(f"{label} contains a non-finite numeric value")
        return
    if isinstance(value, dict):
        for key, item in value.items():
            require_finite_numbers(item, f"{label}.{key}")
        return
    if isinstance(value, (list, tuple)):
        for index, item in enumerate(value):
            require_finite_numbers(item, f"{label}[{index}]")
        return
    raise RuntimeError(f"{label} contains unsupported value type {type(value).__name__}")


def bone_name(reference) -> str:
    return str(reference.get_editor_property("bone_name"))


def vector(value) -> dict:
    return {"x": float(value.x), "y": float(value.y), "z": float(value.z)}


def rotator(value) -> dict:
    return {
        "pitch": float(value.pitch),
        "yaw": float(value.yaw),
        "roll": float(value.roll),
    }


def transform(value) -> dict:
    return {
        "translation": vector(value.translation),
        "rotation": {
            "x": float(value.rotation.x),
            "y": float(value.rotation.y),
            "z": float(value.rotation.z),
            "w": float(value.rotation.w),
        },
        "scale3d": vector(value.scale3d),
    }


def pelvis_record(op_controller) -> dict:
    settings = op_controller.get_settings()
    return {
        "source_pelvis_bone": bone_name(
            settings.get_editor_property("source_pelvis_bone")
        ),
        "target_pelvis_bone": bone_name(
            settings.get_editor_property("target_pelvis_bone")
        ),
        "blend_to_absolute_offset": vector(
            settings.get_editor_property("blend_to_absolute_offset")
        ),
        "floor_constraint_weight": float(
            settings.get_editor_property("floor_constraint_weight")
        ),
        "source_crotch_offset": float(
            settings.get_editor_property("source_crotch_offset")
        ),
        "target_crotch_offset": float(
            settings.get_editor_property("target_crotch_offset")
        ),
        "use_ground_falloff": bool(
            settings.get_editor_property("use_ground_falloff")
        ),
        "ground_falloff_height_percent": float(
            settings.get_editor_property("ground_falloff_height_percent")
        ),
        "rotation_alpha": float(settings.get_editor_property("rotation_alpha")),
        "rotation_offset_local": rotator(
            settings.get_editor_property("rotation_offset_local")
        ),
        "rotation_offset_global": rotator(
            settings.get_editor_property("rotation_offset_global")
        ),
        "translation_alpha": float(
            settings.get_editor_property("translation_alpha")
        ),
        "translation_offset_local": vector(
            settings.get_editor_property("translation_offset_local")
        ),
        "translation_offset_global": vector(
            settings.get_editor_property("translation_offset_global")
        ),
        "blend_to_source_translation": float(
            settings.get_editor_property("blend_to_source_translation")
        ),
        "blend_to_source_translation_weights": vector(
            settings.get_editor_property("blend_to_source_translation_weights")
        ),
        "scale_horizontal": float(
            settings.get_editor_property("scale_horizontal")
        ),
        "scale_vertical": float(settings.get_editor_property("scale_vertical")),
        "affect_ik_horizontal": float(
            settings.get_editor_property("affect_ik_horizontal")
        ),
        "affect_ik_vertical": float(
            settings.get_editor_property("affect_ik_vertical")
        ),
    }


def fk_record(op_controller) -> dict:
    settings = op_controller.get_settings()
    rows = []
    for chain in settings.get_editor_property("chains_to_retarget"):
        rows.append(
            {
                "target_chain": str(
                    chain.get_editor_property("target_chain_name")
                ),
                "enable_fk": bool(chain.get_editor_property("enable_fk")),
                "rotation_mode": str(
                    chain.get_editor_property("rotation_mode")
                ),
                "rotation_alpha": float(
                    chain.get_editor_property("rotation_alpha")
                ),
                "translation_mode": str(
                    chain.get_editor_property("translation_mode")
                ),
                "translation_alpha": float(
                    chain.get_editor_property("translation_alpha")
                ),
            }
        )
    rig = settings.get_editor_property("ik_rig_asset")
    return {
        "target_ik_rig": str(rig.get_path_name()) if rig else None,
        "chains": sorted(rows, key=lambda row: row["target_chain"]),
    }


def run_ik_record(op_controller) -> dict:
    settings = op_controller.get_settings()
    rows = []
    for chain in settings.get_editor_property("chains"):
        rows.append(
            {
                "target_chain": str(
                    chain.get_editor_property("target_chain_name")
                ),
                "enable_ik": bool(chain.get_editor_property("enable_ik")),
                "chain_position_alpha": float(
                    chain.get_editor_property("chain_position_alpha")
                ),
                "chain_rotation_alpha": float(
                    chain.get_editor_property("chain_rotation_alpha")
                ),
                "override_goal_alpha": bool(
                    chain.get_editor_property("override_goal_alpha")
                ),
                "final_position_alpha": float(
                    chain.get_editor_property("final_position_alpha")
                ),
                "final_rotation_alpha": float(
                    chain.get_editor_property("final_rotation_alpha")
                ),
            }
        )
    rig = settings.get_editor_property("ik_rig_asset")
    return {
        "target_ik_rig": str(rig.get_path_name()) if rig else None,
        "chains": sorted(rows, key=lambda row: row["target_chain"]),
    }


def root_motion_record(op_controller) -> dict:
    settings = op_controller.get_settings()
    return {
        "source_root": bone_name(settings.get_editor_property("source_root")),
        "target_root": bone_name(settings.get_editor_property("target_root")),
        "target_pelvis": bone_name(
            settings.get_editor_property("target_pelvis")
        ),
        "root_motion_source": str(
            settings.get_editor_property("root_motion_source")
        ),
        "rotate_with_pelvis": bool(
            settings.get_editor_property("rotate_with_pelvis")
        ),
        "root_height_source": str(
            settings.get_editor_property("root_height_source")
        ),
        "global_offset": transform(settings.get_editor_property("global_offset")),
        "maintain_offset_from_pelvis": bool(
            settings.get_editor_property("maintain_offset_from_pelvis")
        ),
        "propagate_to_non_retargeted_children": bool(
            settings.get_editor_property(
                "propagate_to_non_retargeted_children"
            )
        ),
    }


def curve_record(op_controller) -> dict:
    settings = op_controller.get_settings()
    pairs = []
    for pair in settings.get_editor_property("curves_to_remap"):
        pairs.append(
            {
                "source": str(pair.get_editor_property("source_curve")),
                "target": str(pair.get_editor_property("target_curve")),
            }
        )
    return {
        "copy_all_source_curves": bool(
            settings.get_editor_property("copy_all_source_curves")
        ),
        "remap_curves": bool(settings.get_editor_property("remap_curves")),
        "pairs": pairs,
    }


command_line = unreal.SystemLibrary.get_command_line()
user_dir = validate_invocation(command_line)

retargeter = unreal.load_asset(RETARGETER_PATH)
if retargeter is None or str(retargeter.get_path_name()) != RETARGETER_PATH:
    raise RuntimeError(f"required exact retargeter did not load: {RETARGETER_PATH}")
controller = unreal.IKRetargeterController.get_controller(retargeter)
if controller is None:
    raise RuntimeError("IK Retargeter controller unavailable")
if int(controller.get_num_retarget_ops()) != len(EXPECTED_CONTROLLERS):
    raise RuntimeError("retarget operation count differs from the inspection contract")

source_side = unreal.RetargetSourceOrTarget.SOURCE
target_side = unreal.RetargetSourceOrTarget.TARGET
source_ik_rig = require_object_path(
    controller.get_ik_rig(source_side),
    SOURCE_IK_RIG_PATH,
    "source IK Rig",
)
target_ik_rig = require_object_path(
    controller.get_ik_rig(target_side),
    TARGET_IK_RIG_PATH,
    "target IK Rig",
)
source_preview_mesh = require_object_path(
    controller.get_preview_mesh(source_side),
    SOURCE_PREVIEW_MESH_PATH,
    "source preview mesh",
)
target_preview_mesh = require_object_path(
    controller.get_preview_mesh(target_side),
    TARGET_PREVIEW_MESH_PATH,
    "target preview mesh",
)
op_target_ik_rigs = [
    object_path(rig) for rig in controller.get_all_target_ik_rigs()
]
if not op_target_ik_rigs or any(
    path != TARGET_IK_RIG_PATH for path in op_target_ik_rigs
):
    raise RuntimeError(
        "retarget operations do not exclusively reference the exact target IK Rig: "
        f"{op_target_ik_rigs}"
    )

operations = []
settings_records = {}
operation_names = set()
for index, expected_class in enumerate(EXPECTED_CONTROLLERS):
    op_controller = controller.get_op_controller(index)
    actual_class = op_controller.get_class().get_name() if op_controller else ""
    if actual_class != expected_class:
        raise RuntimeError(
            f"retarget operation {index} is {actual_class}, expected {expected_class}"
        )
    op_name = str(controller.get_op_name(index))
    normalized_name = op_name.casefold()
    if not op_name or normalized_name == "none" or op_name != op_name.strip():
        raise RuntimeError(f"retarget operation {index} has an invalid empty name")
    if normalized_name in operation_names:
        raise RuntimeError(f"retarget operation name is not unique: {op_name!r}")
    operation_names.add(normalized_name)
    enabled = bool(controller.get_retarget_op_enabled(index))
    if not enabled:
        raise RuntimeError(f"retarget operation {index} {op_name!r} is disabled")
    operations.append(
        {
            "index": index,
            "name": op_name,
            "controller_class": actual_class,
            "enabled": enabled,
        }
    )
    if index == 0:
        settings_records[op_name] = pelvis_record(op_controller)
    elif index == 1:
        settings_records[op_name] = fk_record(op_controller)
    elif index == 2:
        settings_records[op_name] = run_ik_record(op_controller)
    elif index == 3:
        settings_records[op_name] = root_motion_record(op_controller)
    elif index == 4:
        settings_records[op_name] = curve_record(op_controller)

mapping_by_operation = {}
for operation in operations:
    if operation["index"] not in (1, 2):
        continue
    mapping = {
        target: str(controller.get_source_chain(target, operation["name"]))
        for target in TARGET_CHAINS
    }
    if mapping != EXPECTED_MAPPING_BY_TARGET:
        differences = {
            target: {
                "actual": mapping.get(target),
                "expected": EXPECTED_MAPPING_BY_TARGET[target],
            }
            for target in TARGET_CHAINS
            if mapping.get(target) != EXPECTED_MAPPING_BY_TARGET[target]
        }
        raise RuntimeError(
            f"retarget operation {operation['name']!r} mapping differs: {differences}"
        )
    mapping_by_operation[operation["name"]] = mapping

payload = {
    "status": "INSPECTION_COMPLETE_RELEASE_BLOCKED",
    "disk_mutation": "NONE",
    "mutation_policy": "NO_CREATE_NO_MODIFY_NO_SAVE_NO_RENAME_NO_DELETE",
    "invocation_switch": INSPECTION_SWITCH,
    "external_user_dir": str(user_dir),
    "retargeter": RETARGETER_PATH,
    "source_ik_rig": source_ik_rig,
    "target_ik_rig": target_ik_rig,
    "source_preview_mesh": source_preview_mesh,
    "target_preview_mesh": target_preview_mesh,
    "op_target_ik_rigs": sorted(set(op_target_ik_rigs)),
    "operations": operations,
    "operation_names_unique_nonempty": True,
    "settings": settings_records,
    "mapping_by_operation": mapping_by_operation,
    "per_operation_mapping_contract_validated": True,
    "source_pose": str(controller.get_current_retarget_pose_name(source_side)),
    "target_pose": str(controller.get_current_retarget_pose_name(target_side)),
    "source_pose_root_offset_cm": vector(
        controller.get_root_offset_in_retarget_pose(source_side)
    ),
    "target_pose_root_offset_cm": vector(
        controller.get_root_offset_in_retarget_pose(target_side)
    ),
    "known_unproved_boundaries": [
        "SOURCE_HEAD_CHAIN_IS_NOT_MAPPED_TO_AN_INDEPENDENT_TARGET_HEAD_CHAIN",
        "SOURCE_LEG_ENDS_AT_BALL_WHILE_TARGET_LEG_ENDS_AT_FOOT",
        "TARGET_FOOT_CHAINS_ARE_UNMAPPED",
        "ROOT_AND_PELVIS_BEHAVIOR_REQUIRES_A_SOLVED_CLIP_EXPORT_TEST",
        "NO_ANIMATION_OR_CONTACT_QUALITY_APPROVAL",
    ],
    "numeric_values_finite": True,
    "production_approval": False,
}
require_finite_numbers(payload)
print(
    "DG_METAHUMAN_TO_DGMASTER_INSPECTION="
    + json.dumps(payload, sort_keys=True, allow_nan=False)
)
