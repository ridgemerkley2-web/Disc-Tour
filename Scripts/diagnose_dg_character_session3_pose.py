"""Read-only UE 5.8 probe for the Session 3 authored animation pose basis."""

import json
import math
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT_PATH = PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session3PoseDiagnosis.json"


def _names(values):
    return [str(value) for value in values]


def _public(value, contains=""):
    needle = contains.casefold()
    return sorted(
        name
        for name in dir(value)
        if not name.startswith("_") and needle in name.casefold()
    )


def _transform(value):
    translation = value.translation
    rotation = value.rotation
    scale = value.scale3d
    return {
        "translation": [float(translation.x), float(translation.y), float(translation.z)],
        "rotation": [float(rotation.x), float(rotation.y), float(rotation.z), float(rotation.w)],
        "scale": [float(scale.x), float(scale.y), float(scale.z)],
    }


def _distance(left, right):
    a = left.translation
    b = right.translation
    return math.sqrt(
        (float(a.x) - float(b.x)) ** 2
        + (float(a.y) - float(b.y)) ** 2
        + (float(a.z) - float(b.z)) ** 2
    )


def _pose_sample(pose, bones):
    result = {}
    for bone in bones:
        result[bone] = {
            "local": _transform(
                pose.get_bone_pose(bone, unreal.AnimPoseSpaces.LOCAL)
            ),
            "world": _transform(
                pose.get_bone_pose(bone, unreal.AnimPoseSpaces.WORLD)
            ),
        }
    return result


def _connectivity(pose, reference_pose, pairs):
    result = {}
    for parent, child in pairs:
        parent_pose = pose.get_bone_pose(parent, unreal.AnimPoseSpaces.WORLD)
        child_pose = pose.get_bone_pose(child, unreal.AnimPoseSpaces.WORLD)
        parent_ref = reference_pose.get_bone_pose(parent, unreal.AnimPoseSpaces.WORLD)
        child_ref = reference_pose.get_bone_pose(child, unreal.AnimPoseSpaces.WORLD)
        posed_distance = _distance(parent_pose, child_pose)
        reference_distance = _distance(parent_ref, child_ref)
        result[f"{parent}->{child}"] = {
            "reference_cm": reference_distance,
            "posed_cm": posed_distance,
            "ratio": posed_distance / reference_distance if reference_distance else None,
        }
    return result


def main():
    skeleton = unreal.load_asset("/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master")
    mesh = unreal.load_asset("/Game/DiscGolf/Characters/Meshes/SK_DG_Master")
    sequence = unreal.load_asset("/Game/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype")
    if not skeleton or not mesh or not sequence:
        raise RuntimeError("Session 3 assets did not load")

    reference_pose = skeleton.get_reference_pose()
    report = {
        "status": "READ_ONLY_PROBE",
        "reference_pose_type": str(type(reference_pose)),
        "reference_pose_methods": _public(reference_pose),
        "sequence_methods_pose": _public(sequence, "pose"),
        "sequence_methods_model": _public(sequence, "model"),
        "anim_pose_types": _names(
            name for name in dir(unreal) if "animpose" in name.casefold()
        ),
        "reference_pose_get_bone_pose_doc": str(
            getattr(getattr(reference_pose, "get_bone_pose", None), "__doc__", "")
        ),
        "sequence_get_anim_pose_at_frame_doc": str(
            getattr(getattr(sequence, "get_anim_pose_at_frame", None), "__doc__", "")
        ),
        "mesh_bounds": {
            "origin": [
                float(mesh.get_bounds().origin.x),
                float(mesh.get_bounds().origin.y),
                float(mesh.get_bounds().origin.z),
            ],
            "extent": [
                float(mesh.get_bounds().box_extent.x),
                float(mesh.get_bounds().box_extent.y),
                float(mesh.get_bounds().box_extent.z),
            ],
        },
    }

    bone_names = _names(reference_pose.get_bone_names())
    report["bone_count"] = len(bone_names)
    report["bone_names"] = bone_names

    get_pose = getattr(reference_pose, "get_bone_pose", None)
    if get_pose:
        poses = {}
        for bone in (
            "root", "pelvis", "spine_01", "spine_04", "clavicle_r",
            "upperarm_r", "lowerarm_r", "hand_r", "disc_grip_r",
            "thigh_l", "calf_l", "foot_l",
        ):
            try:
                poses[bone] = _transform(get_pose(bone))
            except Exception as error:
                poses[bone] = {"error": repr(error)}
        report["reference_bone_poses"] = poses

    sample_bones = (
        "root", "pelvis", "spine_01", "spine_04", "clavicle_r",
        "upperarm_r", "lowerarm_r", "hand_r", "disc_grip_r",
        "thigh_l", "calf_l", "foot_l",
    )
    connectivity_pairs = (
        ("root", "pelvis"),
        ("pelvis", "spine_01"),
        ("spine_04", "clavicle_r"),
        ("clavicle_r", "upperarm_r"),
        ("upperarm_r", "lowerarm_r"),
        ("lowerarm_r", "hand_r"),
        ("hand_r", "disc_grip_r"),
        ("pelvis", "thigh_l"),
        ("thigh_l", "calf_l"),
        ("calf_l", "foot_l"),
    )
    report["reference_pose_samples"] = _pose_sample(reference_pose, sample_bones)
    report["reference_connectivity"] = _connectivity(
        reference_pose, reference_pose, connectivity_pairs
    )

    evaluated = {}
    for eval_name, eval_type in (
        ("source", unreal.AnimDataEvalType.SOURCE),
        ("raw", unreal.AnimDataEvalType.RAW),
        ("compressed", unreal.AnimDataEvalType.COMPRESSED),
    ):
        options = unreal.AnimPoseEvaluationOptions()
        options.set_editor_property("evaluation_type", eval_type)
        options.set_editor_property("optional_skeletal_mesh", mesh)
        options.set_editor_property("should_retarget", True)
        evaluation = {}
        for frame in (0, 54, 96, 112, 138, 162, 168):
            try:
                pose = sequence.get_anim_pose_at_frame(frame, options)
                evaluation[str(frame)] = {
                    "valid": bool(pose.is_valid()),
                    "bones": _pose_sample(pose, sample_bones),
                    "connectivity": _connectivity(
                        pose, reference_pose, connectivity_pairs
                    ),
                }
            except Exception as error:
                evaluation[str(frame)] = {"error": repr(error)}
        evaluated[eval_name] = evaluation
    report["evaluated_poses"] = evaluated

    for accessor in ("get_data_model", "data_model"):
        try:
            model = getattr(sequence, accessor)
            model = model() if callable(model) else model
            report["data_model_accessor"] = accessor
            report["data_model_type"] = str(type(model))
            report["data_model_methods_bone"] = _public(model, "bone")
            report["data_model_methods_track"] = _public(model, "track")
            report["data_model_methods_transform"] = _public(model, "transform")
            for getter_name in ("get_bone_track_transforms", "get_bone_animation_track"):
                getter = getattr(model, getter_name, None)
                if not getter:
                    continue
                report[f"{getter_name}_doc"] = str(getattr(getter, "__doc__", ""))
                track_dump = {}
                for bone in ("root", "pelvis", "upperarm_r", "lowerarm_r", "hand_r"):
                    try:
                        values = getter(bone)
                        track_dump[bone] = {
                            "type": str(type(values)),
                            "count": len(values),
                            "samples": {
                                str(index): _transform(values[index])
                                for index in (0, 54, 96, 112, 138, 168)
                                if index < len(values)
                            },
                        }
                    except Exception as error:
                        track_dump[bone] = {"error": repr(error)}
                report[getter_name] = track_dump
            break
        except Exception as error:
            report[f"data_model_{accessor}_error"] = repr(error)

    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log(f"SESSION3_POSE_DIAGNOSIS={OUT_PATH}")


if __name__ == "__main__":
    main()
