"""Read-only track/component probe for Session 5 legacy import-scale compatibility.

The probe intentionally does not author or save assets. It records exact local
and component transforms before the repair path is allowed to run, and proves
all accepted and fixture packages remain byte-identical.
"""

from pathlib import Path
import hashlib
import json

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
REPORT_PATH = (
    PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session5MocapUnitProbe.json"
)

PROTECTED_PACKAGES = (
    "Content/DiscGolf/Characters/Meshes/SK_DG_Master.uasset",
    "Content/DiscGolf/Characters/Meshes/SKEL_DG_Master.uasset",
    "Content/DiscGolf/Rigs/IK_DG_Master.uasset",
    "Content/DiscGolf/Rigs/CR_DG_Master.uasset",
    "Content/DiscGolf/Animation/ABP_DG_Player.uasset",
    "Content/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype.uasset",
    "Content/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.uasset",
    "Content/DiscGolf/UI/WBP_DG_CharacterCreator.uasset",
    "Content/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.uasset",
    "Content/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.uasset",
    "Content/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.uasset",
)

FIXTURE_PACKAGES = (
    "Content/DiscGolf/Animation/Mocap/Source/SK_DG_RHBH_SyntheticSource.uasset",
    "Content/DiscGolf/Animation/Mocap/Source/SKEL_DG_RHBH_SyntheticSource.uasset",
    "Content/DiscGolf/Animation/Mocap/Source/A_DG_RHBH_SyntheticSource_RAW.uasset",
    "Content/DiscGolf/Animation/Mocap/Rigs/IK_DG_RHBH_SyntheticSource.uasset",
    "Content/DiscGolf/Animation/Mocap/Rigs/RTG_DG_RHBH_Synthetic_To_Master.uasset",
    "Content/DiscGolf/Animation/Mocap/Retargeted/A_DG_RHBH_Synthetic_RTG.uasset",
    "Content/DiscGolf/Animation/Mocap/Cleaned/A_DG_RHBH_Synthetic_CLN.uasset",
    "Content/DiscGolf/Animation/Mocap/Production/A_DG_RHBH_SyntheticPipelineTest_v001.uasset",
    "Content/DiscGolf/Animation/Mocap/Production/AM_DG_RHBH_SyntheticPipelineTest_v001.uasset",
    "Content/DiscGolf/Animation/Mocap/Production/DA_DG_AnimationLibrary.uasset",
)


def _hash_required(packages):
    result = {}
    for relative in packages:
        path = PROJECT_ROOT / relative
        if not path.is_file():
            raise RuntimeError(f"Required probe package is missing: {path}")
        raw = path.read_bytes()
        result[relative] = hashlib.sha256(raw).hexdigest()
    return result


def main():
    packages = PROTECTED_PACKAGES + FIXTURE_PACKAGES
    before = _hash_required(packages)
    report = json.loads(
        unreal.DiscGolfSession5MocapUtility.probe_session5_unit_compatibility()
    )
    if not str(report.get("status", "")).startswith("PASS"):
        raise RuntimeError(report.get("error", "Session 5 unit probe failed"))
    after = _hash_required(packages)
    changed = [package for package in packages if before[package] != after[package]]
    if changed:
        raise RuntimeError(f"Read-only Session 5 unit probe changed packages: {changed}")
    report["package_integrity"] = {
        "status": "PASS_NO_DISK_MUTATION",
        "protected_package_count": len(PROTECTED_PACKAGES),
        "fixture_package_count": len(FIXTURE_PACKAGES),
        "changed_packages": changed,
        "before": before,
        "after": after,
    }
    report["probe_writes"] = [str(REPORT_PATH)]
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log(
        "DG_SESSION5_MOCAP_UNIT_PROBE: PASS_READ_ONLY "
        "classification=SYNTHETIC_TEST shipping=DO_NOT_SHIP disk_mutation=NONE"
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
