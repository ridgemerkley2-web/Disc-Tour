"""Read-only UE 5.8 inspection for the MetaHuman-to-DGMaster retarget inputs.

Run with UnrealEditor-Cmd's PythonScript commandlet while the project is closed.
The script loads assets and reports their live controller contracts; it does not
create, modify, save, rename, or delete any asset.
"""

from __future__ import annotations

import json

import unreal


METAHUMAN_BODY = (
    "/Game/DiscGolf/Characters/MetaHuman/Generated/MHC_DG_Golfer_Default/"
    "Body/SKM_MHC_DG_Golfer_Default_BodyMesh."
    "SKM_MHC_DG_Golfer_Default_BodyMesh"
)
METAHUMAN_IK_RIG = (
    "/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig.IK_MH_IKRig"
)
DG_MASTER_MESH = "/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master"
DG_MASTER_IK_RIG = "/Game/DiscGolf/Rigs/IK_DG_Master.IK_DG_Master"


def require_asset(path: str):
    # Static loading also resolves enabled plugin content when this commandlet
    # intentionally runs without an Asset Registry cache.
    asset = unreal.load_asset(path)
    if asset is None or str(asset.get_path_name()) != path:
        raise RuntimeError(f"required exact asset did not load: {path}")
    return asset


def rig_record(path: str) -> dict:
    rig = require_asset(path)
    controller = unreal.IKRigController.get_controller(rig)
    if controller is None:
        raise RuntimeError(f"IK Rig controller unavailable: {path}")

    # FBoneChain is a reflected Unreal struct.  UE 5.8's direct Python
    # attribute lookup can return the struct proxy rather than the requested
    # member, so use get_editor_property for every reflected field.
    chain_rows = sorted(
        controller.get_retarget_chains(),
        key=lambda chain: str(chain.get_editor_property("chain_name")),
    )
    chains = []
    for chain in chain_rows:
        chain_name = str(chain.get_editor_property("chain_name"))
        chains.append(
            {
                "name": chain_name,
                "start_bone": str(
                    controller.get_retarget_chain_start_bone(chain_name)
                ),
                "end_bone": str(controller.get_retarget_chain_end_bone(chain_name)),
                "goal": str(chain.get_editor_property("ik_goal_name")),
            }
        )

    mesh = controller.get_skeletal_mesh()
    return {
        "path": path,
        "class": rig.get_class().get_name(),
        "preview_mesh": str(mesh.get_path_name()) if mesh else None,
        "retarget_root": str(controller.get_retarget_root()),
        "root_motion_bone": str(controller.get_root_motion_bone()),
        "solver_count": controller.get_num_solvers(),
        "chains": chains,
    }


def mesh_record(path: str) -> dict:
    mesh = require_asset(path)
    skeleton = mesh.get_editor_property("skeleton")
    return {
        "path": path,
        "class": mesh.get_class().get_name(),
        "skeleton": str(skeleton.get_path_name()) if skeleton else None,
    }


payload = {
    "status": "PASS_READ_ONLY_INPUT_INSPECTION",
    "mutation_policy": "NO_CREATE_NO_MODIFY_NO_SAVE_NO_RENAME_NO_DELETE",
    "metahuman_body": mesh_record(METAHUMAN_BODY),
    "metahuman_ik_rig": rig_record(METAHUMAN_IK_RIG),
    "dg_master_mesh": mesh_record(DG_MASTER_MESH),
    "dg_master_ik_rig": rig_record(DG_MASTER_IK_RIG),
}
print("DG_METAHUMAN_TO_MASTER_INPUTS=" + json.dumps(payload, sort_keys=True))
