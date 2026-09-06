import json

import unreal


RETARGETER_PATH = (
    "/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman."
    "RTG_DGMaster_To_MetaHuman"
)


def vector(value):
    return {
        "x": float(value.x),
        "y": float(value.y),
        "z": float(value.z),
    }


def quaternion(value):
    return {
        "x": float(value.x),
        "y": float(value.y),
        "z": float(value.z),
        "w": float(value.w),
    }


retargeter = unreal.load_asset(RETARGETER_PATH)
if retargeter is None:
    raise RuntimeError(f"Could not load {RETARGETER_PATH}")

controller = unreal.IKRetargeterController.get_controller(retargeter)
if controller is None or controller.get_num_retarget_ops() < 1:
    raise RuntimeError("Could not read canonical retargeter controller/op stack")

op_controller = controller.get_op_controller(0)
if op_controller is None or not hasattr(op_controller, "get_settings"):
    raise RuntimeError("Canonical op zero has no readable settings controller")

settings = op_controller.get_settings()
root_motion_controller = controller.get_op_controller(3)
if root_motion_controller is None or not hasattr(
    root_motion_controller, "get_settings"
):
    raise RuntimeError("Canonical op three has no readable settings controller")
root_motion_settings = root_motion_controller.get_settings()
source_bone = settings.get_editor_property("source_pelvis_bone")
target_bone = settings.get_editor_property("target_pelvis_bone")
source_root_offset = controller.get_root_offset_in_retarget_pose(
    unreal.RetargetSourceOrTarget.SOURCE
)
target_root_offset = controller.get_root_offset_in_retarget_pose(
    unreal.RetargetSourceOrTarget.TARGET
)
target_pose = controller.get_current_retarget_pose(
    unreal.RetargetSourceOrTarget.TARGET
)
target_rotation_offsets = target_pose.get_editor_property(
    "bone_rotation_offsets"
)
target_rotation_offsets_by_name = {
    str(name): quaternion(rotation)
    for name, rotation in target_rotation_offsets.items()
}

payload = {
    "retargeter": retargeter.get_path_name(),
    "op_count": int(controller.get_num_retarget_ops()),
    "op_zero_controller_class": op_controller.get_class().get_path_name(),
    "op_three_controller_class": (
        root_motion_controller.get_class().get_path_name()
    ),
    "source_pelvis_bone": str(source_bone.get_editor_property("bone_name")),
    "target_pelvis_bone": str(target_bone.get_editor_property("bone_name")),
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
    "source_retarget_pose": str(
        controller.get_current_retarget_pose_name(
            unreal.RetargetSourceOrTarget.SOURCE
        )
    ),
    "target_retarget_pose": str(
        controller.get_current_retarget_pose_name(
            unreal.RetargetSourceOrTarget.TARGET
        )
    ),
    "source_retarget_pose_root_offset_cm": vector(source_root_offset),
    "target_retarget_pose_root_offset_cm": vector(target_root_offset),
    "target_retarget_pose_rotation_offset_count": len(
        target_rotation_offsets_by_name
    ),
    "target_retarget_pose_rotation_offsets_by_name": (
        target_rotation_offsets_by_name
    ),
    "target_retarget_pose_root_rotation_offset": (
        target_rotation_offsets_by_name.get("root")
        or target_rotation_offsets_by_name.get("Root")
    ),
    "target_retarget_pose_pelvis_rotation_offset": (
        target_rotation_offsets_by_name.get("pelvis")
        or target_rotation_offsets_by_name.get("Pelvis")
    ),
    "root_motion_source_root": str(
        root_motion_controller.get_source_root_bone()
    ),
    "root_motion_target_root": str(
        root_motion_controller.get_target_root_bone()
    ),
    "root_motion_target_pelvis": str(
        root_motion_controller.get_target_pelvis_bone()
    ),
    "root_motion_source": str(
        root_motion_settings.get_editor_property("root_motion_source")
    ),
    "root_motion_height_source": str(
        root_motion_settings.get_editor_property("root_height_source")
    ),
    "root_motion_propagate_to_non_retargeted_children": bool(
        root_motion_settings.get_editor_property(
            "propagate_to_non_retargeted_children"
        )
    ),
    "root_motion_enabled": bool(controller.get_retarget_op_enabled(3)),
}

unreal.log("DG_SESSION8B_PELVIS_SETTINGS: " + json.dumps(payload, sort_keys=True))
