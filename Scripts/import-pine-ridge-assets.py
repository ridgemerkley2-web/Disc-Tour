"""Import the licensed Pine Ridge source set and build Unreal material assets.

Run with UnrealEditor-Cmd after download-pine-ridge-cc0-assets.ps1. The script is
idempotent and only writes below /Game/Presentation/Course/PineRidge.
"""

from pathlib import Path
import base64
import json
import unreal


ROOT = Path(unreal.Paths.project_dir())
SOURCE_ROOT = ROOT / "SourceArt" / "PineRidge" / "PolyHaven"
CONTENT_ROOT = "/Game/Presentation/Course/PineRidge"
TEXTURE_ROOT = f"{CONTENT_ROOT}/Textures"
FOLIAGE_ROOT = f"{CONTENT_ROOT}/Foliage/PolyHaven/FirSapling"
FIXTURE_ROOT = f"{CONTENT_ROOT}/Fixtures/PolyHaven"
MATERIAL_ROOT = f"{CONTENT_ROOT}/Materials"
RECEIPT_PATH = ROOT / "Saved" / "PineRidgeAssetImportReceipt.json"


def log(message):
    unreal.log(f"PINE_RIDGE_IMPORT: {message}")


def import_asset(filename, destination_path, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(filename))
    task.set_editor_property("destination_path", destination_path)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    if options is not None:
        task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return list(task.get_editor_property("imported_object_paths"))


def load_asset(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
        raise RuntimeError(f"Required asset did not load: {asset_path}")
    return asset


def import_texture(source_file, destination_path, compression):
    imported = import_asset(source_file, destination_path)
    if not imported:
        expected = f"{destination_path}/{source_file.stem}"
        if not unreal.EditorAssetLibrary.does_asset_exist(expected):
            raise RuntimeError(f"Texture import produced no asset: {source_file}")
        imported = [expected]

    texture = load_asset(imported[0])
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Imported source is not Texture2D: {source_file}")
    texture.set_editor_property("compression_settings", compression)
    texture.set_editor_property("srgb", compression == unreal.TextureCompressionSettings.TC_DEFAULT)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return imported[0]


def ensure_ground_cover_textures():
    """Create tiny authored alpha masks used by the generated grass/litter cards."""
    source_dir = ROOT / "SourceArt" / "PineRidge" / "Generated"
    source_dir.mkdir(parents=True, exist_ok=True)
    filenames = (
        "pine_ridge_grass_card_alpha.png",
        "pine_ridge_litter_card_alpha.png",
    )
    for filename in filenames:
        path = source_dir / filename
        encoded = path.with_suffix(path.suffix + ".base64")
        if not encoded.exists():
            raise RuntimeError(f"Missing generated ground-cover source: {encoded}")
        path.write_bytes(base64.b64decode(encoded.read_text(encoding="ascii").strip()))
    destination = f"{TEXTURE_ROOT}/Generated"
    assets = {}
    for filename in filenames:
        imported = import_asset(source_dir / filename, destination)
        expected = f"{destination}/{Path(filename).stem}"
        texture = load_asset(imported[0] if imported else expected)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
        assets[filename] = texture
    return assets


def load_pbr_texture_set(folder, prefix, destination, include_opacity=False):
    textures = {
        "diffuse": load_asset(import_texture(
            folder / f"{prefix}_diffuse_1k.jpg", destination,
            unreal.TextureCompressionSettings.TC_DEFAULT
        )),
        "normal": load_asset(import_texture(
            folder / f"{prefix}_nor_dx_1k.png", destination,
            unreal.TextureCompressionSettings.TC_NORMALMAP
        )),
        "rough": load_asset(import_texture(
            folder / f"{prefix}_rough_1k.jpg", destination,
            unreal.TextureCompressionSettings.TC_MASKS
        )),
        "ao": load_asset(import_texture(
            folder / f"{prefix}_ao_1k.jpg", destination,
            unreal.TextureCompressionSettings.TC_MASKS
        )),
    }
    if include_opacity:
        textures["opacity"] = load_asset(import_texture(
            folder / f"{prefix}_alpha_1k.png", destination,
            unreal.TextureCompressionSettings.TC_MASKS
        ))
    return textures


def ensure_asset(asset_name, package_path, asset_class, factory):
    object_path = f"{package_path}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(object_path):
        return unreal.EditorAssetLibrary.load_asset(object_path)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, package_path, asset_class, factory
    )


def scalar_parameter(material, name, default, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, x, y
    )
    expression.set_editor_property("parameter_name", name)
    expression.set_editor_property("default_value", default)
    return expression


def vector_parameter(material, name, default, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, x, y
    )
    expression.set_editor_property("parameter_name", name)
    expression.set_editor_property("default_value", default)
    return expression


def texture_parameter(material, name, texture, sampler_type, coordinates, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, x, y
    )
    expression.set_editor_property("parameter_name", name)
    expression.set_editor_property("texture", texture)
    expression.set_editor_property("sampler_type", sampler_type)
    unreal.MaterialEditingLibrary.connect_material_expressions(coordinates, "", expression, "Coordinates")
    return expression


def constant(material, value, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, x, y
    )
    expression.set_editor_property("r", value)
    return expression


def rebuild_surface_master(defaults):
    material = ensure_asset(
        "M_PineRidgeTerrain", MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
    )
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("used_with_instanced_static_meshes", True)

    texcoord = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -900, 0
    )
    u_tiling = scalar_parameter(material, "UTiling", 8.0, -900, 190)
    v_tiling = scalar_parameter(material, "VTiling", 8.0, -900, 310)
    tiling = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, -670, 230
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(u_tiling, "", tiling, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(v_tiling, "", tiling, "B")
    scaled_uv = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -450, 30
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(texcoord, "", scaled_uv, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tiling, "", scaled_uv, "B")

    world_position = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionWorldPosition, -900, 520
    )
    world_xy = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -680, 520
    )
    world_xy.set_editor_property("r", True)
    world_xy.set_editor_property("g", True)
    world_xy.set_editor_property("b", False)
    world_xy.set_editor_property("a", False)
    world_uv_scale = scalar_parameter(material, "WorldUVScale", 1.0 / 600.0, -680, 640)
    world_uv = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -450, 560
    )
    use_world_uv = scalar_parameter(material, "UseWorldUV", 0.0, -450, 680)
    selected_uv = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionLinearInterpolate, -220, 40
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(world_position, "", world_xy, "")
    unreal.MaterialEditingLibrary.connect_material_expressions(world_xy, "", world_uv, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(world_uv_scale, "", world_uv, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(scaled_uv, "", selected_uv, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(world_uv, "", selected_uv, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(use_world_uv, "", selected_uv, "Alpha")

    detail_uv_scale = scalar_parameter(material, "DetailWorldUVScale", 1.0 / 220.0,
        -680, 760)
    detail_world_uv = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -450, 760
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(world_xy, "", detail_world_uv, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(detail_uv_scale, "", detail_world_uv, "B")

    base = texture_parameter(
        material, "BaseColorTexture", defaults["diffuse"],
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, selected_uv, 20, -180
    )
    detail_base = texture_parameter(
        material, "DetailBaseColorTexture", defaults["diffuse"],
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, detail_world_uv, 20, -300
    )
    normal = texture_parameter(
        material, "NormalTexture", defaults["normal"],
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, selected_uv, 20, 20
    )
    rough = texture_parameter(
        material, "RoughnessTexture", defaults["rough"],
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, selected_uv, 20, 220
    )
    ao = texture_parameter(
        material, "AmbientOcclusionTexture", defaults["ao"],
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, selected_uv, 20, 420
    )

    tint = vector_parameter(
        material, "BaseColorTint", unreal.LinearColor(0.56, 0.59, 0.52, 1.0), 80, -80
    )
    albedo_power = scalar_parameter(material, "AlbedoContrastPower", 1.0, 80, -200)
    base_contrast = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionPower, 300, -240
    )
    detail_gain = scalar_parameter(material, "DetailGain", 1.75, 80, -360)
    gained_detail = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 300, -360
    )
    detail_modulated = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 520, -300
    )
    detail_strength = scalar_parameter(material, "DetailStrength", 0.0, 520, -400)
    contrasted_base = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionLinearInterpolate, 740, -260
    )
    tinted_base = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 960, -160
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(base, "RGB", base_contrast, "Base")
    unreal.MaterialEditingLibrary.connect_material_expressions(albedo_power, "", base_contrast, "Exp")
    unreal.MaterialEditingLibrary.connect_material_expressions(detail_base, "RGB", gained_detail, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(detail_gain, "", gained_detail, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(base_contrast, "", detail_modulated, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(gained_detail, "", detail_modulated, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(base_contrast, "", contrasted_base, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(detail_modulated, "", contrasted_base, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(detail_strength, "", contrasted_base, "Alpha")
    unreal.MaterialEditingLibrary.connect_material_expressions(contrasted_base, "", tinted_base, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tint, "", tinted_base, "B")
    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVertexColor, 300, 20
    )
    macro_range = scalar_parameter(material, "MacroVariationRange", 0.34, 520, 80)
    macro_minimum = scalar_parameter(material, "MacroVariationMinimum", 0.78, 520, 140)
    macro_scaled = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 740, 80
    )
    macro_factor = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, 960, 80
    )
    varied_base = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 1180, -120
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color, "R", macro_scaled, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(macro_range, "", macro_scaled, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(macro_scaled, "", macro_factor, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(macro_minimum, "", macro_factor, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(tinted_base, "", varied_base, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(macro_factor, "", varied_base, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        varied_base, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    ambient_lift = scalar_parameter(material, "AmbientColorLift", 0.025, 960, 220)
    ambient_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 1180, 200
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(varied_base, "", ambient_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(ambient_lift, "", ambient_color, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        ambient_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        normal, "RGB", unreal.MaterialProperty.MP_NORMAL
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, "R", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION
    )
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def rebuild_grass_blade_material(defaults, card_alpha):
    """Build a masked, wind-reactive grass-card material for HISM ground cover."""
    material = ensure_asset(
        "M_PineRidgeGrassBlade", MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
    )
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("opacity_mask_clip_value", 0.05)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    texcoord = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -920, 300
    )
    opacity_sample = texture_parameter(material, "CardAlphaTexture", card_alpha,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, texcoord, 20, 300)
    tex_u = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -800, 280
    )
    tex_u.set_editor_property("r", True)
    tex_u.set_editor_property("g", False)
    tex_u.set_editor_property("b", False)
    tex_u.set_editor_property("a", False)
    tex_v = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -800, 380
    )
    tex_v.set_editor_property("r", False)
    tex_v.set_editor_property("g", True)
    tex_v.set_editor_property("b", False)
    tex_v.set_editor_property("a", False)
    unreal.MaterialEditingLibrary.connect_material_expressions(texcoord, "", tex_u, "")
    unreal.MaterialEditingLibrary.connect_material_expressions(texcoord, "", tex_v, "")
    instance_random = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionPerInstanceRandom, -920, 40
    )
    tint = vector_parameter(
        material, "BladeTint", unreal.LinearColor(0.20, 0.52, 0.16, 1.0), -920, -280
    )
    shade_range = scalar_parameter(material, "ShadeRange", 0.26, -920, 120)
    shade_min = scalar_parameter(material, "ShadeMinimum", 0.82, -920, 180)
    random_range = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -650, 40
    )
    random_shade = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -430, 20
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(instance_random, "", random_range, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(shade_range, "", random_range, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(random_range, "", random_shade, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(shade_min, "", random_shade, "B")
    final_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -180, -160
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(tint, "", final_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(random_shade, "", final_color, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        final_color, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    ambient_lift = scalar_parameter(material, "AmbientColorLift", 0.06, -180, 20)
    ambient_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 40, -40
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(final_color, "", ambient_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(ambient_lift, "", ambient_color, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        ambient_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    roughness = scalar_parameter(material, "Roughness", 0.84, 40, 100)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )

    # The imported authored mask supplies the stable blade silhouette. The following
    # dormant nodes retain tunable analytic values for future card-shape experiments.
    u_scale = scalar_parameter(material, "CardUScale", 2.0, -920, 380)
    u_center = scalar_parameter(material, "CardUCenter", 1.0, -920, 440)
    scaled_u = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -650, 340
    )
    centered_u = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSubtract, -430, 340
    )
    abs_u = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAbs, -210, 340
    )
    taper_range = scalar_parameter(material, "TaperRange", 0.82, -650, 500)
    tip_width = scalar_parameter(material, "TipWidth", 0.10, -650, 560)
    taper = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -430, 500
    )
    allowed_width = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -210, 500
    )
    silhouette = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSubtract, 20, 420
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(tex_u, "", scaled_u, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(u_scale, "", scaled_u, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(scaled_u, "", centered_u, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(u_center, "", centered_u, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(centered_u, "", abs_u, "")
    unreal.MaterialEditingLibrary.connect_material_expressions(tex_v, "", taper, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(taper_range, "", taper, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(taper, "", allowed_width, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tip_width, "", allowed_width, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(allowed_width, "", silhouette, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(abs_u, "", silhouette, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity_sample, "R", unreal.MaterialProperty.MP_OPACITY_MASK
    )

    # Per-instance phase prevents synchronized motion. The UV mask anchors roots while the
    # tip responds most strongly, and material instances tune each species independently.
    time = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTime, -920, 720
    )
    wind_speed = scalar_parameter(material, "WindSpeed", 0.42, -920, 780)
    time_scaled = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -650, 700
    )
    phase_spread = scalar_parameter(material, "PhaseSpread", 1.0, -920, 840)
    random_phase = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -650, 800
    )
    phase = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -430, 740
    )
    wave = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSine, -210, 740
    )
    tip_mask = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSubtract, -430, 900
    )
    root_anchor = scalar_parameter(material, "RootAnchor", 1.0, -650, 940)
    wind_strength = scalar_parameter(material, "WindStrength", 7.0, -210, 860)
    wave_tip = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 20, 760
    )
    wind_amount = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 240, 760
    )
    wind_direction = vector_parameter(material, "WindDirection",
        unreal.LinearColor(0.86, 0.52, 0.0, 0.0), 20, 940)
    wind_offset = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 460, 820
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(time, "", time_scaled, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(wind_speed, "", time_scaled, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(instance_random, "", random_phase, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(phase_spread, "", random_phase, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(time_scaled, "", phase, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(random_phase, "", phase, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(phase, "", wave, "")
    # The engine Plane's V axis follows local X rather than the card's long local Y axis;
    # use U for the root-to-tip mask after the HISM transform rotates/scales the plane.
    unreal.MaterialEditingLibrary.connect_material_expressions(root_anchor, "", tip_mask, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tex_u, "", tip_mask, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(wave, "", wave_tip, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tip_mask, "", wave_tip, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(wave_tip, "", wind_amount, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(wind_strength, "", wind_amount, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(wind_direction, "", wind_offset, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(wind_amount, "", wind_offset, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        wind_offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET
    )

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def rebuild_ground_blend_material(name, inner, outer, inner_tint, outer_tint):
    """Blend a groomed biome into forest floor using procedural ribbon vertex alpha."""
    material = ensure_asset(name, MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    texcoord = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -1180, 60
    )
    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVertexColor, -1180, -180
    )
    inner_base = texture_parameter(material, "InnerBaseColor", inner["diffuse"],
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, texcoord, -920, -440)
    outer_base = texture_parameter(material, "OuterBaseColor", outer["diffuse"],
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, texcoord, -920, -300)
    inner_tint_node = vector_parameter(material, "InnerTint", inner_tint, -920, -580)
    outer_tint_node = vector_parameter(material, "OuterTint", outer_tint, -920, -160)
    tinted_inner = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -650, -440
    )
    tinted_outer = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -650, -260
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(inner_base, "RGB", tinted_inner, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(inner_tint_node, "", tinted_inner, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(outer_base, "RGB", tinted_outer, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(outer_tint_node, "", tinted_outer, "B")
    base_lerp = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionLinearInterpolate, -360, -350
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(tinted_outer, "", base_lerp, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tinted_inner, "", base_lerp, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color, "A", base_lerp, "Alpha")
    macro_range = scalar_parameter(material, "MacroVariationRange", 0.30, -360, -120)
    macro_minimum = scalar_parameter(material, "MacroVariationMinimum", 0.80, -360, -60)
    macro_scaled = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -100, -110
    )
    macro_factor = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, 120, -110
    )
    varied_base = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 340, -300
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color, "R", macro_scaled, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(macro_range, "", macro_scaled, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(macro_scaled, "", macro_factor, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(macro_minimum, "", macro_factor, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(base_lerp, "", varied_base, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(macro_factor, "", varied_base, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        varied_base, "", unreal.MaterialProperty.MP_BASE_COLOR
    )

    ambient_lift = scalar_parameter(material, "AmbientColorLift", 0.025, 340, -180)
    ambient = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 560, -220
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(varied_base, "", ambient, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(ambient_lift, "", ambient, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        ambient, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )

    for suffix, property_name, sampler, y in (
        ("Normal", unreal.MaterialProperty.MP_NORMAL, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 40),
        ("Roughness", unreal.MaterialProperty.MP_ROUGHNESS, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 260),
        ("AO", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 480),
    ):
        key = "normal" if suffix == "Normal" else "rough" if suffix == "Roughness" else "ao"
        inner_sample = texture_parameter(material, f"Inner{suffix}", inner[key], sampler,
            texcoord, -650, y)
        outer_sample = texture_parameter(material, f"Outer{suffix}", outer[key], sampler,
            texcoord, -650, y + 90)
        blend = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionLinearInterpolate, -330, y + 40
        )
        channel = "RGB" if suffix == "Normal" else "R"
        unreal.MaterialEditingLibrary.connect_material_expressions(outer_sample, channel, blend, "A")
        unreal.MaterialEditingLibrary.connect_material_expressions(inner_sample, channel, blend, "B")
        unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color, "A", blend, "Alpha")
        unreal.MaterialEditingLibrary.connect_material_property(blend, "", property_name)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def rebuild_litter_material(card_alpha):
    """Create a masked rough material for slope-aligned HISM leaf/needle cards."""
    material = ensure_asset(
        "M_PineRidgeLeafLitter", MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
    )
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("opacity_mask_clip_value", 0.18)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    texcoord = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -720, 220
    )
    opacity_sample = texture_parameter(material, "CardAlphaTexture", card_alpha,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, texcoord, 420, 220)
    tex_u = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -600, 220
    )
    tex_u.set_editor_property("r", True)
    tex_u.set_editor_property("g", False)
    tex_u.set_editor_property("b", False)
    tex_u.set_editor_property("a", False)
    tex_v = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -600, 320
    )
    tex_v.set_editor_property("r", False)
    tex_v.set_editor_property("g", True)
    tex_v.set_editor_property("b", False)
    tex_v.set_editor_property("a", False)
    unreal.MaterialEditingLibrary.connect_material_expressions(texcoord, "", tex_u, "")
    unreal.MaterialEditingLibrary.connect_material_expressions(texcoord, "", tex_v, "")
    instance_random = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionPerInstanceRandom, -720, 40
    )
    tint = vector_parameter(material, "LitterTint",
        unreal.LinearColor(0.74, 0.56, 0.32, 1.0), -720, -260)
    shade_range = scalar_parameter(material, "ShadeRange", 0.30, -720, 100)
    shade_min = scalar_parameter(material, "ShadeMinimum", 0.72, -720, 160)
    random_range = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -460, 20
    )
    random_shade = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -240, 20
    )
    base = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 0, -120
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(instance_random, "", random_range, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(shade_range, "", random_range, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(random_range, "", random_shade, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(shade_min, "", random_shade, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(tint, "", base, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(random_shade, "", base, "B")
    unreal.MaterialEditingLibrary.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = scalar_parameter(material, "Roughness", 0.94, -160, 20)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # A weighted UV diamond removes the engine plane's rectangular corners and reads as
    # either broad leaves or narrow needle clumps after species-specific instance scaling.
    uv_scale = scalar_parameter(material, "CardUVScale", 2.0, -720, 300)
    uv_center = scalar_parameter(material, "CardUVCenter", 1.0, -720, 360)
    u_scaled = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -460, 260)
    v_scaled = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -460, 400)
    u_centered = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSubtract, -240, 260)
    v_centered = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSubtract, -240, 400)
    abs_u = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAbs, -20, 260)
    abs_v = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAbs, -20, 400)
    v_weight = scalar_parameter(material, "LengthWeight", 0.58, -20, 500)
    weighted_v = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 200, 400)
    distance = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, 420, 320)
    silhouette = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionOneMinus, 640, 320)
    unreal.MaterialEditingLibrary.connect_material_expressions(tex_u, "", u_scaled, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(uv_scale, "", u_scaled, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(tex_v, "", v_scaled, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(uv_scale, "", v_scaled, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(u_scaled, "", u_centered, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(uv_center, "", u_centered, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(v_scaled, "", v_centered, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(uv_center, "", v_centered, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(u_centered, "", abs_u, "")
    unreal.MaterialEditingLibrary.connect_material_expressions(v_centered, "", abs_v, "")
    unreal.MaterialEditingLibrary.connect_material_expressions(abs_v, "", weighted_v, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(v_weight, "", weighted_v, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(abs_u, "", distance, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(weighted_v, "", distance, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(distance, "", silhouette, "")
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity_sample, "R", unreal.MaterialProperty.MP_OPACITY_MASK)
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def _rebuild_water_material_experimental():
    """Create an original low-cost lake material; no third-party texture is required."""
    material = ensure_asset(
        "M_GalleryLakeWater", MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
    )
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    # Opaque lit water is intentional for the baseline tier: it preserves moving geometry/normals,
    # avoids translucent sorting, and stays readable under the lightweight runtime sky rig.
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", True)

    texcoord = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -1180, -40
    )
    tiling = scalar_parameter(material, "WaveTiling", 14.0, -1180, 100)
    scaled_uv = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -950, -20
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(texcoord, "", scaled_uv, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tiling, "", scaled_uv, "B")

    panner_a = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionPanner, -720, -140
    )
    panner_a.set_editor_property("speed_x", 0.018)
    panner_a.set_editor_property("speed_y", 0.009)
    panner_b = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionPanner, -720, 100
    )
    panner_b.set_editor_property("speed_x", -0.011)
    panner_b.set_editor_property("speed_y", 0.021)
    unreal.MaterialEditingLibrary.connect_material_expressions(scaled_uv, "", panner_a, "Coordinate")
    unreal.MaterialEditingLibrary.connect_material_expressions(scaled_uv, "", panner_b, "Coordinate")

    mask_a = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -500, -140
    )
    mask_a.set_editor_property("r", True)
    mask_b = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -500, 100
    )
    mask_b.set_editor_property("g", True)
    unreal.MaterialEditingLibrary.connect_material_expressions(panner_a, "", mask_a, "")
    unreal.MaterialEditingLibrary.connect_material_expressions(panner_b, "", mask_b, "")

    sine_a = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSine, -290, -140
    )
    sine_b = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSine, -290, 100
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(mask_a, "", sine_a, "")
    unreal.MaterialEditingLibrary.connect_material_expressions(mask_b, "", sine_b, "")
    wave_sum = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -80, -20
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(sine_a, "", wave_sum, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(sine_b, "", wave_sum, "B")

    wave_amplitude = scalar_parameter(material, "WaveAmplitudeCm", 3.5, -80, 150)
    wave_height = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 150, 30
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(wave_sum, "", wave_height, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(wave_amplitude, "", wave_height, "B")
    zero = constant(material, 0.0, -80, 280)
    zero_xy = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, 150, 250
    )
    offset = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, 390, 120
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(zero, "", zero_xy, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(zero, "", zero_xy, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(zero_xy, "", offset, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(wave_height, "", offset, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET
    )

    normal_strength = scalar_parameter(material, "NormalStrength", 0.075, 150, 390)
    normal_x = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 390, 330
    )
    normal_y = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 390, 440
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(sine_a, "", normal_x, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(normal_strength, "", normal_x, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(sine_b, "", normal_y, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(normal_strength, "", normal_y, "B")
    normal_xy = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, 620, 370
    )
    normal_xyz = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, 840, 370
    )
    one = constant(material, 1.0, 620, 520)
    unreal.MaterialEditingLibrary.connect_material_expressions(normal_x, "", normal_xy, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(normal_y, "", normal_xy, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(normal_xy, "", normal_xyz, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(one, "", normal_xyz, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        normal_xyz, "", unreal.MaterialProperty.MP_NORMAL
    )

    deep = vector_parameter(
        material, "DeepWaterColor", unreal.LinearColor(0.018, 0.16, 0.25, 1.0), -500, -500
    )
    shallow = vector_parameter(
        material, "ShallowWaterColor", unreal.LinearColor(0.055, 0.34, 0.30, 1.0), -500, -380
    )
    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVertexColor, -500, -250
    )
    shallow_mask = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -280, -250
    )
    shallow_mask.set_editor_property("r", True)
    shore_mask = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, -280, -150
    )
    shore_mask.set_editor_property("g", True)
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color, "", shallow_mask, "")
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color, "", shore_mask, "")
    water_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionLinearInterpolate, -40, -420
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(deep, "", water_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(shallow, "", water_color, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(shallow_mask, "", water_color, "Alpha")
    foam_tint = vector_parameter(
        material, "ShoreTint", unreal.LinearColor(0.30, 0.45, 0.40, 1.0), -40, -230
    )
    foam = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 180, -230
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(foam_tint, "", foam, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(shore_mask, "", foam, "B")
    final_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, 410, -350
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(water_color, "", final_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(foam, "", final_color, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        final_color, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    ambient_lift = scalar_parameter(material, "AmbientColorLift", 0.52, 410, -260)
    ambient_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 650, -350
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(final_color, "", ambient_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(ambient_lift, "", ambient_color, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        ambient_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )

    roughness = scalar_parameter(material, "Roughness", 0.16, 410, -210)
    specular = scalar_parameter(material, "Specular", 0.82, 410, -120)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        specular, "", unreal.MaterialProperty.MP_SPECULAR
    )

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def rebuild_water_material():
    """Build the production baseline lake shader from scalar radial-wave inputs."""
    water_asset_path = f"{MATERIAL_ROOT}/M_GalleryLakeWater"
    if unreal.EditorAssetLibrary.does_asset_exist(water_asset_path):
        # Recreate this generated-only asset so retired expression nodes cannot survive graph migrations.
        if not unreal.EditorAssetLibrary.delete_asset(water_asset_path):
            raise RuntimeError("Could not replace generated Gallery Lake water material")
    material = ensure_asset(
        "M_GalleryLakeWater", MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
    )
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", True)

    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVertexColor, -1000, -180
    )
    deep = vector_parameter(
        material, "DeepWaterColor", unreal.LinearColor(0.006, 0.040, 0.055, 1.0), -1000, -480
    )
    shallow = vector_parameter(
        material, "ShallowWaterColor", unreal.LinearColor(0.012, 0.075, 0.060, 1.0), -1000, -360
    )
    water_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionLinearInterpolate, -700, -400
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(deep, "", water_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(shallow, "", water_color, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color, "R", water_color, "Alpha")

    shore_tint = vector_parameter(
        material, "ShoreTint", unreal.LinearColor(0.004, 0.010, 0.008, 1.0), -700, -250
    )
    shore_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -450, -250
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(shore_tint, "", shore_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color, "G", shore_color, "B")
    final_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -190, -360
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(water_color, "", final_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(shore_color, "", final_color, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        final_color, "", unreal.MaterialProperty.MP_BASE_COLOR
    )

    ambient_lift = scalar_parameter(material, "AmbientColorLift", 0.12, -190, -220)
    ambient_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 60, -330
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(final_color, "", ambient_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(ambient_lift, "", ambient_color, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        ambient_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )

    time = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTime, -1000, 40
    )
    wave_speed = scalar_parameter(material, "WaveSpeed", 0.16, -1000, 150)
    moving_time = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -760, 50
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(time, "", moving_time, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(wave_speed, "", moving_time, "B")
    wave_tiling = scalar_parameter(material, "WaveTiling", 14.0, -1000, 260)
    radial_phase = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -760, 230
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color, "R", radial_phase, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(wave_tiling, "", radial_phase, "B")
    angular_mix = scalar_parameter(material, "AngularWaveMix", 2.6, -760, 340)
    angular_phase = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -520, 300
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color, "B", angular_phase, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(angular_mix, "", angular_phase, "B")
    spatial_phase = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -300, 240
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(radial_phase, "", spatial_phase, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(angular_phase, "", spatial_phase, "B")
    phase = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -520, 100
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(moving_time, "", phase, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(spatial_phase, "", phase, "B")
    sine = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSine, -290, 100
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(phase, "", sine, "")
    amplitude = scalar_parameter(material, "WaveAmplitudeCm", 3.5, -290, 210)
    height = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -50, 100
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(sine, "", height, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(amplitude, "", height, "B")
    up = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -50, 280
    )
    up.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    offset = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 190, 190
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(up, "", offset, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(height, "", offset, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET
    )

    normal_strength = scalar_parameter(material, "NormalStrength", 0.065, 190, 310)
    quarter_phase = constant(material, 0.25, -50, 430)
    phase_y = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, 190, 430
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(phase, "", phase_y, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(quarter_phase, "", phase_y, "B")
    sine_y = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSine, 420, 430
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(phase_y, "", sine_y, "")
    slope_x = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 420, 230
    )
    slope_y = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 640, 430
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(sine, "", slope_x, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(normal_strength, "", slope_x, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(sine_y, "", slope_y, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(normal_strength, "", slope_y, "B")
    axis_x = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, 420, 540
    )
    axis_x.set_editor_property("constant", unreal.LinearColor(1.0, 0.0, 0.0, 0.0))
    axis_y = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, 640, 540
    )
    axis_y.set_editor_property("constant", unreal.LinearColor(0.0, 1.0, 0.0, 0.0))
    normal_x = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 650, 230
    )
    normal_y = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 860, 430
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(axis_x, "", normal_x, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(slope_x, "", normal_x, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(axis_y, "", normal_y, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(slope_y, "", normal_y, "B")
    normal_xy = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, 1080, 330
    )
    final_normal = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, 1290, 270
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(normal_x, "", normal_xy, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(normal_y, "", normal_xy, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(normal_xy, "", final_normal, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(up, "", final_normal, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        final_normal, "", unreal.MaterialProperty.MP_NORMAL
    )

    roughness = scalar_parameter(material, "Roughness", 0.18, 60, -180)
    specular = scalar_parameter(material, "Specular", 0.78, 60, -90)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        specular, "", unreal.MaterialProperty.MP_SPECULAR
    )

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def rebuild_foliage_master(name, defaults, masked):
    material = ensure_asset(name, MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", masked)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    if masked:
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        material.set_editor_property("opacity_mask_clip_value", 0.36)

    texcoord = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -600, 20
    )
    base = texture_parameter(
        material, "BaseColorTexture", defaults["diffuse"],
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, texcoord, -260, -180
    )
    normal = texture_parameter(
        material, "NormalTexture", defaults["normal"],
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, texcoord, -260, 20
    )
    rough = texture_parameter(
        material, "RoughnessTexture", defaults["rough"],
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, texcoord, -260, 220
    )
    default_tint = unreal.LinearColor(0.58, 0.60, 0.52, 1.0) if not masked \
        else unreal.LinearColor(0.42, 0.67, 0.38, 1.0)
    tint = vector_parameter(material, "BaseColorTint", default_tint, 0, -80)
    tinted_base = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 220, -160
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(base, "RGB", tinted_base, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tint, "", tinted_base, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        tinted_base, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        normal, "RGB", unreal.MaterialProperty.MP_NORMAL
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, "R", unreal.MaterialProperty.MP_ROUGHNESS
    )
    if "ao" in defaults:
        ao = texture_parameter(
            material, "AmbientOcclusionTexture", defaults["ao"],
            unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, texcoord, -260, 420
        )
        unreal.MaterialEditingLibrary.connect_material_property(
            ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION
        )
    if masked:
        opacity = texture_parameter(
            material, "OpacityTexture", defaults["opacity"],
            unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, texcoord, -260, 620
        )
        unreal.MaterialEditingLibrary.connect_material_property(
            opacity, "R", unreal.MaterialProperty.MP_OPACITY_MASK
        )

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def ensure_material_instance(name, parent, textures, vectors=None, scalars=None):
    factory = unreal.MaterialInstanceConstantFactoryNew()
    instance = ensure_asset(name, MATERIAL_ROOT, unreal.MaterialInstanceConstant, factory)
    instance.set_editor_property("parent", parent)
    for parameter_name, texture in textures.items():
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
            instance, parameter_name, texture
        )
    for parameter_name, value in (vectors or {}).items():
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            instance, parameter_name, value
        )
    for parameter_name, value in (scalars or {}).items():
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            instance, parameter_name, value
        )
    unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False)
    return instance


def make_fbx_options(combine_meshes):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    static_data = options.get_editor_property("static_mesh_import_data")
    static_data.set_editor_property("combine_meshes", combine_meshes)
    static_data.set_editor_property("generate_lightmap_u_vs", True)
    static_data.set_editor_property("auto_generate_collision", False)
    static_data.set_editor_property("convert_scene", True)
    static_data.set_editor_property("convert_scene_unit", True)
    return options


def configure_static_meshes(root, material_resolver):
    paths = []
    diagnostics = []
    for asset_path in unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False):
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(asset, unreal.StaticMesh):
            continue
        paths.append(asset_path)
        for slot_index, slot in enumerate(asset.get_editor_property("static_materials")):
            asset.set_material(slot_index, material_resolver(slot))
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
        bounds = asset.get_bounds()
        diagnostics.append({
            "asset": asset_path,
            "boundsOriginCm": [bounds.origin.x, bounds.origin.y, bounds.origin.z],
            "boundsExtentCm": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z],
            "materialSlots": [
                str(slot.get_editor_property("material_slot_name"))
                for slot in asset.get_editor_property("static_materials")
            ],
        })
    return paths, diagnostics


def configure_lod_chain(root, reductions, subsystem):
    if subsystem is None:
        raise RuntimeError("StaticMeshEditorSubsystem is unavailable; run this pipeline with UnrealEditor.exe, not UnrealEditor-Cmd.exe")
    for asset_path in unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False):
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(asset, unreal.StaticMesh):
            continue
        options = unreal.StaticMeshReductionOptions()
        options.set_editor_property("auto_compute_lod_screen_size", False)
        settings = []
        for percent_triangles, screen_size in reductions:
            setting = unreal.StaticMeshReductionSettings()
            setting.set_editor_property("percent_triangles", percent_triangles)
            setting.set_editor_property("screen_size", screen_size)
            settings.append(setting)
        options.set_editor_property("reduction_settings", settings)
        generated = subsystem.set_lods(asset, options)
        if generated != len(settings):
            raise RuntimeError(
                f"LOD generation returned {generated}, expected {len(settings)} for {asset_path}"
            )
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


def main():
    manifest = SOURCE_ROOT / "asset_manifest.json"
    if not manifest.is_file():
        raise RuntimeError(
            "Missing source manifest. Run Scripts/download-pine-ridge-cc0-assets.ps1 first."
        )

    # LOD generation is part of the atomic import contract. Commandlet mode does not expose
    # StaticMeshEditorSubsystem in UE 5.8, so reject it before reimport can strip existing LODs.
    static_mesh_subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if static_mesh_subsystem is None:
        raise RuntimeError(
            "Pine Ridge import requires UnrealEditor.exe because UE 5.8 commandlet mode "
            "does not expose StaticMeshEditorSubsystem"
        )

    unreal.EditorAssetLibrary.make_directory(TEXTURE_ROOT)
    unreal.EditorAssetLibrary.make_directory(FOLIAGE_ROOT)
    unreal.EditorAssetLibrary.make_directory(FIXTURE_ROOT)
    unreal.EditorAssetLibrary.make_directory(MATERIAL_ROOT)
    ground_cover_masks = ensure_ground_cover_textures()

    terrain_sources = {
        "ForestGround": SOURCE_ROOT / "Textures" / "ForestGround01",
        "LeafyGrass": SOURCE_ROOT / "Textures" / "LeafyGrass",
        "GrassPath": SOURCE_ROOT / "Textures" / "GrassPath2",
    }
    prefixes = {
        "ForestGround": "forrest_ground_01",
        "LeafyGrass": "leafy_grass",
        "GrassPath": "grass_path_2",
    }
    textures = {}
    imported_paths = []
    for family, folder in terrain_sources.items():
        destination = f"{TEXTURE_ROOT}/{family}"
        prefix = prefixes[family]
        textures[family] = {
            "diffuse": load_asset(import_texture(
                folder / f"{prefix}_diff_2k.jpg", destination,
                unreal.TextureCompressionSettings.TC_DEFAULT
            )),
            "normal": load_asset(import_texture(
                folder / f"{prefix}_nor_dx_2k.jpg", destination,
                unreal.TextureCompressionSettings.TC_NORMALMAP
            )),
            "rough": load_asset(import_texture(
                folder / f"{prefix}_rough_2k.jpg", destination,
                unreal.TextureCompressionSettings.TC_MASKS
            )),
            "ao": load_asset(import_texture(
                folder / f"{prefix}_ao_2k.jpg", destination,
                unreal.TextureCompressionSettings.TC_MASKS
            )),
        }

    fir_texture_root = SOURCE_ROOT / "Models" / "FirSapling" / "Textures"
    fir_destination = f"{TEXTURE_ROOT}/FirSapling"
    fir = {
        "branches_diffuse": load_asset(import_texture(
            fir_texture_root / "fir_sapling_branches_diff_1k.jpg", fir_destination,
            unreal.TextureCompressionSettings.TC_DEFAULT
        )),
        "branches_normal": load_asset(import_texture(
            fir_texture_root / "fir_sapling_branches_nor_dx_1k.png", fir_destination,
            unreal.TextureCompressionSettings.TC_NORMALMAP
        )),
        "branches_rough": load_asset(import_texture(
            fir_texture_root / "fir_sapling_branches_rough_1k.jpg", fir_destination,
            unreal.TextureCompressionSettings.TC_MASKS
        )),
        "twigs_diffuse": load_asset(import_texture(
            fir_texture_root / "fir_sapling_twigs_diff_1k.jpg", fir_destination,
            unreal.TextureCompressionSettings.TC_DEFAULT
        )),
        "twigs_normal": load_asset(import_texture(
            fir_texture_root / "fir_sapling_twigs_nor_dx_1k.png", fir_destination,
            unreal.TextureCompressionSettings.TC_NORMALMAP
        )),
        "twigs_rough": load_asset(import_texture(
            fir_texture_root / "fir_sapling_twigs_rough_1k.jpg", fir_destination,
            unreal.TextureCompressionSettings.TC_MASKS
        )),
        "twigs_opacity": load_asset(import_texture(
            fir_texture_root / "fir_sapling_twigs_alpha_1k.png", fir_destination,
            unreal.TextureCompressionSettings.TC_MASKS
        )),
    }

    boulder = load_pbr_texture_set(
        SOURCE_ROOT / "Models" / "Boulder01" / "Textures", "boulder_01",
        f"{TEXTURE_ROOT}/Boulder01"
    )
    shrub = load_pbr_texture_set(
        SOURCE_ROOT / "Models" / "Shrub04" / "Textures", "shrub_04",
        f"{TEXTURE_ROOT}/Shrub04", include_opacity=True
    )
    sign_wood = load_pbr_texture_set(
        SOURCE_ROOT / "Textures" / "WeatheredPlanks", "weathered_planks",
        f"{TEXTURE_ROOT}/WeatheredPlanks"
    )

    surface_master = rebuild_surface_master(textures["ForestGround"])
    grass_blade_material = rebuild_grass_blade_material(
        textures["LeafyGrass"], ground_cover_masks["pine_ridge_grass_card_alpha.png"])
    fairway_blend_material = rebuild_ground_blend_material(
        "M_PineRidgeFairwayBlend", textures["LeafyGrass"], textures["ForestGround"],
        unreal.LinearColor(0.14, 0.27, 0.09, 1.0),
        unreal.LinearColor(0.22, 0.27, 0.16, 1.0),
    )
    trail_blend_material = rebuild_ground_blend_material(
        "M_PineRidgeTrailBlend", textures["GrassPath"], textures["ForestGround"],
        unreal.LinearColor(0.30, 0.25, 0.17, 1.0),
        unreal.LinearColor(0.22, 0.27, 0.16, 1.0),
    )
    litter_material = rebuild_litter_material(
        ground_cover_masks["pine_ridge_litter_card_alpha.png"])
    water_material = rebuild_water_material()
    surface_instances = {
        "MI_PineRidgeFairway": (textures["LeafyGrass"], unreal.LinearColor(0.14, 0.27, 0.09, 1.0)),
        "MI_PineRidgeForestFloor": (textures["ForestGround"], unreal.LinearColor(0.38, 0.42, 0.36, 1.0)),
        "MI_PineRidgePath": (textures["GrassPath"], unreal.LinearColor(0.30, 0.25, 0.17, 1.0)),
    }
    for instance_name, (maps, tint) in surface_instances.items():
        ensure_material_instance(
            instance_name, surface_master,
            {
                "BaseColorTexture": maps["diffuse"],
                "DetailBaseColorTexture": maps["diffuse"],
                "NormalTexture": maps["normal"],
                "RoughnessTexture": maps["rough"],
                "AmbientOcclusionTexture": maps["ao"],
            },
            {"BaseColorTint": tint},
            {"AmbientColorLift": 0.01
                 if instance_name == "MI_PineRidgeForestFloor" else 0.025,
             "UseWorldUV": 1.0 if instance_name == "MI_PineRidgeForestFloor" else 0.0,
             # Poly Haven's forest-ground scan reads at the correct physical scale
             # when one repeat covers roughly two metres. A sub-metre secondary
             # sample breaks up repetition without turning the floor into noise.
             "WorldUVScale": 1.0 / 200.0,
             "DetailWorldUVScale": 1.0 / 75.0,
             "DetailStrength": 0.12
                 if instance_name == "MI_PineRidgeForestFloor" else 0.0,
             "DetailGain": 1.30,
             "AlbedoContrastPower": 1.08
                 if instance_name == "MI_PineRidgeForestFloor" else 1.0,
             # The full-range vertex mask remains available for biome-scale breakup,
             # but the shader contribution stays subtle so it does not overpower the
             # scanned twigs and needles with visible procedural bands.
             "MacroVariationRange": 0.10
                 if instance_name == "MI_PineRidgeForestFloor" else 0.14,
             "MacroVariationMinimum": 0.90
                 if instance_name == "MI_PineRidgeForestFloor" else 0.86},
        )
    trail_wear_instance = ensure_material_instance(
        "MI_PineRidgeTrailWear", surface_master,
        {
            "BaseColorTexture": textures["GrassPath"]["diffuse"],
            "NormalTexture": textures["GrassPath"]["normal"],
            "RoughnessTexture": textures["GrassPath"]["rough"],
            "AmbientOcclusionTexture": textures["GrassPath"]["ao"],
        },
        {"BaseColorTint": unreal.LinearColor(0.20, 0.145, 0.085, 1.0)},
        {"UTiling": 2.8, "VTiling": 2.8, "AmbientColorLift": 0.07},
    )
    shore_reed_instance = ensure_material_instance(
        "MI_PineRidgeShoreReeds", surface_master,
        {
            "BaseColorTexture": textures["LeafyGrass"]["diffuse"],
            "NormalTexture": textures["LeafyGrass"]["normal"],
            "RoughnessTexture": textures["LeafyGrass"]["rough"],
            "AmbientOcclusionTexture": textures["LeafyGrass"]["ao"],
        },
        {"BaseColorTint": unreal.LinearColor(0.18, 0.30, 0.055, 1.0)},
        {"UTiling": 1.5, "VTiling": 3.5, "AmbientColorLift": 0.06},
    )

    bark_master = rebuild_foliage_master(
        "M_PineRidgeBark",
        {"diffuse": fir["branches_diffuse"], "normal": fir["branches_normal"],
         "rough": fir["branches_rough"]},
        masked=False,
    )
    needle_master = rebuild_foliage_master(
        "M_PineRidgeNeedles",
        {"diffuse": fir["twigs_diffuse"], "normal": fir["twigs_normal"],
         "rough": fir["twigs_rough"], "opacity": fir["twigs_opacity"]},
        masked=True,
    )
    branch_instance = ensure_material_instance(
        "MI_FirSaplingBranches", bark_master,
        {"BaseColorTexture": fir["branches_diffuse"], "NormalTexture": fir["branches_normal"],
         "RoughnessTexture": fir["branches_rough"]},
        {"BaseColorTint": unreal.LinearColor(0.52, 0.50, 0.43, 1.0)},
    )
    twig_instance = ensure_material_instance(
        "MI_FirSaplingTwigs", needle_master,
        {"BaseColorTexture": fir["twigs_diffuse"], "NormalTexture": fir["twigs_normal"],
         "RoughnessTexture": fir["twigs_rough"], "OpacityTexture": fir["twigs_opacity"]},
        {"BaseColorTint": unreal.LinearColor(0.40, 0.68, 0.36, 1.0)},
    )

    rock_master = rebuild_foliage_master("M_PineRidgeRock", boulder, masked=False)
    rock_instance = ensure_material_instance(
        "MI_Boulder01", rock_master,
        {"BaseColorTexture": boulder["diffuse"], "NormalTexture": boulder["normal"],
         "RoughnessTexture": boulder["rough"], "AmbientOcclusionTexture": boulder["ao"]},
        {"BaseColorTint": unreal.LinearColor(0.72, 0.74, 0.69, 1.0)},
    )
    brush_master = rebuild_foliage_master("M_PineRidgeBrush", shrub, masked=True)
    brush_instance = ensure_material_instance(
        "MI_Shrub04", brush_master,
        {"BaseColorTexture": shrub["diffuse"], "NormalTexture": shrub["normal"],
         "RoughnessTexture": shrub["rough"], "AmbientOcclusionTexture": shrub["ao"],
         "OpacityTexture": shrub["opacity"]},
        {"BaseColorTint": unreal.LinearColor(0.52, 0.71, 0.45, 1.0)},
    )
    sign_master = rebuild_foliage_master("M_PineRidgeSignWood", sign_wood, masked=False)
    sign_instance = ensure_material_instance(
        "MI_PineRidgeSignWood", sign_master,
        {"BaseColorTexture": sign_wood["diffuse"], "NormalTexture": sign_wood["normal"],
         "RoughnessTexture": sign_wood["rough"], "AmbientOcclusionTexture": sign_wood["ao"]},
        {"BaseColorTint": unreal.LinearColor(0.88, 0.74, 0.54, 1.0)},
    )

    imported_paths.extend(import_asset(
        SOURCE_ROOT / "Models" / "FirSapling" / "fir_sapling_1k.fbx",
        FOLIAGE_ROOT,
        make_fbx_options(False),
    ))
    boulder_root = f"{FIXTURE_ROOT}/Boulder01"
    shrub_root = f"{FIXTURE_ROOT}/Shrub04"
    imported_paths.extend(import_asset(
        SOURCE_ROOT / "Models" / "Boulder01" / "boulder_01_1k.fbx",
        boulder_root, make_fbx_options(True)
    ))
    imported_paths.extend(import_asset(
        SOURCE_ROOT / "Models" / "Shrub04" / "shrub_04_1k.fbx",
        shrub_root, make_fbx_options(True)
    ))

    # Imported scan/source meshes are intentionally retained at full close-up detail,
    # then reduced aggressively for the thousands of persistent-world instances.
    configure_lod_chain(FOLIAGE_ROOT, [
        (1.00, 1.00), (0.20, 0.22), (0.05, 0.085), (0.0125, 0.030)
    ], static_mesh_subsystem)
    configure_lod_chain(boulder_root, [
        (1.00, 1.00), (0.18, 0.16), (0.035, 0.045)
    ], static_mesh_subsystem)

    static_meshes, mesh_diagnostics = configure_static_meshes(
        FOLIAGE_ROOT,
        lambda slot: twig_instance
        if "twig" in str(slot.get_editor_property("material_slot_name")).lower()
        or "needle" in str(slot.get_editor_property("material_slot_name")).lower()
        else branch_instance,
    )
    boulder_meshes, boulder_diagnostics = configure_static_meshes(
        boulder_root, lambda slot: rock_instance
    )
    shrub_meshes, shrub_diagnostics = configure_static_meshes(
        shrub_root, lambda slot: brush_instance
    )

    if not static_meshes:
        raise RuntimeError("Fir sapling import created no static meshes")
    if len(boulder_meshes) != 1 or len(shrub_meshes) != 1:
        raise RuntimeError(
            f"Fixture import expected one combined boulder and shrub mesh; "
            f"received {len(boulder_meshes)} and {len(shrub_meshes)}"
        )

    receipt = {
        "schema": "disc_golf_pine_ridge_unreal_import",
        "schemaVersion": 2,
        "sourceManifest": str(manifest.relative_to(ROOT)).replace("\\", "/"),
        "contentRoot": CONTENT_ROOT,
        "staticMeshes": static_meshes,
        "meshDiagnostics": mesh_diagnostics,
        "fixtureStaticMeshes": boulder_meshes + shrub_meshes,
        "fixtureMeshDiagnostics": boulder_diagnostics + shrub_diagnostics,
        "terrainMaterial": f"{MATERIAL_ROOT}/M_PineRidgeTerrain",
        "grassMaterial": f"{MATERIAL_ROOT}/M_PineRidgeGrassBlade",
        "fairwayBlendMaterial": f"{MATERIAL_ROOT}/{fairway_blend_material.get_name()}",
        "trailBlendMaterial": f"{MATERIAL_ROOT}/{trail_blend_material.get_name()}",
        "trailWearMaterial": f"{MATERIAL_ROOT}/{trail_wear_instance.get_name()}",
        "litterMaterial": f"{MATERIAL_ROOT}/{litter_material.get_name()}",
        "shoreReedMaterial": f"{MATERIAL_ROOT}/{shore_reed_instance.get_name()}",
        "waterMaterial": f"{MATERIAL_ROOT}/M_GalleryLakeWater",
        "surfaceMaterialInstances": [
            f"{MATERIAL_ROOT}/{name}" for name in surface_instances
        ],
        "foliageMaterials": [
            f"{MATERIAL_ROOT}/MI_FirSaplingBranches",
            f"{MATERIAL_ROOT}/MI_FirSaplingTwigs",
        ],
        "fixtureMaterials": [
            f"{MATERIAL_ROOT}/MI_Boulder01",
            f"{MATERIAL_ROOT}/MI_Shrub04",
            f"{MATERIAL_ROOT}/MI_PineRidgeSignWood",
        ],
    }
    RECEIPT_PATH.parent.mkdir(parents=True, exist_ok=True)
    RECEIPT_PATH.write_text(json.dumps(receipt, indent=2), encoding="utf-8")
    unreal.EditorAssetLibrary.save_directory(CONTENT_ROOT, only_if_is_dirty=False, recursive=True)
    log(f"SUCCESS meshes={len(static_meshes)} receipt={RECEIPT_PATH}")


try:
    main()
except Exception as exc:
    unreal.log_error(f"PINE_RIDGE_IMPORT: FAILED: {exc}")
    raise
