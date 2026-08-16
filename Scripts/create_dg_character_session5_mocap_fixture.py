"""Create the deterministic Session 5 synthetic mocap pipeline fixture.

The native editor utility uses a project-owned duplicate source skeleton and a
real UE 5.8 IK Retargeter batch pass. This wrapper proves accepted Session 1-4
packages are byte-identical before and after authoring. The fixture is tagged
SYNTHETIC_TEST / DO_NOT_SHIP and never replaces the accepted RHBH prototype.
"""

from pathlib import Path
import hashlib
import json

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
REPORT_PATH = (
    PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session5MocapFixtureCreation.json"
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

PIPELINE_SEQUENCE_PACKAGES = (
    "Content/DiscGolf/Animation/Mocap/Source/A_DG_RHBH_SyntheticSource_RAW.uasset",
    "Content/DiscGolf/Animation/Mocap/Retargeted/A_DG_RHBH_Synthetic_RTG.uasset",
    "Content/DiscGolf/Animation/Mocap/Cleaned/A_DG_RHBH_Synthetic_CLN.uasset",
    "Content/DiscGolf/Animation/Mocap/Production/A_DG_RHBH_SyntheticPipelineTest_v001.uasset",
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

IMMUTABLE_FIXTURE_PACKAGES = FIXTURE_PACKAGES[:5]
LEGACY_COMPATIBILITY_REPAIR_PACKAGES = PIPELINE_SEQUENCE_PACKAGES[1:]


def _hash_required(packages):
    result = {}
    for relative in packages:
        path = PROJECT_ROOT / relative
        if not path.is_file():
            raise RuntimeError(f"Required accepted package is missing: {path}")
        raw = path.read_bytes()
        result[relative] = {
            "sha256": hashlib.sha256(raw).hexdigest(),
            "size_bytes": len(raw),
        }
    return result


def _hash_existing(packages):
    result = {}
    for relative in packages:
        path = PROJECT_ROOT / relative
        if path.is_file():
            raw = path.read_bytes()
            result[relative] = {
                "sha256": hashlib.sha256(raw).hexdigest(),
                "size_bytes": len(raw),
            }
    return result


def main():
    before = _hash_required(PROTECTED_PACKAGES)
    fixture_before = _hash_existing(FIXTURE_PACKAGES)
    immutable_fixture_before = _hash_existing(IMMUTABLE_FIXTURE_PACKAGES)
    complete_fixture_existed = len(fixture_before) == len(FIXTURE_PACKAGES)
    report = json.loads(
        unreal.DiscGolfSession5MocapUtility.author_session5_fixture()
    )
    if not str(report.get("status", "")).startswith("PASS"):
        raise RuntimeError(report.get("error", "Session 5 authoring failed"))

    after = _hash_required(PROTECTED_PACKAGES)
    stage_hashes = _hash_required(PIPELINE_SEQUENCE_PACKAGES)
    changed = [
        package
        for package in PROTECTED_PACKAGES
        if before[package]["sha256"] != after[package]["sha256"]
    ]
    if changed:
        raise RuntimeError(f"Session 5 authoring changed accepted packages: {changed}")
    immutable_fixture_after = _hash_required(IMMUTABLE_FIXTURE_PACKAGES)
    changed_immutable_fixture = [
        package
        for package, identity in immutable_fixture_before.items()
        if immutable_fixture_after[package]["sha256"] != identity["sha256"]
    ]
    if changed_immutable_fixture:
        raise RuntimeError(
            "Session 5 compatibility authoring changed source/RAW/rig packages: "
            f"{changed_immutable_fixture}"
        )
    fixture_after = _hash_required(FIXTURE_PACKAGES)
    changed_existing_fixture = [
        package
        for package, identity in fixture_before.items()
        if fixture_after[package]["sha256"] != identity["sha256"]
    ]
    distinct_hashes = {
        identity["sha256"] for identity in stage_hashes.values()
    }
    if len(distinct_hashes) != len(PIPELINE_SEQUENCE_PACKAGES):
        raise RuntimeError("RAW/RTG/CLN/final packages are not hash-distinct")
    if report.get("authoring_status") == "PASS_ALREADY_CURRENT_NO_ASSET_WRITES":
        if report.get("asset_writes"):
            raise RuntimeError("Idempotent Session 5 rerun reported unexpected asset writes")
        if changed_existing_fixture:
            raise RuntimeError(
                "Idempotent Session 5 rerun changed fixture packages: "
                f"{changed_existing_fixture}"
            )
    elif complete_fixture_existed:
        if set(changed_existing_fixture) != set(LEGACY_COMPATIBILITY_REPAIR_PACKAGES):
            raise RuntimeError(
                "Legacy compatibility migration must change exactly RTG/CLN/final: "
                f"{changed_existing_fixture}"
            )

    report["accepted_package_integrity"] = {
        "status": "PASS_UNCHANGED",
        "package_count": len(PROTECTED_PACKAGES),
        "changed_packages": changed,
        "before": before,
        "after": after,
    }
    report["pipeline_hash_separation"] = {
        "status": "PASS_DISTINCT_NON_DESTRUCTIVE_STAGES",
        "changed_preexisting_fixture_packages": changed_existing_fixture,
        "changed_immutable_source_raw_or_rig_packages": changed_immutable_fixture,
        "before_existing_fixture": fixture_before,
        "after": stage_hashes,
        "distinct_sha256_count": len(distinct_hashes),
        "retarget_compatibility_normalization": (
            "LEGACY_NON_UNIT_ROOT_SCALE_COMPATIBILITY_NORMALIZATION_V1"
        ),
        "cleanup_recipe": (
            "ROOT_XY_LOCK_TO_FRAME0;ROOT_Z_DELTA_CLAMP_3CM;"
            "REMOVE_SEQUENCE_NOTIFIES;REMOVE_TRANSFORM_CURVES;REAUTHOR_6_DG_CURVES"
        ),
    }
    report["wrapper_writes"] = [str(REPORT_PATH)]
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log(
        "DG_SESSION5_MOCAP_CREATE: "
        f"{report.get('authoring_status')} classification=SYNTHETIC_TEST "
        "shipping=DO_NOT_SHIP prototype_mutation=NONE"
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
