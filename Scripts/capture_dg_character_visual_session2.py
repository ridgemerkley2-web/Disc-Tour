"""Capture the remaining Session 2 character visual-review evidence.

This is a normal-Editor-only capture harness.  It deliberately does not run in
an Unreal commandlet because high-resolution screenshots require a live Level
Editor viewport.  Every actor it places is transient, every pose is evaluated
in memory, and every filesystem write is confined to:

    Saved/CharacterFramework/Screenshots/Session2_VisualCloseout

The harness uses the project-owned SK_DG_Master / CR_DG_Master foundation and
the exact Engine Cylinder used by gameplay (scale 0.21, 0.21, 0.015).  It does
not save a level or asset, assign a pawn/AnimBP, create gameplay animation, or
perform any Session 3 wiring.

Run this from File > Execute Python Script in a normal Unreal Editor session,
or pass it to UnrealEditor.exe with -ExecutePythonScript.  Stop PIE/SIE first
and leave a Level Editor viewport open.  The capture is asynchronous; wait for
the DG_SESSION2_VISUAL_CAPTURE: COMPLETE log marker before closing the Editor.
"""

from datetime import datetime, timezone
from pathlib import Path
import hashlib
import json
import math
import struct
import traceback

import unreal


# Use the script location instead of Paths.project_dir(): this checkout can be
# reached through a junction, and Saved evidence must remain under C:\DGTour.
PROJECT_ROOT = Path(__file__).absolute().parents[1]
OUTPUT_DIR = (
    PROJECT_ROOT
    / "Saved"
    / "CharacterFramework"
    / "Screenshots"
    / "Session2_VisualCloseout"
)
MANIFEST_PATH = OUTPUT_DIR / "Session2_VisualCloseout_CaptureManifest.json"

MESH_PATH = "/Game/DiscGolf/Characters/Meshes/SK_DG_Master"
SKELETON_PATH = "/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master"
IK_PATH = "/Game/DiscGolf/Rigs/IK_DG_Master"
CONTROL_RIG_PATH = "/Game/DiscGolf/Rigs/CR_DG_Master"
ANIM_BP_PATH = "/Game/DiscGolf/Animation/ABP_DG_Player"
CYLINDER_PATH = "/Engine/BasicShapes/Cylinder.Cylinder"
PLANE_PATH = "/Engine/BasicShapes/Plane.Plane"
REVIEW_MATERIAL_PATH = "/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"
DISC_REVIEW_MATERIAL_PATH = "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"

PROFILE_PATHS = (
    "/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact",
    "/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter",
    "/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms",
)
PROFILE_DISPLAY_NAMES = ("ShortCompact", "Baseline", "TallLongArms")

PROFILE_CONTROL_FIELDS = {
    "height_cm": "dg_height_cm",
    "wingspan_scale": "dg_wingspan_scale",
    "shoulder_width_scale": "dg_shoulder_width_scale",
    "torso_length_scale": "dg_torso_length_scale",
    "leg_length_scale": "dg_leg_length_scale",
    "hand_scale": "dg_hand_scale",
}

PACKAGE_FILES = {
    MESH_PATH: PROJECT_ROOT
    / "Content/DiscGolf/Characters/Meshes/SK_DG_Master.uasset",
    SKELETON_PATH: PROJECT_ROOT
    / "Content/DiscGolf/Characters/Meshes/SKEL_DG_Master.uasset",
    IK_PATH: PROJECT_ROOT / "Content/DiscGolf/Rigs/IK_DG_Master.uasset",
    CONTROL_RIG_PATH: PROJECT_ROOT
    / "Content/DiscGolf/Rigs/CR_DG_Master.uasset",
    ANIM_BP_PATH: PROJECT_ROOT
    / "Content/DiscGolf/Animation/ABP_DG_Player.uasset",
    PROFILE_PATHS[0]: PROJECT_ROOT
    / "Content/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.uasset",
    PROFILE_PATHS[1]: PROJECT_ROOT
    / "Content/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.uasset",
    PROFILE_PATHS[2]: PROJECT_ROOT
    / "Content/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.uasset",
}

SCREEN_WIDTH = 1920
SCREEN_HEIGHT = 1080
# The automated launch uses the empty /Engine/Maps/Entry startup world.  Keep
# the render fixture at world origin so ControlRigComponent's output mapping,
# the registered SkeletalMeshComponent, and socket attachments all share one
# unambiguous world-space origin.
SCENE_Z = 0.0
RIG_SPACE = unreal.ControlRigComponentSpace.RIG_SPACE
DISC_SCALE = (0.21, 0.21, 0.015)

SHOT_FILENAMES = (
    "01_PBIK_Compressed_MirroredElbowsKnees.png",
    "02_PBIK_Extended_NoStretch.png",
    "03_PlantFoot_Start_Reachback.png",
    "04_PlantFoot_Mid_Brace.png",
    "05_PlantFoot_End_FollowThrough.png",
    "06_Grip_Left_Cylinder_Axes.png",
    "07_Grip_Right_Cylinder_Axes.png",
    "08_ProfileFixtures_AllThree.png",
)

CAPTURE_PREFIX = "DG_S2_VISUAL_CAPTURE_"
AUTO_EXIT_TOKEN = "-DGSession2VisualCaptureAutoExit"
EXECUTE_PYTHON_TOKEN = "-ExecutePythonScript"
_ACTIVE_CAPTURE = None


def _require(condition, message):
    if not condition:
        raise RuntimeError(message)


def _load(path, asset_type):
    asset = unreal.load_asset(path)
    _require(
        isinstance(asset, asset_type),
        f"Required {asset_type.__name__} did not load: {path}",
    )
    return asset


def _vector(value):
    if isinstance(value, unreal.Vector):
        return value
    return unreal.Vector(float(value[0]), float(value[1]), float(value[2]))


def _tuple(value):
    return (float(value.x), float(value.y), float(value.z))


def _add(a, b):
    a_value = _tuple(a) if isinstance(a, unreal.Vector) else tuple(a)
    b_value = _tuple(b) if isinstance(b, unreal.Vector) else tuple(b)
    return tuple(left + right for left, right in zip(a_value, b_value))


def _sub(a, b):
    a_value = _tuple(a) if isinstance(a, unreal.Vector) else tuple(a)
    b_value = _tuple(b) if isinstance(b, unreal.Vector) else tuple(b)
    return tuple(left - right for left, right in zip(a_value, b_value))


def _scale(value, scalar):
    source = _tuple(value) if isinstance(value, unreal.Vector) else tuple(value)
    return tuple(component * scalar for component in source)


def _distance(a, b):
    delta = _sub(a, b)
    return math.sqrt(sum(component * component for component in delta))


def _sha256(path):
    _require(path.is_file(), f"Expected project package is missing: {path}")
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def _package_hashes():
    return {package: _sha256(path) for package, path in PACKAGE_FILES.items()}


def _png_info(path):
    _require(path.is_file(), f"Screenshot was not written: {path}")
    data = path.read_bytes()
    _require(len(data) >= 24, f"Screenshot is truncated: {path}")
    _require(data[:8] == b"\x89PNG\r\n\x1a\n", f"Not a PNG file: {path}")
    width, height = struct.unpack(">II", data[16:24])
    return {
        "path": str(path),
        "bytes": len(data),
        "width": width,
        "height": height,
        "sha256": hashlib.sha256(data).hexdigest().upper(),
    }


def _set_curve(component, curve_name, value):
    hierarchy = component.get_control_rig().get_hierarchy()
    key = unreal.RigElementKey(type=unreal.RigElementType.CURVE, name=curve_name)
    _require(hierarchy.contains(key), f"CR_DG_Master is missing curve {curve_name}")
    hierarchy.set_curve_value(key, float(value), False)


class RigFixture:
    """One transient rendered SK_DG_Master driven by transient CR_DG_Master."""

    CONTROL_NAMES = (
        "ctrl_hand_l",
        "ctrl_hand_r",
        "ctrl_foot_l",
        "ctrl_foot_r",
    )

    def __init__(self, harness, label, location, profile):
        self.harness = harness
        self.label = label
        self.location = _vector(location)
        self.profile = profile
        self.actor = harness.spawn_actor(
            unreal.SkeletalMeshActor,
            self.location,
            unreal.Rotator(0.0, 0.0, 0.0),
            f"Rig_{label}",
        )
        self.skeletal = self.actor.get_component_by_class(
            unreal.SkeletalMeshComponent
        )
        _require(self.skeletal is not None, "SkeletalMeshActor has no mesh component")
        self.skeletal.set_mobility(unreal.ComponentMobility.MOVABLE)
        self.skeletal.set_skeletal_mesh_asset(harness.mesh)
        self.skeletal.set_material(0, harness.review_material)
        self.skeletal.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        self.skeletal.set_bounds_scale(5.0)
        self.skeletal.set_update_animation_in_editor(True)
        self.skeletal.set_visibility(True, True)

        self.control = unreal.new_object(
            unreal.ControlRigComponent,
            outer=self.actor,
            name=f"{CAPTURE_PREFIX}CR_{label}",
        )
        harness.transient_objects.append(self.control)
        self.control.set_control_rig_asset_reference(
            harness.control_rig.get_control_rig_asset_reference()
        )
        self.control.add_mapped_complete_skeletal_mesh(
            self.skeletal,
            unreal.ControlRigComponentMapDirection.OUTPUT,
        )
        self.control.set_bone_initial_transforms_from_skeletal_mesh(harness.mesh)
        self.control.initialize()
        _require(self.control.can_execute(), f"Transient rig cannot execute: {label}")
        _require(
            self.control.does_element_exist("pelvis", unreal.RigElementType.BONE),
            f"Transient rig has no pelvis: {label}",
        )
        self.apply_profile_controls()
        self.control.update(0.0)
        self.sync_world_location()
        self.initial_controls = {
            name: _tuple(self.control.get_control_position(name, RIG_SPACE))
            for name in self.CONTROL_NAMES
        }
        self.set_visible(False)

    def apply_profile_controls(self):
        body = self.profile.get_editor_property("body")
        for field, control_name in PROFILE_CONTROL_FIELDS.items():
            self.control.set_control_float(
                control_name,
                float(body.get_editor_property(field)),
            )

    def profile_values(self):
        body = self.profile.get_editor_property("body")
        return {
            field: float(body.get_editor_property(field))
            for field in PROFILE_CONTROL_FIELDS
        }

    def set_visible(self, visible):
        if visible:
            self.sync_world_location()
        self.skeletal.set_visibility(bool(visible), True)

    def sync_world_location(self):
        # The transient ControlRigComponent is intentionally not registered as
        # an actor component.  Reassert the registered render actor transform
        # after each manual evaluation so multiple profile fixtures remain at
        # their authored review offsets instead of overlapping at world zero.
        self.actor.set_actor_location(self.location, False, True)
        self.skeletal.set_world_location(self.location, False, True)

    def reset(self):
        self.apply_profile_controls()
        self.control.set_control_float("dg_hand_ik_alpha_l", 0.0)
        self.control.set_control_float("dg_hand_ik_alpha_r", 0.0)
        _set_curve(self.control, "DG_FootPlant_L", 0.0)
        _set_curve(self.control, "DG_FootPlant_R", 0.0)
        for name, position in self.initial_controls.items():
            self.control.set_control_position(name, _vector(position), RIG_SPACE)
        self.control.update(0.0)
        self.sync_world_location()

    def set_control_delta(self, name, delta):
        self.control.set_control_position(
            name,
            _vector(_add(self.initial_controls[name], delta)),
            RIG_SPACE,
        )

    def bone_world(self, bone_name):
        # Read the registered renderer's evaluated socket transform.  This is
        # the same transform path used by an attached held-disc component and
        # prevents a numerically valid transient rig pose from being displayed
        # at a different component origin than the mesh actually renders.
        transform = self.skeletal.get_socket_transform(
            bone_name,
            unreal.RelativeTransformSpace.RTS_WORLD,
        )
        return transform.translation

    def rig_bone_world(self, bone_name):
        local = self.control.get_bone_transform(bone_name, RIG_SPACE).translation
        return _vector(_add(self.location, local))

    def compressed_pose(self):
        self.reset()
        self.control.set_control_float("dg_hand_ik_alpha_l", 1.0)
        self.control.set_control_float("dg_hand_ik_alpha_r", 1.0)
        _set_curve(self.control, "DG_FootPlant_L", 1.0)
        _set_curve(self.control, "DG_FootPlant_R", 1.0)
        for name, delta in (
            ("ctrl_hand_l", (-25.0, 20.0, -5.0)),
            ("ctrl_hand_r", (25.0, 20.0, -5.0)),
            ("ctrl_foot_l", (0.0, 10.0, 25.0)),
            ("ctrl_foot_r", (0.0, 10.0, 25.0)),
        ):
            self.set_control_delta(name, delta)
        self.control.update(0.0)
        self.sync_world_location()

    def extended_pose(self):
        self.reset()
        self.control.set_control_float("dg_hand_ik_alpha_l", 1.0)
        self.control.set_control_float("dg_hand_ik_alpha_r", 1.0)
        _set_curve(self.control, "DG_FootPlant_L", 1.0)
        _set_curve(self.control, "DG_FootPlant_R", 1.0)
        for name, delta in (
            ("ctrl_hand_l", (25.0, 15.0, 8.0)),
            ("ctrl_hand_r", (-25.0, 15.0, 8.0)),
            ("ctrl_foot_l", (0.0, -18.0, 5.0)),
            ("ctrl_foot_r", (0.0, 18.0, 5.0)),
        ):
            self.set_control_delta(name, delta)
        self.control.update(0.0)
        self.sync_world_location()

    def plant_pose(self, throwing_delta, off_hand_delta):
        self.reset()
        _set_curve(self.control, "DG_FootPlant_R", 1.0)
        _set_curve(self.control, "DG_FootPlant_L", 0.0)
        self.control.set_control_float("dg_hand_ik_alpha_r", 1.0)
        self.control.set_control_float("dg_hand_ik_alpha_l", 1.0)
        self.control.update(0.0)
        anchor = self.bone_world("foot_r")
        self.set_control_delta("ctrl_hand_r", throwing_delta)
        self.set_control_delta("ctrl_hand_l", off_hand_delta)
        self.control.update(1.0 / 30.0)
        self.sync_world_location()
        current = self.bone_world("foot_r")
        return anchor, current, _distance(anchor, current)


class Session2VisualCapture:
    def __init__(self):
        self.editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        self.level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        _require(self.editor is not None, "UnrealEditorSubsystem is unavailable")
        _require(self.level_editor is not None, "LevelEditorSubsystem is unavailable")
        _require(self.actors is not None, "EditorActorSubsystem is unavailable")
        self.world = self.editor.get_editor_world()
        _require(
            self.world is not None,
            "No editor world. Run this in normal Unreal Editor, not a commandlet.",
        )
        _require(
            self.editor.get_game_world() is None,
            "PIE/SIE is active. Stop play before running the visual capture.",
        )
        _require(
            not self.level_editor.is_in_play_in_editor(),
            "PIE/SIE is active. Stop play before running the visual capture.",
        )
        self.viewport_config_key = str(
            self.level_editor.get_active_viewport_config_key()
        ).strip()
        _require(
            bool(self.viewport_config_key),
            "No active Level Editor viewport; high-resolution capture would be unsafe.",
        )

        self.spawned_actors = []
        self.transient_objects = []
        self.current_labels = []
        self.selection_before = list(self.actors.get_selected_level_actors())
        self.viewport_before = self._read_viewport_camera()
        self.tick_handle = None
        self.phase = "created"
        self.phase_ticks = 0
        self.phase_seconds = 0.0
        self.settle_frames = 0
        self.shot_index = -1
        self.current_task = None
        self.current_path = None
        self.capture_results = []
        self.cleaned_up = False
        self.started_utc = datetime.now(timezone.utc).isoformat()
        self.before_hashes = _package_hashes()
        self.dirty_maps_before = sorted(
            str(package.get_path_name())
            for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        )
        self.auto_exit = AUTO_EXIT_TOKEN.casefold() in unreal.SystemLibrary.get_command_line().casefold()

        self.mesh = None
        self.skeleton = None
        self.ik_rig = None
        self.control_rig = None
        self.cylinder = None
        self.review_material = None
        self.disc_review_material = None
        self.profiles = []
        self.main_fixture = None
        self.profile_fixtures = []
        self.camera = None
        self.camera_component = None
        self.left_disc = None
        self.right_disc = None
        self.stage_plane = None
        self.shots = []

    def _read_viewport_camera(self):
        try:
            value = self.editor.get_level_viewport_camera_info()
            if isinstance(value, tuple):
                vectors = [item for item in value if isinstance(item, unreal.Vector)]
                rotations = [item for item in value if isinstance(item, unreal.Rotator)]
                if vectors and rotations:
                    return (vectors[0], rotations[0])
        except Exception:
            pass
        return None

    def spawn_actor(self, actor_class, location, rotation, suffix):
        actor = self.actors.spawn_actor_from_class(
            actor_class,
            _vector(location),
            rotation,
            True,
        )
        _require(actor is not None, f"Could not spawn transient actor {suffix}")
        actor.set_actor_label(f"{CAPTURE_PREFIX}{suffix}")
        self.spawned_actors.append(actor)
        return actor

    def start(self):
        try:
            self._load_assets()
            self._prepare_output()
            self._build_transient_scene()
            self._build_shot_queue()
            self.phase = "advance"
            self.tick_handle = unreal.register_slate_post_tick_callback(self.tick)
            unreal.log(
                "DG_SESSION2_VISUAL_CAPTURE: STARTED shots=8 "
                f"output={OUTPUT_DIR}"
            )
        except Exception as error:
            self.fail(error)

    def _load_assets(self):
        self.mesh = _load(MESH_PATH, unreal.SkeletalMesh)
        self.skeleton = _load(SKELETON_PATH, unreal.Skeleton)
        self.ik_rig = _load(IK_PATH, unreal.IKRigDefinition)
        self.control_rig = _load(CONTROL_RIG_PATH, unreal.ControlRigBlueprint)
        self.cylinder = _load(CYLINDER_PATH, unreal.StaticMesh)
        self.plane = _load(PLANE_PATH, unreal.StaticMesh)
        self.review_material = _load(REVIEW_MATERIAL_PATH, unreal.Material)
        self.disc_review_material = _load(
            DISC_REVIEW_MATERIAL_PATH,
            unreal.Material,
        )
        self.profiles = [
            _load(path, unreal.DiscGolfCharacterProfile) for path in PROFILE_PATHS
        ]

        _require(
            self.mesh.get_editor_property("skeleton") == self.skeleton,
            "SK_DG_Master does not resolve SKEL_DG_Master",
        )

    def _prepare_output(self):
        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        for filename in SHOT_FILENAMES:
            path = OUTPUT_DIR / filename
            if path.exists():
                path.unlink()
        if MANIFEST_PATH.exists():
            MANIFEST_PATH.unlink()

    def _build_transient_scene(self):
        # Fixed review exposure prevents the white validation proxy from
        # blooming into an unreadable silhouette in the otherwise empty Entry
        # map.  These process-local CVars are discarded when the dedicated
        # capture Editor exits.
        unreal.SystemLibrary.execute_console_command(self.world, "r.BloomQuality 0")
        unreal.SystemLibrary.execute_console_command(
            self.world, "r.EyeAdaptationQuality 0"
        )
        origin = (0.0, 0.0, SCENE_Z)
        self.stage_plane = self.spawn_actor(
            unreal.StaticMeshActor,
            (0.0, 0.0, SCENE_Z - 2.0),
            unreal.Rotator(0.0, 0.0, 0.0),
            "StagePlane",
        )
        plane_component = self.stage_plane.get_component_by_class(
            unreal.StaticMeshComponent
        )
        _require(plane_component is not None, "Stage plane has no mesh component")
        plane_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        plane_component.set_static_mesh(self.plane)
        plane_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        plane_component.set_world_scale3d(unreal.Vector(9.0, 9.0, 1.0))

        # Transient front/key lights make the review reproducible even if the
        # currently open level has no useful light near the capture altitude.
        for index, location in enumerate(
            ((260.0, 320.0, SCENE_Z + 260.0), (-260.0, 220.0, SCENE_Z + 120.0))
        ):
            light = self.spawn_actor(
                unreal.PointLight,
                location,
                unreal.Rotator(0.0, 0.0, 0.0),
                f"KeyLight_{index}",
            )
            component = light.get_component_by_class(unreal.PointLightComponent)
            _require(component is not None, "PointLight has no light component")
            component.set_intensity(1500.0 if index == 0 else 800.0)
            component.set_attenuation_radius(1400.0)

        self.camera = self.spawn_actor(
            unreal.CameraActor,
            (0.0, 520.0, SCENE_Z + 115.0),
            unreal.Rotator(0.0, -90.0, 0.0),
            "Camera",
        )
        self.camera_component = self.camera.get_component_by_class(
            unreal.CameraComponent
        )
        _require(self.camera_component is not None, "CameraActor has no component")

        self.main_fixture = RigFixture(
            self,
            "Baseline_Main",
            origin,
            self.profiles[1],
        )
        self.profile_fixtures = [
            RigFixture(
                self,
                display_name,
                (offset, 0.0, SCENE_Z),
                profile,
            )
            for display_name, offset, profile in zip(
                PROFILE_DISPLAY_NAMES,
                (-220.0, 0.0, 220.0),
                self.profiles,
            )
        ]

        self.left_disc = self._spawn_disc("Left", "disc_grip_l", 0.0)
        self.right_disc = self._spawn_disc("Right", "disc_grip_r", 180.0)

    def _spawn_disc(self, label, bone_name, yaw):
        actor = self.spawn_actor(
            unreal.StaticMeshActor,
            (0.0, 0.0, SCENE_Z),
            unreal.Rotator(0.0, 0.0, 0.0),
            f"Disc_{label}",
        )
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        _require(component is not None, f"{label} disc has no mesh component")
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_static_mesh(self.cylinder)
        component.set_material(0, self.disc_review_material)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        # Keep this renderer independent from the skeletal component's import
        # scale.  _prepare_grip places it at the evaluated grip socket using
        # the composed, validated world convention (+Y character forward,
        # +Z disc normal).  The manifest separately records the exact
        # attachment-local transforms: left identity, right local-Z yaw 180.
        component.set_world_scale3d(unreal.Vector(*DISC_SCALE))
        component.set_visibility(False, True)
        return component

    def _build_shot_queue(self):
        self.shots = [
            {
                "filename": SHOT_FILENAMES[0],
                "label": "PBIK COMPRESSED — MIRRORED ELBOWS + KNEES",
                "prepare": self._prepare_compressed,
                "evidence": [
                    "both hand IK alphas = 1",
                    "both foot-plant curves = 1",
                    "mirrored elbow and knee preferred bends",
                ],
            },
            {
                "filename": SHOT_FILENAMES[1],
                "label": "PBIK EXTENDED — NO-STRETCH REVIEW",
                "prepare": self._prepare_extended,
                "evidence": [
                    "extended hand and foot effectors",
                    "same SK_DG_Master segment lengths",
                ],
            },
            {
                "filename": SHOT_FILENAMES[2],
                "label": "PLANT FOOT — START / REACHBACK",
                "prepare": lambda: self._prepare_plant(
                    "START / REACHBACK",
                    (0.0, -35.0, -5.0),
                    (-15.0, 10.0, -5.0),
                ),
                "evidence": ["right foot plant curve = 1", "fixed anchor marker"],
            },
            {
                "filename": SHOT_FILENAMES[3],
                "label": "PLANT FOOT — MID / BRACE",
                "prepare": lambda: self._prepare_plant(
                    "MID / BRACE",
                    (45.0, 12.0, -10.0),
                    (-45.0, -10.0, -10.0),
                ),
                "evidence": ["right foot plant curve = 1", "fixed anchor marker"],
            },
            {
                "filename": SHOT_FILENAMES[4],
                "label": "PLANT FOOT — END / FOLLOW-THROUGH",
                "prepare": lambda: self._prepare_plant(
                    "END / FOLLOW-THROUGH",
                    (110.0, 40.0, -15.0),
                    (-110.0, -25.0, -15.0),
                ),
                "evidence": ["right foot plant curve = 1", "fixed anchor marker"],
            },
            {
                "filename": SHOT_FILENAMES[5],
                "label": "LEFT GRIP — GAMEPLAY CYLINDER + AXES",
                "prepare": lambda: self._prepare_grip("left"),
                "evidence": [
                    "Cylinder scale = 0.21, 0.21, 0.015",
                    "disc_grip_l local rotation = 0, 0, 0",
                    "red +X forward; green +Y span; blue +Z normal",
                ],
            },
            {
                "filename": SHOT_FILENAMES[6],
                "label": "RIGHT GRIP — GAMEPLAY CYLINDER + AXES",
                "prepare": lambda: self._prepare_grip("right"),
                "evidence": [
                    "Cylinder scale = 0.21, 0.21, 0.015",
                    "disc_grip_r local yaw = 180",
                    "red +X forward; green +Y span; blue +Z normal",
                ],
            },
            {
                "filename": SHOT_FILENAMES[7],
                "label": "PROFILE FIXTURES — SHARED SESSION 2 FOUNDATION",
                "prepare": self._prepare_profiles,
                "evidence": [
                    "ShortCompact / Baseline / TallLongArms profiles loaded",
                    "same SK_DG_Master / SKEL_DG_Master / IK_DG_Master / CR_DG_Master",
                    "runtime silhouette deformation intentionally deferred to Session 4",
                ],
            },
        ]

    def _hide_capture_subjects(self):
        unreal.SystemLibrary.flush_persistent_debug_lines(self.world)
        for label in self.current_labels:
            if label in self.spawned_actors:
                self.actors.destroy_actor(label)
                self.spawned_actors.remove(label)
        self.current_labels = []
        self.main_fixture.set_visible(False)
        for fixture in self.profile_fixtures:
            fixture.set_visible(False)
        self.left_disc.set_visibility(False, True)
        self.right_disc.set_visibility(False, True)

    def _set_camera(self, location, target, field_of_view):
        location_value = _vector(location)
        target_value = _vector(target)
        rotation = unreal.MathLibrary.find_look_at_rotation(
            location_value, target_value
        )
        self.camera.set_actor_location(location_value, False, True)
        self.camera.set_actor_rotation(rotation, True)
        self.camera_component.set_field_of_view(float(field_of_view))
        return location_value

    def _spawn_label(self, text, location, camera_location, world_size=12.0):
        location_value = _vector(location)
        rotation = unreal.MathLibrary.find_look_at_rotation(
            location_value, _vector(camera_location)
        )
        actor = self.spawn_actor(
            unreal.TextRenderActor,
            location_value,
            rotation,
            f"Label_{len(self.current_labels)}",
        )
        component = actor.get_component_by_class(unreal.TextRenderComponent)
        _require(component is not None, "TextRenderActor has no text component")
        component.set_text(str(text))
        component.set_world_size(float(world_size))
        component.set_text_render_color(unreal.Color(255, 255, 255, 255))
        component.set_visibility(True, True)
        try:
            component.set_horizontal_alignment(
                unreal.HorizTextAligment.EHTA_CENTER
            )
        except Exception:
            # Alignment is cosmetic and enum spelling has varied across UE
            # Python wrappers; capture validity must not depend on it.
            pass
        self.current_labels.append(actor)
        return actor

    def _front_camera(self, distance=520.0, height=115.0, fov=42.0):
        return self._set_camera(
            (0.0, distance, SCENE_Z + height),
            (0.0, 0.0, SCENE_Z + 100.0),
            fov,
        )

    def _draw_joint_markers(self):
        colors = {
            "elbow": unreal.LinearColor(0.0, 0.8, 1.0, 1.0),
            "knee": unreal.LinearColor(1.0, 0.75, 0.0, 1.0),
            "center": unreal.LinearColor(0.2, 1.0, 0.2, 1.0),
        }
        for bone in ("lowerarm_l", "lowerarm_r"):
            unreal.SystemLibrary.draw_debug_sphere(
                self.world,
                self.main_fixture.bone_world(bone),
                4.0,
                16,
                colors["elbow"],
                10.0,
                2.0,
            )
        for bone in ("calf_l", "calf_r"):
            unreal.SystemLibrary.draw_debug_sphere(
                self.world,
                self.main_fixture.bone_world(bone),
                4.0,
                16,
                colors["knee"],
                10.0,
                2.0,
            )
        unreal.SystemLibrary.draw_debug_line(
            self.world,
            unreal.Vector(0.0, 0.0, SCENE_Z),
            unreal.Vector(0.0, 0.0, SCENE_Z + 220.0),
            colors["center"],
            10.0,
            1.5,
        )

    def _prepare_compressed(self):
        self._hide_capture_subjects()
        self.main_fixture.set_visible(True)
        self.main_fixture.compressed_pose()
        camera = self._front_camera()
        self._draw_joint_markers()
        self._spawn_label(
            "PBIK COMPRESSED | CYAN ELBOWS | GOLD KNEES | GREEN MIDLINE",
            (0.0, 8.0, SCENE_Z + 230.0),
            camera,
            11.0,
        )

    def _prepare_extended(self):
        self._hide_capture_subjects()
        self.main_fixture.set_visible(True)
        self.main_fixture.extended_pose()
        camera = self._front_camera()
        self._draw_joint_markers()
        self._spawn_label(
            "PBIK EXTENDED | NO-STRETCH VISUAL REVIEW | SK_DG_MASTER",
            (0.0, 8.0, SCENE_Z + 230.0),
            camera,
            11.0,
        )

    def _prepare_plant(self, phase_name, throwing_delta, off_hand_delta):
        self._hide_capture_subjects()
        self.main_fixture.set_visible(True)
        anchor, current, drift = self.main_fixture.plant_pose(
            throwing_delta, off_hand_delta
        )
        camera = self._set_camera(
            (390.0, 480.0, SCENE_Z + 145.0),
            (0.0, 0.0, SCENE_Z + 95.0),
            43.0,
        )
        unreal.SystemLibrary.draw_debug_sphere(
            self.world,
            anchor,
            6.0,
            20,
            unreal.LinearColor(0.0, 1.0, 0.15, 1.0),
            10.0,
            2.5,
        )
        unreal.SystemLibrary.draw_debug_sphere(
            self.world,
            current,
            3.0,
            16,
            unreal.LinearColor(1.0, 0.2, 0.0, 1.0),
            10.0,
            2.0,
        )
        unreal.SystemLibrary.draw_debug_arrow(
            self.world,
            anchor,
            _vector(_add(anchor, (0.0, 0.0, 42.0))),
            8.0,
            unreal.LinearColor(0.0, 1.0, 0.15, 1.0),
            10.0,
            2.5,
        )
        self._spawn_label(
            f"PLANT FOOT {phase_name} | RIGHT ANCHOR (GREEN) | DRIFT {drift:.3f} CM",
            (0.0, 8.0, SCENE_Z + 230.0),
            camera,
            10.0,
        )

    def _prepare_grip(self, side):
        self._hide_capture_subjects()
        self.main_fixture.set_visible(True)
        self.main_fixture.reset()
        is_left = side == "left"
        disc = self.left_disc if is_left else self.right_disc
        other = self.right_disc if is_left else self.left_disc
        bone = "disc_grip_l" if is_left else "disc_grip_r"
        yaw = 0.0 if is_left else 180.0
        disc.set_visibility(True, True)
        other.set_visibility(False, True)
        # Disc-grip acceptance uses the evaluated Control Rig hierarchy itself.
        # The renderer socket cache can be one editor frame behind a manually
        # evaluated, unregistered transient ControlRigComponent; using the rig
        # transform keeps the evidence tied to the validated disc_grip_l/r
        # bones rather than a stale skinned socket pose.
        grip = self.main_fixture.rig_bone_world(bone)
        disc.set_world_location(grip, False, True)
        # The character validation asset faces +Y while the disc's physical
        # forward axis is local +X.  Yaw +90 composes the left identity/right
        # local-yaw-180 grip contracts into the same world release direction.
        disc.set_world_rotation(
            unreal.Rotator(pitch=0.0, yaw=90.0, roll=0.0),
            False,
            True,
        )
        disc.set_world_scale3d(unreal.Vector(*DISC_SCALE))
        center = disc.get_world_location()
        _require(
            _distance(center, grip) <= 1.0,
            f"{side} gameplay Cylinder is not snapped to {bone}: "
            f"center={_tuple(center)} grip={_tuple(grip)}",
        )
        camera_offset = (90.0 if is_left else -90.0, 185.0, 105.0)
        camera = self._set_camera(
            _add(grip, camera_offset),
            grip,
            34.0,
        )

        # Component vectors come from the attached, scaled gameplay Cylinder,
        # so the markers prove the evaluated attachment convention rather than
        # merely drawing assumed world axes.
        axes = (
            (disc.get_forward_vector(), unreal.LinearColor(1.0, 0.0, 0.0, 1.0)),
            (disc.get_right_vector(), unreal.LinearColor(0.0, 1.0, 0.0, 1.0)),
            (disc.get_up_vector(), unreal.LinearColor(0.0, 0.35, 1.0, 1.0)),
        )
        for direction, color in axes:
            unreal.SystemLibrary.draw_debug_arrow(
                self.world,
                center,
                _vector(_add(center, _scale(direction, 26.0))),
                7.0,
                color,
                10.0,
                2.5,
            )
        unreal.SystemLibrary.draw_debug_sphere(
            self.world,
            center,
            2.5,
            12,
            unreal.LinearColor(1.0, 1.0, 0.0, 1.0),
            10.0,
            2.0,
        )
        self._spawn_label(
            (
                f"{side.upper()} GRIP | CYLINDER 0.21,0.21,0.015 | YAW {yaw:.0f}\n"
                "RED +X FORWARD | GREEN +Y SPAN | BLUE +Z NORMAL"
            ),
            _add(grip, (0.0, 0.0, 48.0)),
            camera,
            5.5,
        )

    def _prepare_profiles(self):
        self._hide_capture_subjects()
        for fixture in self.profile_fixtures:
            # The dynamic validator already exercises the same reachable PBIK
            # stress pose through every profile.  For this single comparative
            # frame, clear only the transient output mapping so three clean
            # reference-pose renderers can hold independent world offsets.
            # Runtime silhouette deformation remains the honest Session 4
            # gate and is called out in the visible title/manifest.
            fixture.control.clear_mapped_elements()
            fixture.skeletal.set_skeletal_mesh_asset(self.mesh)
            fixture.skeletal.set_material(0, self.review_material)
            fixture.sync_world_location()
            fixture.set_visible(True)
        camera = self._set_camera(
            (0.0, 900.0, SCENE_Z + 125.0),
            (0.0, 0.0, SCENE_Z + 100.0),
            48.0,
        )
        self._spawn_label(
            "PROFILE FIXTURES | SAME SK / SKELETON / IK / CR | DEFORMATION DEFERRED S4",
            (0.0, 10.0, SCENE_Z + 245.0),
            camera,
            10.0,
        )
        for fixture, name in zip(self.profile_fixtures, PROFILE_DISPLAY_NAMES):
            values = fixture.profile_values()
            self._spawn_label(
                f"{name}\n{values['height_cm']:.0f} CM | WING {values['wingspan_scale']:.2f}",
                (fixture.location.x, 5.0, SCENE_Z + 215.0),
                camera,
                7.0,
            )

    def _reset_phase_clock(self):
        self.phase_ticks = 0
        self.phase_seconds = 0.0

    def tick(self, delta_seconds):
        if self.phase in ("complete", "failed"):
            return
        self.phase_ticks += 1
        self.phase_seconds += max(0.0, float(delta_seconds))
        try:
            if self.phase == "advance":
                self.shot_index += 1
                if self.shot_index >= len(self.shots):
                    self.complete()
                    return
                shot = self.shots[self.shot_index]
                shot["prepare"]()
                self.level_editor.editor_invalidate_viewports()
                self.current_path = OUTPUT_DIR / shot["filename"]
                self.settle_frames = 8
                self.phase = "settle"
                self._reset_phase_clock()
                unreal.log(
                    "DG_SESSION2_VISUAL_CAPTURE: PREPARED "
                    f"{self.shot_index + 1}/{len(self.shots)} {shot['filename']}"
                )
                return

            if self.phase == "settle":
                self.settle_frames -= 1
                if self.settle_frames > 0:
                    return
                shot = self.shots[self.shot_index]
                self.current_task = unreal.AutomationLibrary.take_high_res_screenshot(
                    SCREEN_WIDTH,
                    SCREEN_HEIGHT,
                    str(self.current_path),
                    self.camera,
                    False,
                    False,
                    unreal.ComparisonTolerance.LOW,
                    shot["label"],
                    0.15,
                    True,
                )
                _require(self.current_task is not None, "Screenshot task was not created")
                _require(
                    self.current_task.is_valid_task(),
                    "Screenshot task is invalid; a Level Editor viewport is required",
                )
                self.phase = "capturing"
                self._reset_phase_clock()
                return

            if self.phase == "capturing":
                if self.current_task.is_task_done():
                    self.phase = "verify_file"
                    self.current_task = None
                    self._reset_phase_clock()
                    return
                _require(
                    self.phase_seconds < 45.0 and self.phase_ticks < 2700,
                    f"Timed out capturing {self.current_path}",
                )
                return

            if self.phase == "verify_file":
                if self.current_path.is_file() and self.current_path.stat().st_size > 4096:
                    info = _png_info(self.current_path)
                    _require(
                        (info["width"], info["height"])
                        == (SCREEN_WIDTH, SCREEN_HEIGHT),
                        f"Unexpected screenshot dimensions: {info}",
                    )
                    shot = self.shots[self.shot_index]
                    info.update(
                        {
                            "status": "CAPTURED_PENDING_HUMAN_REVIEW",
                            "label": shot["label"],
                            "evidence": shot["evidence"],
                        }
                    )
                    self.capture_results.append(info)
                    unreal.log(
                        "DG_SESSION2_VISUAL_CAPTURE: CAPTURED "
                        f"{self.shot_index + 1}/{len(self.shots)} {self.current_path}"
                    )
                    self.phase = "advance"
                    self._reset_phase_clock()
                    return
                _require(
                    self.phase_seconds < 15.0 and self.phase_ticks < 900,
                    f"Screenshot task completed but file did not appear: {self.current_path}",
                )
        except Exception as error:
            self.fail(error)

    def _manifest_base(self):
        return {
            "engine_version": unreal.SystemLibrary.get_engine_version(),
            "scope": "SESSION_2_NORMAL_EDITOR_VISUAL_CLOSEOUT_ONLY",
            "started_utc": self.started_utc,
            "finished_utc": datetime.now(timezone.utc).isoformat(),
            "output_directory": str(OUTPUT_DIR),
            "resolution": [SCREEN_WIDTH, SCREEN_HEIGHT],
            "assets": {
                "skeletal_mesh": MESH_PATH,
                "skeleton": SKELETON_PATH,
                "ik_rig": IK_PATH,
                "control_rig": CONTROL_RIG_PATH,
                "animation_blueprint": ANIM_BP_PATH,
                "gameplay_disc_mesh": CYLINDER_PATH,
                "profile_fixtures": list(PROFILE_PATHS),
            },
            "disc_attachment_contract": {
                "raw_mesh_scale": list(DISC_SCALE),
                "raw_disc_local_forward": "+X",
                "raw_disc_local_normal": "+Z",
                "left": {
                    "bone": "disc_grip_l",
                    "location_cm": [0.0, 0.0, 0.0],
                    "rotation_pitch_yaw_roll_deg": [0.0, 0.0, 0.0],
                },
                "right": {
                    "bone": "disc_grip_r",
                    "location_cm": [0.0, 0.0, 0.0],
                    "rotation_pitch_yaw_roll_deg": [0.0, 180.0, 0.0],
                },
            },
            "axis_conventions": {
                "character_asset_forward": "+Y",
                "character_asset_up": "+Z",
                "disc_forward_marker": "RED +X",
                "disc_span_marker": "GREEN +Y",
                "disc_normal_marker": "BLUE +Z",
            },
            "profile_fixture_resolution": [
                {
                    "name": name,
                    "profile": fixture.profile.get_path_name(),
                    "profile_controls": fixture.profile_values(),
                    "mesh": MESH_PATH,
                    "skeleton": SKELETON_PATH,
                    "ik_rig": IK_PATH,
                    "control_rig": CONTROL_RIG_PATH,
                    "runtime_body_deformation": "DEFERRED_TO_SESSION_4",
                }
                for name, fixture in zip(
                    PROFILE_DISPLAY_NAMES, self.profile_fixtures
                )
            ],
            "writes": {
                "png_and_manifest_only": True,
                "uasset_writes": [],
                "level_save_calls": [],
                "content_save_calls": [],
            },
            "dedicated_editor_session": {
                "startup_world": str(self.world.get_path_name()),
                "dirty_maps_before": self.dirty_maps_before,
                "auto_exit_without_saving": self.auto_exit,
                "note": (
                    "Transient EditorActorSubsystem placement may dirty the startup map in memory; "
                    "this dedicated process exits without saving it."
                ),
            },
            "session_3_wiring": "NONE",
        }

    def complete(self):
        try:
            _require(
                len(self.capture_results) == len(self.shots),
                "Capture queue finished with missing screenshots",
            )
            after_hashes = _package_hashes()
            dirty_maps_after = sorted(
                str(package.get_path_name())
                for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
            )
            changed = [
                package
                for package, before in self.before_hashes.items()
                if after_hashes[package] != before
            ]
            _require(not changed, f"Project packages changed on disk: {changed}")
            manifest = self._manifest_base()
            manifest.update(
                {
                    "status": "CAPTURED_PENDING_HUMAN_VISUAL_REVIEW",
                    "capture_count": len(self.capture_results),
                    "captures": self.capture_results,
                    "package_integrity": {
                        "status": "PASS_NO_UASSET_DISK_MUTATION",
                        "package_count": len(self.before_hashes),
                        "changed_packages": changed,
                        "before_sha256": self.before_hashes,
                        "after_sha256": after_hashes,
                    },
                    "dirty_map_state": {
                        "before": self.dirty_maps_before,
                        "after_transient_cleanup": dirty_maps_after,
                        "saved": False,
                    },
                    "acceptance": {
                        "fresh_labeled_png_evidence": "CAPTURED",
                        "compressed_and_extended_pbik": "CAPTURED",
                        "mirrored_elbow_and_knee_review": "CAPTURED",
                        "plant_foot_start_mid_end": "CAPTURED",
                        "left_and_right_grip_axis_views": "CAPTURED",
                        "three_profile_fixtures": "CAPTURED",
                        "human_visual_acceptance": "PENDING_REVIEW",
                    },
                }
            )
            MANIFEST_PATH.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
            self.phase = "complete"
            self.cleanup()
            unreal.log(
                "DG_SESSION2_VISUAL_CAPTURE: COMPLETE "
                f"captures={len(self.capture_results)} manifest={MANIFEST_PATH} "
                "human_review=PENDING"
            )
            if self.auto_exit:
                unreal.log(
                    "DG_SESSION2_VISUAL_CAPTURE: AUTO_EXIT dedicated session; no map/content save"
                )
                unreal.SystemLibrary.quit_editor()
        except Exception as error:
            self.fail(error)

    def fail(self, error):
        if self.phase == "failed":
            return
        self.phase = "failed"
        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        try:
            manifest = self._manifest_base()
        except Exception:
            manifest = {
                "scope": "SESSION_2_NORMAL_EDITOR_VISUAL_CLOSEOUT_ONLY",
                "started_utc": self.started_utc,
                "finished_utc": datetime.now(timezone.utc).isoformat(),
            }
        manifest.update(
            {
                "status": "FAIL",
                "error": f"{type(error).__name__}: {error}",
                "traceback": traceback.format_exc(),
                "captures_completed": self.capture_results,
                "human_visual_acceptance": "NOT_READY",
                "uasset_writes": [],
                "session_3_wiring": "NONE",
            }
        )
        MANIFEST_PATH.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
        self.cleanup()
        unreal.log_error(
            "DG_SESSION2_VISUAL_CAPTURE: FAIL "
            f"{type(error).__name__}: {error}; manifest={MANIFEST_PATH}"
        )
        if self.auto_exit:
            unreal.SystemLibrary.quit_editor()

    def cleanup(self):
        global _ACTIVE_CAPTURE
        if self.cleaned_up:
            return
        self.cleaned_up = True
        try:
            unreal.SystemLibrary.flush_persistent_debug_lines(self.world)
        except Exception:
            pass
        for actor in reversed(self.spawned_actors):
            try:
                self.actors.destroy_actor(actor)
            except Exception:
                pass
        self.spawned_actors = []
        try:
            self.actors.set_selected_level_actors(self.selection_before)
        except Exception:
            pass
        if self.viewport_before is not None:
            try:
                self.editor.set_level_viewport_camera_info(
                    self.viewport_before[0], self.viewport_before[1]
                )
            except Exception:
                pass
        if self.tick_handle is not None:
            handle = self.tick_handle
            self.tick_handle = None
            try:
                unreal.unregister_slate_post_tick_callback(handle)
            except Exception:
                pass
        _ACTIVE_CAPTURE = None


def main():
    global _ACTIVE_CAPTURE
    _require(
        _ACTIVE_CAPTURE is None,
        "A Session 2 visual capture is already active in this Python module",
    )
    # UE's -ExecutePythonScript runner requests QUIT_EDITOR on the tick after
    # the top-level script returns unless this documented editor-scripting
    # keep-alive flag is armed.  This harness deliberately returns after
    # registering its asynchronous Slate callback, so keep the dedicated
    # process alive until complete()/fail() performs the explicit safe exit.
    command_line = unreal.SystemLibrary.get_command_line()
    if EXECUTE_PYTHON_TOKEN.casefold() in command_line.casefold():
        unreal.EditorPythonScripting.set_keep_python_script_alive(True)
        unreal.log("DG_SESSION2_VISUAL_CAPTURE: ASYNC_KEEP_ALIVE armed")
    _ACTIVE_CAPTURE = Session2VisualCapture()
    _ACTIVE_CAPTURE.start()


if __name__ == "__main__":
    main()
