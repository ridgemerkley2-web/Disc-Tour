"""Invoke the guarded C++ v008 RAW retarget utility and verify its exact result."""
import json

import unreal

result = unreal.DiscGolfAuthenticV008RetargetUtility.author_img2396_raw_retarget()
unreal.log("DG_V008_RAW_RETARGET=" + result)
try:
    payload = json.loads(result)
except (TypeError, json.JSONDecodeError) as exc:
    raise RuntimeError(f"v008 retarget utility returned invalid JSON: {exc}: {result}") from exc
if payload.get("schema") != "DiscGolfTour.AuthenticV008RawRetarget.v1" or payload.get("status") != "PASS_AUTHORED_EXACTLY_ONE_RAW_RETARGET":
    raise RuntimeError(result)
