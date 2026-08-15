"""Generate the canonical cooked disc equipment Primary Data Assets.

Run through Unreal Editor, not regular Python:
UnrealEditor-Cmd.exe DiscGolfTour.uproject -run=pythonscript -script=Scripts/generate_disc_data_assets.py
"""

import unreal


MOLDS = (
    ("Apex", "Apex", 12, 5, -1.0, 3.0),
    ("Vector", "Vector", 9, 5, -2.0, 2.0),
    ("Line", "Line", 7, 5, -1.0, 2.0),
    ("Compass", "Compass", 5, 5, 0.0, 1.0),
    ("Touch", "Touch", 2, 3, 0.0, 1.0),
)

PLASTICS = (
    ("Base", unreal.DiscPlastic.BASE, 1.18, 0.88, 0.72, 1.18),
    ("Tour", unreal.DiscPlastic.TOUR, 1.0, 1.0, 1.0, 1.0),
    ("Crystal", unreal.DiscPlastic.CRYSTAL, 0.84, 1.12, 1.20, 0.86),
)


def set_property(target, name, value):
    target.set_editor_property(name, value)


def make_aero(speed, glide, turn, fade):
    speed_norm = max(0.0, min(1.0, (speed - 2.0) / 10.0))
    aero = unreal.DiscAeroProfile()
    values = {
        "MassKg": 0.175,
        "DiameterM": 0.211,
        "AreaM2": 0.03496,
        "InertiaAxialKgM2": 0.000974,
        "InertiaPlanarKgM2": 0.000487,
        "CL0": 0.22 + 0.018 * glide,
        "CLa": 1.72 + 0.05 * glide,
        "CD0": 0.135 + (0.080 - 0.135) * speed_norm,
        "CDa": 0.62,
        "CM0": 0.0,
        "CMa": 0.0,
        "HighSpeedTurnMomentNm": max(0.0, -turn) * 0.0035,
        "LowSpeedFadeMomentNm": max(0.0, fade) * 0.0025,
        "TurnStartsAboveMps": 13.5 + (20.0 - 13.5) * speed_norm,
        "FadeStartsBelowMps": 12.0 + (17.0 - 12.0) * speed_norm,
        "SpinDecayPerSecond": 0.032,
        "GroundRestitution": 0.16,
        "GroundFriction": 0.46,
    }
    for name, value in values.items():
        set_property(aero, name, value)
    return aero


def get_or_create_asset(asset_name, package_path, asset_class):
    object_path = f"{package_path}/{asset_name}.{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(object_path):
        existing = unreal.EditorAssetLibrary.load_asset(object_path)
        if not isinstance(existing, asset_class):
            raise RuntimeError(f"{object_path} exists with the wrong class")
        return existing
    factory = unreal.DataAssetFactory()
    set_property(factory, "DataAssetClass", asset_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, package_path, asset_class, factory
    )
    if not asset:
        raise RuntimeError(f"Failed to create {object_path}")
    return asset


def generate_molds():
    package_path = "/Game/Data/Discs/Molds"
    unreal.EditorAssetLibrary.make_directory(package_path)
    for mold_id, display_name, speed, glide, turn, fade in MOLDS:
        asset = get_or_create_asset(
            f"DA_Mold_{mold_id}", package_path, unreal.DiscMoldDataAsset
        )
        definition = unreal.DiscMoldDefinition()
        for name, value in {
            "MoldId": mold_id,
            "DisplayName": display_name,
            "Speed": speed,
            "Glide": glide,
            "Turn": turn,
            "Fade": fade,
            "Aero": make_aero(speed, glide, turn, fade),
        }.items():
            set_property(definition, name, value)
        set_property(asset, "Mold", definition)
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Failed to save mold asset {mold_id}")


def generate_plastics():
    package_path = "/Game/Data/Discs/Plastics"
    unreal.EditorAssetLibrary.make_directory(package_path)
    for plastic_id, plastic, turn, fade, restitution, friction in PLASTICS:
        asset = get_or_create_asset(
            f"DA_Plastic_{plastic_id}", package_path, unreal.DiscPlasticDataAsset
        )
        definition = unreal.DiscPlasticDefinition()
        for name, value in {
            "PlasticId": plastic_id,
            "Plastic": plastic,
            "HighSpeedTurnMomentScale": turn,
            "LowSpeedFadeMomentScale": fade,
            "GroundRestitutionScale": restitution,
            "GroundFrictionScale": friction,
        }.items():
            set_property(definition, name, value)
        set_property(asset, "PlasticDefinition", definition)
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Failed to save plastic asset {plastic_id}")


generate_molds()
generate_plastics()
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
    ["/Game/Data/Discs"], force_rescan=True
)
unreal.log("Generated 5 DiscMold and 3 DiscPlastic Primary Data Assets")
