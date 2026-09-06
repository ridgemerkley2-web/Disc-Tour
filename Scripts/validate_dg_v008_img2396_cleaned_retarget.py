"""Cold read-only validation of the append-only v008 CLEANED diagnostic crop."""
import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(r"C:\DGTour")
RAW_FILE = ROOT / "Content/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_RAW.uasset"
NORMALIZED_FILE = ROOT / "Content/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_NORMALIZED.uasset"
CLEANED_FILE = ROOT / "Content/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_CLEANED.uasset"
EXPECTED_RAW_SHA256 = "9c4482eaf5fe3b68e32fd51319af473ca06d6ab695e5b45c273e5d1eae95a4f4"
EXPECTED_NORMALIZED_SHA256 = "8732ee6e476e0ce44b93630dff23864cdbfb09b020bffc40097678c52c219e1e"
EXPECTED_CLEANED_SHA256 = "9eae65b7632e0a25967414409c59d745f103b8a195b18469dd1ce1df2c191ce9"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else ""


before = {
    "raw": sha256(RAW_FILE),
    "normalized": sha256(NORMALIZED_FILE),
    "cleaned": sha256(CLEANED_FILE),
}
if before["raw"] != EXPECTED_RAW_SHA256:
    raise RuntimeError("BLOCKED_REJECTED_RAW_PACKAGE_HASH")
if before["normalized"] != EXPECTED_NORMALIZED_SHA256:
    raise RuntimeError("BLOCKED_FROZEN_NORMALIZED_PACKAGE_HASH")
if before["cleaned"] != EXPECTED_CLEANED_SHA256:
    raise RuntimeError("BLOCKED_FROZEN_CLEANED_PACKAGE_HASH")

result = unreal.DiscGolfAuthenticV008RetargetUtility.validate_img2396_cleaned_retarget()
unreal.log("DG_V008_CLEANED_RETARGET_VALIDATION=" + result)
payload = json.loads(result)
after = {
    "raw": sha256(RAW_FILE),
    "normalized": sha256(NORMALIZED_FILE),
    "cleaned": sha256(CLEANED_FILE),
}
if after != before:
    raise RuntimeError("BLOCKED_PACKAGE_MUTATED_DURING_COLD_CLEANED_VALIDATION")
if (payload.get("schema") != "DiscGolfTour.AuthenticV008CleanedRetarget.v1"
        or payload.get("status") != "PASS_CLEANED_RETARGET_VALID"):
    raise RuntimeError(result)
