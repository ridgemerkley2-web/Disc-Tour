"""Strict read-only Session 4 CR/ABP/creator-widget validation.

Only the JSON evidence report under Saved is written. Package hashes prove that
reflected validation itself does not persist an asset mutation.
"""

from pathlib import Path
import hashlib
import json

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
REPORT_PATH = PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session4AssetValidation.json"

PROTECTED_PACKAGES = (
    "Content/DiscGolf/Characters/Meshes/SK_DG_Master.uasset",
    "Content/DiscGolf/Characters/Meshes/SKEL_DG_Master.uasset",
    "Content/DiscGolf/Rigs/IK_DG_Master.uasset",
    "Content/DiscGolf/Rigs/CR_DG_Master.uasset",
    "Content/DiscGolf/Animation/ABP_DG_Player.uasset",
    "Content/DiscGolf/Animation/Throws/A_DG_RHBH_Prototype.uasset",
    "Content/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.uasset",
    "Content/DiscGolf/UI/WBP_DG_CharacterCreator.uasset",
)


def _hash_packages():
    result = {}
    for relative in PROTECTED_PACKAGES:
        path = PROJECT_ROOT / relative
        if not path.is_file():
            raise RuntimeError(f"Required Session 4 package is missing: {path}")
        raw = path.read_bytes()
        result[relative] = {
            "sha256": hashlib.sha256(raw).hexdigest(),
            "size_bytes": len(raw),
        }
    return result


def main():
    before = _hash_packages()
    report = json.loads(
        unreal.DiscGolfSession4AssetUtility.validate_session4_assets()
    )
    if not str(report.get("status", "")).startswith("PASS"):
        raise RuntimeError(report.get("error", "Session 4 strict asset validation failed"))
    after = _hash_packages()
    changed = [
        package
        for package in PROTECTED_PACKAGES
        if before[package]["sha256"] != after[package]["sha256"]
    ]
    if changed:
        raise RuntimeError(
            f"Read-only Session 4 validation changed packages on disk: {changed}"
        )
    report["package_integrity"] = {
        "status": "PASS_NO_DISK_MUTATION",
        "package_count": len(PROTECTED_PACKAGES),
        "changed_packages": changed,
        "before": before,
        "after": after,
    }
    report["validator_writes"] = [str(REPORT_PATH)]
    report["uasset_write_calls"] = []
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log(
        "DG_SESSION4_ASSET_VALIDATION: PASS "
        "CR=Begin_Profile_PBIK ABP=RefPose_DefaultSlot_ControlRig_Root "
        "WBP=ProjectOwnedParent disk_mutation=NONE"
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
