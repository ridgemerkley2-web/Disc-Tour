"""Create asset-agnostic production environment folders and starter Unreal assets.

Run in full UnrealEditor. Existing, internally licensed CC0 Pine Ridge meshes are wired only
as provisional young conifer/shrub/rock variants when those slots are empty; Fab production
slots and any approved or tuned variants already in the data asset are never replaced.
"""

import unreal


ROOT = "/Game/Environment"
FOLDERS = [
    f"{ROOT}/Forest/Trees",
    f"{ROOT}/Forest/Shrubs",
    f"{ROOT}/Forest/Ferns",
    f"{ROOT}/Forest/Grass",
    f"{ROOT}/Forest/GroundCover",
    f"{ROOT}/Forest/Logs",
    f"{ROOT}/Forest/Rocks",
    f"{ROOT}/Forest/Materials",
    f"{ROOT}/Forest/PCG",
    f"{ROOT}/Water",
    f"{ROOT}/Course",
    f"{ROOT}/Optimization",
]

ASSET_SET_PATH = f"{ROOT}/Forest/DA_TemperateMountainForest_Assets"
PRESET_PATH = f"{ROOT}/Forest/DA_TemperateMountainForest"
GRAPH_PATH = f"{ROOT}/Forest/PCG/PCG_TemperateMountainForest"
MPC_PATH = f"{ROOT}/Forest/Materials/MPC_EnvironmentWind"


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"Could not create content folder {path}")


def ensure_asset(path, asset_class, factory=None):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        existing = unreal.EditorAssetLibrary.load_asset(path)
        if existing:
            return existing
        raise RuntimeError(f"Asset exists but could not be loaded: {path}")
    package, name = path.rsplit("/", 1)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, package, asset_class, factory)


for folder in FOLDERS:
    ensure_folder(folder)

asset_set = ensure_asset(ASSET_SET_PATH, unreal.DiscGolfEnvironmentAssetSet, unreal.DataAssetFactory())
asset_set.set_editor_property("asset_set_id", "TemperateMountainForest_Assets")

category = unreal.DiscGolfEnvironmentAssetCategory
known = {
    category.TREE_CONIFER_YOUNG: [
        "/Game/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_a",
        "/Game/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_b",
        "/Game/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_c",
    ],
    category.SHRUB: [
        "/Game/Presentation/Course/PineRidge/Fixtures/PolyHaven/Shrub04/shrub_04_1k",
    ],
    category.ROCK_LARGE: [
        "/Game/Presentation/Course/PineRidge/Fixtures/PolyHaven/Boulder01/boulder_01_1k",
    ],
}

slots = list(asset_set.get_editor_property("slots"))
initialized_slot_count = 0
retained_slot_count = 0
for index, slot in enumerate(slots):
    # This script is also used to rebuild the starter PCG graph. Once an artist or the binder
    # has approved a slot, rerunning graph setup must not discard its visual/collision/interaction
    # proxies or its per-variant weight and scale tuning. Treat every non-empty slot as owned data.
    existing_variants = list(slot.get_editor_property("variants"))
    if existing_variants:
        retained_slot_count += 1
        slots[index] = slot
        continue

    paths = known.get(slot.get_editor_property("category"), [])
    variants = []
    for path in paths:
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if not mesh:
            raise RuntimeError(f"Known internal CC0 mesh is missing: {path}")
        variant = unreal.DiscGolfEnvironmentMeshVariant()
        variant.set_editor_property("visual_mesh", mesh)
        variant.set_editor_property("weight", 1.0)
        variant.set_editor_property("uniform_scale_range", unreal.Vector2D(0.88, 1.12))
        variant.set_editor_property("nanite_suitable", slot.get_editor_property("category") == category.ROCK_LARGE)
        variants.append(variant)
    if variants:
        slot.set_editor_property("variants", variants)
        initialized_slot_count += 1
    slots[index] = slot
asset_set.set_editor_property("slots", slots)

graph = ensure_asset(GRAPH_PATH, unreal.PCGGraph)

# Rebuild the starter graph deterministically. Each category is a complete data-driven
# branch: sampled ground -> authored zone density/exclusion -> density filter -> HISM.
for existing_node in list(graph.get_editor_property("nodes")):
    graph.remove_node(existing_node)

world_node, world_settings = graph.add_node_of_type(unreal.PCGWorldRayHitSettings)
world_node.set_node_position(-1100, 0)
world_node.set_editor_property("node_title", "Collision Ground Surface")
query = world_settings.get_editor_property("query_params")
query.set_editor_property("override_default_params", False)
query.set_editor_property("select_landscape_hits", unreal.PCGWorldQuerySelectLandscapeHits.INCLUDE)
world_settings.set_editor_property("query_params", query)

sampler_node, sampler_settings = graph.add_node_of_type(unreal.PCGSurfaceSamplerSettings)
sampler_node.set_node_position(-820, 0)
sampler_node.set_editor_property("node_title", "Course-scale Surface Points")
sampler_settings.set_editor_property("points_per_squared_meter", 0.08)
sampler_settings.set_editor_property("point_extents", unreal.Vector(150.0, 150.0, 35.0))
sampler_settings.set_editor_property("looseness", 1.0)
sampler_settings.set_editor_property("apply_density_to_points", False)
graph.add_edge(world_node, "Out", sampler_node, "Surface")

categories = [
    category.TREE_CONIFER_LARGE,
    category.TREE_CONIFER_MEDIUM,
    category.TREE_CONIFER_YOUNG,
    category.TREE_DECIDUOUS_LARGE,
    category.TREE_DECIDUOUS_MEDIUM,
    category.SAPLING,
    category.SHRUB,
    category.FERN,
    category.GRASS,
    category.GROUND_COVER,
    category.LOG,
    category.STUMP,
    category.ROCK_SMALL,
    category.ROCK_LARGE,
    category.FOREST_DEBRIS,
    category.LEAF_LITTER,
]

for row, asset_category in enumerate(categories):
    y = row * 210 - 1500
    source_slot = next(slot for slot in slots if slot.get_editor_property("category") == asset_category)
    density_node, density_settings = graph.add_node_of_type(unreal.PCGDiscGolfZoneDensitySettings)
    density_node.set_node_position(-500, y)
    density_node.set_editor_property("node_title", str(asset_category))
    density_settings.set_editor_property("asset_category", asset_category)
    density_settings.set_editor_property("write_mesh_attribute", True)
    density_settings.set_editor_property("mesh_attribute_name", "DiscGolfMesh")

    filter_node, filter_settings = graph.add_node_of_type(unreal.PCGDensityFilterSettings)
    filter_node.set_node_position(-170, y)
    filter_settings.set_editor_property("lower_bound", 0.01)
    filter_settings.set_editor_property("upper_bound", 1.0)

    prune_node, prune_settings = graph.add_node_of_type(unreal.PCGSelfPruningSettings)
    prune_node.set_node_position(120, y)
    pruning = prune_settings.get_editor_property("parameters")
    pruning.set_editor_property("pruning_type", unreal.PCGSelfPruningType.ALL_EQUAL)
    pruning.set_editor_property("randomized_pruning", True)
    prune_settings.set_editor_property("parameters", pruning)

    spawner_node, spawner_settings = graph.add_node_of_type(unreal.PCGStaticMeshSpawnerSettings)
    spawner_node.set_node_position(430, y)
    spawner_settings.set_mesh_selector_type(unreal.PCGMeshSelectorByAttribute)
    selector = spawner_settings.get_editor_property("mesh_selector_parameters")
    selector.set_editor_property("attribute_name", "DiscGolfMesh")
    descriptor = selector.get_editor_property("template_descriptor")
    descriptor.set_editor_property("component_class", unreal.HierarchicalInstancedStaticMeshComponent)
    descriptor.set_editor_property("generate_overlap_events", False)
    descriptor.set_editor_property("can_ever_affect_navigation", False)
    descriptor.set_editor_property("use_default_collision", False)
    descriptor.set_editor_property("instance_start_cull_distance", int(source_slot.get_editor_property("cull_start_cm")))
    descriptor.set_editor_property("instance_end_cull_distance", int(source_slot.get_editor_property("cull_end_cm")))
    descriptor.set_editor_property("world_position_offset_disable_distance", int(source_slot.get_editor_property("cull_start_cm")))
    casts_shadow = asset_category not in (
        category.FERN,
        category.GRASS,
        category.GROUND_COVER,
        category.FOREST_DEBRIS,
        category.LEAF_LITTER,
    )
    descriptor.set_editor_property("cast_shadow", casts_shadow)
    descriptor.set_editor_property("cast_dynamic_shadow", casts_shadow)
    body_instance = descriptor.get_editor_property("body_instance")
    body_instance.set_editor_property("collision_profile_name", "NoCollision")
    descriptor.set_editor_property("body_instance", body_instance)
    descriptor.set_editor_property("component_tags", ["Environment.PCG.Visual", str(asset_category)])
    selector.set_editor_property("template_descriptor", descriptor)
    spawner_settings.set_editor_property("synchronous_load", False)
    spawner_settings.set_editor_property("allow_merge_different_data_in_same_instanced_components", True)

    graph.add_edge(sampler_node, "Out", density_node, "In")
    graph.add_edge(density_node, "Out", filter_node, "In")
    graph.add_edge(filter_node, "Out", prune_node, "In")
    graph.add_edge(prune_node, "Out", spawner_node, "In")
    graph.add_edge(spawner_node, "Out", graph.get_output_node(), "Out")

    # Physical proxies are a second pass and remain completely separate from leaves/visual cards.
    collision_mode = source_slot.get_editor_property("collision_mode")
    collision_entries = []
    for source_variant in source_slot.get_editor_property("variants"):
        collision_mesh = source_variant.get_editor_property("collision_proxy_mesh")
        if collision_mesh:
            entry = unreal.PCGMeshSelectorWeightedEntry()
            entry.set_editor_property("weight", max(1, int(source_variant.get_editor_property("weight") * 100)))
            collision_descriptor = entry.get_editor_property("descriptor")
            collision_descriptor.set_editor_property("static_mesh", collision_mesh)
            collision_descriptor.set_editor_property("component_class", unreal.HierarchicalInstancedStaticMeshComponent)
            collision_descriptor.set_editor_property("use_default_collision", True)
            collision_descriptor.set_editor_property("generate_overlap_events", False)
            collision_descriptor.set_editor_property("can_ever_affect_navigation", False)
            collision_descriptor.set_editor_property("visible", False)
            woody_categories = {
                unreal.DiscGolfEnvironmentAssetCategory.TREE_CONIFER_LARGE,
                unreal.DiscGolfEnvironmentAssetCategory.TREE_CONIFER_MEDIUM,
                unreal.DiscGolfEnvironmentAssetCategory.TREE_CONIFER_YOUNG,
                unreal.DiscGolfEnvironmentAssetCategory.TREE_DECIDUOUS_LARGE,
                unreal.DiscGolfEnvironmentAssetCategory.TREE_DECIDUOUS_MEDIUM,
                unreal.DiscGolfEnvironmentAssetCategory.LOG,
                unreal.DiscGolfEnvironmentAssetCategory.STUMP,
            }
            collision_tag = "Environment.Collision.Tree" if asset_category in woody_categories else "Environment.Collision.Rock"
            collision_descriptor.set_editor_property("component_tags", [collision_tag, str(asset_category)])
            entry.set_editor_property("descriptor", collision_descriptor)
            collision_entries.append(entry)

    if collision_entries:
        collision_spawner, collision_settings = graph.add_node_of_type(unreal.PCGStaticMeshSpawnerSettings)
        collision_spawner.set_node_position(730, y - 35)
        collision_spawner.set_editor_property("node_title", f"{asset_category} Physical Proxies")
        collision_settings.set_mesh_selector_type(unreal.PCGMeshSelectorWeighted)
        collision_selector = collision_settings.get_editor_property("mesh_selector_parameters")
        collision_selector.set_editor_property("mesh_entries", collision_entries)
        collision_settings.set_editor_property("synchronous_load", False)
        graph.add_edge(prune_node, "Out", collision_spawner, "In")
        graph.add_edge(collision_spawner, "Out", graph.get_output_node(), "Out")

    # Canopy/shrub proxy meshes are used as placement evidence for nonblocking drag volumes.
    # Spawn Actor setup is intentionally finished in the Editor after sizing each species proxy.
    has_interaction_proxy = any(
        bool(variant.get_editor_property("interaction_proxy_mesh"))
        for variant in source_slot.get_editor_property("variants")
    )
    if has_interaction_proxy:
        unreal.log_warning(
            f"INTERACTION_PROXY_EDITOR_STEP: {asset_category} has proxy meshes; add a Spawn Actor "
            "branch for DiscGolfVegetationInteractionActor after per-species extent review."
        )

mpc = ensure_asset(MPC_PATH, unreal.MaterialParameterCollection)

if len(mpc.get_editor_property("scalar_parameters")) == 0:
    scalar_parameters = []
    for name, default in (("WindSpeedMps", 2.8), ("GustStrengthMps", 1.4)):
        parameter = unreal.CollectionScalarParameter()
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", default)
        scalar_parameters.append(parameter)
    mpc.set_editor_property("scalar_parameters", scalar_parameters)

if len(mpc.get_editor_property("vector_parameters")) == 0:
    direction = unreal.CollectionVectorParameter()
    direction.set_editor_property("parameter_name", "WindDirection")
    direction.set_editor_property("default_value", unreal.LinearColor(0.9701, 0.2425, 0.0, 0.0))
    mpc.set_editor_property("vector_parameters", [direction])

preset = ensure_asset(PRESET_PATH, unreal.DiscGolfForestPreset, unreal.DataAssetFactory())
preset.set_editor_property("preset_id", "TemperateMountainForest")
preset.set_editor_property("asset_set", asset_set)
preset.set_editor_property("forest_graph", graph)
materials = preset.get_editor_property("materials")
materials.set_editor_property("forest_floor", unreal.EditorAssetLibrary.load_asset(
    "/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgeForestFloor"))
materials.set_editor_property("open_fairway", unreal.EditorAssetLibrary.load_asset(
    "/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgeFairway"))
materials.set_editor_property("creek_water", unreal.EditorAssetLibrary.load_asset(
    "/Game/Presentation/Course/PineRidge/Materials/M_GalleryLakeWater"))
preset.set_editor_property("materials", materials)
wind = preset.get_editor_property("wind")
wind.set_editor_property("foliage_wind_collection", mpc)
preset.set_editor_property("wind", wind)

for asset in (asset_set, graph, mpc, preset):
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

unreal.log(
    "PRODUCTION_ENVIRONMENT_ASSETS: SUCCESS "
    f"folders={len(FOLDERS)} slots={len(slots)} "
    f"initialized_slots={initialized_slot_count} retained_slots={retained_slot_count} "
    f"branches={len(categories)} graph_nodes={len(graph.get_editor_property('nodes'))} "
    f"preset={PRESET_PATH} graph={GRAPH_PATH}"
)
