"""One-shot guarded author for the append-only v008 CLEANED diagnostic crop."""
import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(r"C:\DGTour")
RAW_FILE = ROOT / "Content/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_RAW.uasset"
NORMALIZED_FILE = ROOT / "Content/DiscGolf/Animation/Authentic/v008/Diagnostic/Retargeted/AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_NORMALIZED.uasset"
EXPECTED_RAW_SHA256 = "9c4482eaf5fe3b68e32fd51319af473ca06d6ab695e5b45c273e5d1eae95a4f4"
EXPECTED_NORMALIZED_SHA256 = "8732ee6e476e0ce44b93630dff23864cdbfb09b020bffc40097678c52c219e1e"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else ""


before = {"raw": sha256(RAW_FILE), "normalized": sha256(NORMALIZED_FILE)}
if before["raw"] != EXPECTED_RAW_SHA256:
    raise RuntimeError("BLOCKED_REJECTED_RAW_PACKAGE_HASH")
if before["normalized"] != EXPECTED_NORMALIZED_SHA256:
    raise RuntimeError("BLOCKED_FROZEN_NORMALIZED_PACKAGE_HASH")

result = unreal.DiscGolfAuthenticV008RetargetUtility.author_img2396_cleaned_retarget()
unreal.log("DG_V008_CLEANED_RETARGET=" + result)
payload = json.loads(result)

after = {"raw": sha256(RAW_FILE), "normalized": sha256(NORMALIZED_FILE)}
if after != before:
    raise RuntimeError("BLOCKED_PREDECESSOR_PACKAGE_MUTATED_DURING_CLEANED_AUTHORING")
if (payload.get("schema") != "DiscGolfTour.AuthenticV008CleanedRetarget.v1"
        or payload.get("status") != "PASS_AUTHORED_EXACTLY_ONE_CLEANED_RETARGET"):
    raise RuntimeError(result)
