"""Generate the current approval-gated environment slot report from Unreal Editor.

Usage:
  UnrealEditor-Cmd.exe DiscGolfTour.uproject -run=pythonscript \
    -script=Scripts/validate-environment-asset-bindings.py -unattended -NullRHI
"""

import unreal


ASSET_SET = "/Game/Environment/Forest/DA_TemperateMountainForest_Assets"
asset_set = unreal.EditorAssetLibrary.load_asset(ASSET_SET)
if not asset_set:
    raise RuntimeError(f"Environment asset set is missing: {ASSET_SET}")

result = unreal.DiscGolfEnvironmentAssetBinder.validate_environment_asset_readiness(asset_set)
if not result.report_generated:
    raise RuntimeError("Environment binding report generation failed")
if not result.structurally_complete:
    raise RuntimeError("Environment binding data asset is missing category slots")

unreal.log(
    "ENVIRONMENT_BINDING_REPORT_OK: "
    f"{result.report_path} production_ready={str(result.production_ready).lower()}"
)
