"""Cold-process validation for the append-only v008 DGMaster RAW sequence."""
import unreal

result = unreal.DiscGolfAuthenticV008RetargetUtility.validate_img2396_raw_retarget()
unreal.log("DG_V008_RAW_RETARGET_VALIDATION=" + result)
if '"status":"PASS_RAW_RETARGET_VALID"' not in result:
    raise RuntimeError(result)
