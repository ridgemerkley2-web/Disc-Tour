"""Author the owned Session 4 CR/ABP/creator-widget asset wiring in UE 5.8.

The native editor utility is intentionally idempotent. It preserves the master
skeleton, accepted PBIK settings, RHBH montage/notifies, and gameplay authority.
"""

from pathlib import Path
import importlib.util
import json

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
REPORT_PATH = PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session4AssetCreation.json"


def _strict_accepted_rig_preflight():
    validator_path = PROJECT_ROOT / "Scripts" / "validate_dg_character_session2.py"
    spec = importlib.util.spec_from_file_location("dg_session2_rig_preflight", validator_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load accepted rig validator: {validator_path}")
    validator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(validator)
    mesh = validator._load(validator.MESH_PATH, unreal.SkeletalMesh)
    rig = validator._load(validator.CONTROL_RIG_PATH, unreal.ControlRigBlueprint)
    validator._validate_control_rig(rig, mesh)


def main():
    # Refuse all writes unless the full accepted Session 2 PBIK/effectors/
    # preferred-angle contract passes first. The Session 4 native utility then
    # adds only its owned unit/ABP/widget layer.
    _strict_accepted_rig_preflight()
    report = json.loads(
        unreal.DiscGolfSession4AssetUtility.author_session4_assets()
    )
    report["strict_session2_pbik_preflight"] = "PASS"
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    if not str(report.get("status", "")).startswith("PASS"):
        raise RuntimeError(report.get("error", "Session 4 asset authoring failed"))
    unreal.log(
        "DG_SESSION4_ASSETS: "
        f"{report.get('authoring_status')} "
        "CR=Begin_Profile_PBIK "
        "ABP=RefPose_DefaultSlot_ControlRig_Root "
        "WBP=WBP_DG_CharacterCreator"
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
        REPORT_PATH.write_text(
            json.dumps(
                {"status": "FAIL", "error": f"{type(error).__name__}: {error}"},
                indent=2,
            ),
            encoding="utf-8",
        )
        raise
