"""Author the owned Session 3 RHBH animation assets in Unreal Engine 5.8.

The editor-only native utility owns the deterministic bone keys, motion curves,
montage events, and ABP slot graph. It has no access to the runtime throw or
flight authority. A rerun is validation-only once the exact contract exists.
"""

from pathlib import Path
import json

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
REPORT_PATH = PROJECT_ROOT / "Saved" / "CharacterFramework" / "Session3AssetCreation.json"


def main():
    raw_report = unreal.DiscGolfSession3AssetUtility.author_session3_assets()
    report = json.loads(raw_report)
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    if not str(report.get("status", "")).startswith("PASS"):
        raise RuntimeError(report.get("error", "Session 3 asset authoring failed"))
    unreal.log(
        "DG_SESSION3_ASSETS: "
        f"{report.get('authoring_status')} "
        "sequence=A_DG_RHBH_Prototype montage=AM_DG_RHBH_Prototype "
        "release=frame96 finish=frame162 ABP=RefPose_DefaultSlot_Root"
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
        if not REPORT_PATH.exists():
            REPORT_PATH.write_text(
                json.dumps(
                    {
                        "status": "FAIL",
                        "error": f"{type(error).__name__}: {error}",
                    },
                    indent=2,
                ),
                encoding="utf-8",
            )
        raise
