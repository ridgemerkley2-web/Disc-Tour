"""Strict live rig validation for the accepted Session 3 boundary.

Session 2's validator intentionally proves that no Session 3 wiring exists, so
it must continue to fail after integration. This successor reuses every live
master/IK/Control-Rig/profile/AnimBP structural check and replaces only that
historical no-wiring assertion with the Session 3 single-authority reports.
"""

from pathlib import Path
import json
import sys

import unreal


SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
REPORT_PATH = PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session3RigValidation.json"
sys.path.insert(0, str(SCRIPT_DIR))

import validate_dg_character_session2 as session2  # noqa: E402
import validate_dg_character_session3_wiring as wiring  # noqa: E402


def _load_asset_contract():
    raw = unreal.DiscGolfSession3AssetUtility.validate_session3_assets()
    report = json.loads(raw)
    if not str(report.get("status", "")).startswith("PASS"):
        raise RuntimeError(
            report.get("error", "Session 3 reflected asset contract failed")
        )
    return report


def main():
    mesh = session2._load(session2.MESH_PATH, unreal.SkeletalMesh)
    skeleton = session2._load(session2.SKELETON_PATH, unreal.Skeleton)
    ik_rig = session2._load(session2.IK_PATH, unreal.IKRigDefinition)
    control_rig = session2._load(
        session2.CONTROL_RIG_PATH, unreal.ControlRigBlueprint
    )
    anim_bp = session2._load(session2.ANIM_BP_PATH, unreal.AnimBlueprint)

    wiring.main()
    wiring_report = json.loads(
        wiring.REPORT_PATH.read_text(encoding="utf-8")
    )
    if wiring_report.get("status") != "PASS_SESSION3_SINGLE_AUTHORITY_WIRING":
        raise RuntimeError("Session 3 source wiring report did not pass")

    report = {
        "schema": "DiscGolfTour.Session3RigValidation.v1",
        "status": "PASS_STRICT_SESSION3_RIG_AND_AUTHORITY",
        "engine_version": unreal.SystemLibrary.get_engine_version(),
        "source_contract": session2._validate_source_contract(),
        "assets": {
            "mesh": session2.MESH_PATH,
            "skeleton": session2.SKELETON_PATH,
            "ik_rig": session2.IK_PATH,
            "control_rig": session2.CONTROL_RIG_PATH,
            "animation_blueprint": session2.ANIM_BP_PATH,
        },
        "master": session2._validate_master(mesh, skeleton),
        "ik_rig": session2._validate_ik(ik_rig, mesh),
        "control_rig": session2._validate_control_rig(control_rig, mesh),
        "body_profiles": session2._validate_profiles(),
        "animation_blueprint": session2._validate_anim_bp(anim_bp, skeleton),
        "session3_animation_contract": _load_asset_contract(),
        "authority_boundary": wiring_report,
        "accepted_scope": [
            "one right-handed backhand drive montage",
            "provisional Cylinder held at disc_grip_r",
            "one-way grip-position handoff into existing launch authority",
        ],
        "deferred_to_session_4": [
            "runtime body-profile deformation",
            "profile-to-rig runtime plumbing",
            "throw-style tuning and creator integration",
        ],
        "production_art_limitations": [
            "validation proxy is not a production character",
            "Cylinder and block hand do not establish final rim ergonomics",
        ],
        "asset_write_calls": [],
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log(
        "DG_SESSION3_RIG_VALIDATION: PASS bones=69 goals=4 effectors=4 "
        "profiles=3 release=1 finish=1 authority=SINGLE_EXISTING_FLIGHT_PATH"
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
        REPORT_PATH.write_text(
            json.dumps(
                {
                    "schema": "DiscGolfTour.Session3RigValidation.v1",
                    "status": "FAIL",
                    "engine_version": unreal.SystemLibrary.get_engine_version(),
                    "error": f"{type(error).__name__}: {error}",
                },
                indent=2,
            ),
            encoding="utf-8",
        )
        raise
