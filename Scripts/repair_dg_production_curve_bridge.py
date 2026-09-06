"""Repair the production-motion curve bridge into CR_DG_Master.

The dense v006 sequences carry nineteen animation curves.  CR_DG_Master was
originally authored with the six Session 2 curves only, so its native profile
unit could not observe ``DG_GazeTargetAlpha`` and incorrectly selected the
legacy synthetic follow-through hand target.  This narrowly scoped editor
script registers the production curve metadata on SKEL_DG_Master, imports the
same curves into the existing Control Rig hierarchy, recompiles that rig, and
verifies the exact contract.  It does not alter animation sequences, montages,
release notifies, gameplay physics, or save data.

Run with UnrealEditor-Cmd ``-run=pythonscript`` while the project is closed.
"""

from __future__ import annotations

import json
from pathlib import Path

import unreal


SKELETON_PATH = "/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master"
CONTROL_RIG_PATH = "/Game/DiscGolf/Rigs/CR_DG_Master"
PRODUCTION_CURVES = (
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


def _curve_names(rig: unreal.ControlRigBlueprint) -> set[str]:
    return {
        str(key.name)
        for key in rig.get_hierarchy().get_all_keys()
        if key.type == unreal.RigElementType.CURVE
    }


def main() -> None:
    skeleton = unreal.load_asset(SKELETON_PATH)
    rig = unreal.load_asset(CONTROL_RIG_PATH)
    if not isinstance(skeleton, unreal.Skeleton):
        raise RuntimeError(f"Missing expected Skeleton: {SKELETON_PATH}")
    if not isinstance(rig, unreal.ControlRigBlueprint):
        raise RuntimeError(f"Missing expected ControlRigBlueprint: {CONTROL_RIG_PATH}")

    skeleton_changed = False
    skeleton_curves = {str(name) for name in skeleton.get_curve_meta_data_names()}
    for curve_name in PRODUCTION_CURVES:
        if curve_name not in skeleton_curves:
            if not skeleton.add_curve_meta_data(curve_name):
                raise RuntimeError(f"Could not register skeleton curve {curve_name}")
            skeleton_changed = True

    before = _curve_names(rig)
    missing_before = sorted(set(PRODUCTION_CURVES) - before)
    if missing_before:
        imported = rig.get_hierarchy_controller().import_curves_from_asset(
            skeleton.get_path_name(), "", False
        )
        if not imported:
            raise RuntimeError(
                "Control Rig curve import returned no elements while production curves were missing"
            )
        rig.request_auto_vm_recompilation()
        rig.recompile_vm()

    if skeleton_changed and not unreal.EditorAssetLibrary.save_loaded_asset(skeleton, False):
        raise RuntimeError(f"Could not save {SKELETON_PATH}")
    if missing_before and not unreal.EditorAssetLibrary.save_loaded_asset(rig, False):
        raise RuntimeError(f"Could not save {CONTROL_RIG_PATH}")

    after = _curve_names(rig)
    missing_after = sorted(set(PRODUCTION_CURVES) - after)
    if missing_after:
        raise RuntimeError(f"Control Rig production curve bridge incomplete: {missing_after}")

    report = {
        "schema": "DiscGolfTour.ProductionCurveBridgeRepair.v1",
        "status": "PASS",
        "skeleton": SKELETON_PATH,
        "control_rig": CONTROL_RIG_PATH,
        "required_curve_count": len(PRODUCTION_CURVES),
        "required_curves": list(PRODUCTION_CURVES),
        "missing_before": missing_before,
        "missing_after": missing_after,
        "skeleton_changed": skeleton_changed,
        "control_rig_changed": bool(missing_before),
        "physics_or_release_authority_changed": False,
    }
    report_path = (
        Path(unreal.Paths.project_saved_dir())
        / "ProductionMotion"
        / "ProductionCurveBridgeRepair.json"
    )
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    unreal.log(f"DG_PRODUCTION_CURVE_BRIDGE: PASS {json.dumps(report, sort_keys=True)}")


main()
