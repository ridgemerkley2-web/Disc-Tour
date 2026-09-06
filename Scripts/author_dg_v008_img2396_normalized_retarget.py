"""Invoke the one-shot guarded v008 normalized derivative author."""
import json
import hashlib
from pathlib import Path
import unreal

raw_file = Path(r"C:\DGTour\Content\DiscGolf\Animation\Authentic\v008\Diagnostic\Retargeted\AS_DG_RHBH_IMG2396_v008_DIAGNOSTIC_DGMaster_RAW.uasset")
expected_raw_sha256 = "9c4482eaf5fe3b68e32fd51319af473ca06d6ab695e5b45c273e5d1eae95a4f4"
if not raw_file.is_file() or hashlib.sha256(raw_file.read_bytes()).hexdigest() != expected_raw_sha256:
    raise RuntimeError("BLOCKED_REJECTED_RAW_PACKAGE_HASH")

result = unreal.DiscGolfAuthenticV008RetargetUtility.author_img2396_normalized_retarget()
unreal.log("DG_V008_NORMALIZED_RETARGET=" + result)
payload = json.loads(result)
if (payload.get("schema") != "DiscGolfTour.AuthenticV008NormalizedRetarget.v1"
        or payload.get("status") != "PASS_AUTHORED_EXACTLY_ONE_NORMALIZED_RETARGET"):
    raise RuntimeError(result)
