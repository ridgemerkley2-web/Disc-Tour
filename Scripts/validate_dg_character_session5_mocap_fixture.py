"""Strict read-only validation for the Session 5 synthetic mocap fixture.

Only this JSON evidence report is written. Hashes cover both the accepted
Session 1-4 authority packages and every Session 5 fixture package, proving the
native validator performed no persisted asset mutation.
"""

from pathlib import Path
import hashlib
import json

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
REPORT_PATH = (
    PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session5MocapFixtureValidation.json"
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
            raise RuntimeError(f"Required Session 5 package is missing: {path}")
        raw = path.read_bytes()
        result[relative] = {
            "sha256": hashlib.sha256(raw).hexdigest(),
            "size_bytes": len(raw),
        }
    return result


def main():
    packages = PROTECTED_PACKAGES + FIXTURE_PACKAGES
    before = _hash_required(packages)
    report = json.loads(
        unreal.DiscGolfSession5MocapUtility.validate_session5_fixture()
    )
    if not str(report.get("status", "")).startswith("PASS"):
        raise RuntimeError(report.get("error", "Session 5 strict validation failed"))
    after = _hash_required(packages)
    changed = [
        package
        for package in packages
        if before[package]["sha256"] != after[package]["sha256"]
    ]
    if changed:
        raise RuntimeError(f"Read-only Session 5 validation changed packages: {changed}")
    sequence_packages = (
        "Content/DiscGolf/Animation/Mocap/Source/A_DG_RHBH_SyntheticSource_RAW.uasset",
        "Content/DiscGolf/Animation/Mocap/Retargeted/A_DG_RHBH_Synthetic_RTG.uasset",
        "Content/DiscGolf/Animation/Mocap/Cleaned/A_DG_RHBH_Synthetic_CLN.uasset",
        "Content/DiscGolf/Animation/Mocap/Production/A_DG_RHBH_SyntheticPipelineTest_v001.uasset",
    )
    sequence_hashes = {package: after[package] for package in sequence_packages}
    distinct_hashes = {value["sha256"] for value in sequence_hashes.values()}
    if len(distinct_hashes) != len(sequence_packages):
        raise RuntimeError("RAW/RTG/CLN/final packages are not hash-distinct")

    report["package_integrity"] = {
        "status": "PASS_NO_DISK_MUTATION",
        "package_count": len(packages),
        "changed_packages": changed,
        "before": before,
        "after": after,
    }
    report["pipeline_hash_separation"] = {
        "status": "PASS_DISTINCT_AND_UNCHANGED",
        "packages": sequence_hashes,
        "distinct_sha256_count": len(distinct_hashes),
    }
    report["validator_writes"] = [str(REPORT_PATH)]
    report["uasset_write_calls"] = []
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log(
        "DG_SESSION5_MOCAP_VALIDATE: PASS source=SYNTHETIC_TEST "
        "shipping=DO_NOT_SHIP retarget=UE5_8_IK_BATCH disk_mutation=NONE"
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
