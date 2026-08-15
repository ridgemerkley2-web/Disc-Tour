"""Emit a compact Pine Ridge material/texture diagnostic to the Unreal log."""

import unreal


def describe_texture(path):
    texture = unreal.EditorAssetLibrary.load_asset(path)
    unreal.log(
        f"PINE_RIDGE_DIAG texture={path} srgb={texture.get_editor_property('srgb')} "
        f"compression={texture.get_editor_property('compression_settings')} "
        f"size={texture.blueprint_get_size_x()}x{texture.blueprint_get_size_y()}"
    )


def describe_material(path):
    material = unreal.EditorAssetLibrary.load_asset(path)
    node = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material, unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.log(
        f"PINE_RIDGE_DIAG material={path} base_node={node} "
        f"two_sided={material.get_editor_property('two_sided')} "
        f"blend={material.get_editor_property('blend_mode')}"
    )
    if isinstance(node, unreal.MaterialExpressionTextureSampleParameter2D):
        unreal.log(
            f"PINE_RIDGE_DIAG base_parameter={node.get_editor_property('parameter_name')} "
            f"base_texture={node.get_editor_property('texture')}"
        )


def describe_instance(path):
    instance = unreal.EditorAssetLibrary.load_asset(path)
    value = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(
        instance, "BaseColorTexture"
    )
    tint = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(
        instance, "BaseColorTint"
    )
    u_tiling = unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(
        instance, "UTiling"
    )
    ambient = unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(
        instance, "AmbientColorLift"
    )
    macro_range = unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(
        instance, "MacroVariationRange"
    )
    unreal.log(
        f"PINE_RIDGE_DIAG instance={path} parent={instance.get_editor_property('parent')} "
        f"base={value} tint={tint} u_tiling={u_tiling} ambient={ambient} "
        f"macro_range={macro_range}"
    )


describe_texture(
    "/Game/Presentation/Course/PineRidge/Textures/LeafyGrass/leafy_grass_diff_2k"
)
describe_texture(
    "/Game/Presentation/Course/PineRidge/Textures/ForestGround/forrest_ground_01_diff_2k"
)
describe_material(
    "/Game/Presentation/Course/PineRidge/Materials/M_PineRidgeTerrain"
)
describe_material(
    "/Game/Presentation/Course/PineRidge/Materials/M_PineRidgeBark"
)
describe_instance(
    "/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgeFairway"
)
describe_instance(
    "/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgeForestFloor"
)
describe_instance(
    "/Game/Presentation/Course/PineRidge/Materials/MI_FirSaplingBranches"
)
