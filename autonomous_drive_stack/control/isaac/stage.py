"""
Shared Isaac Sim setup for the Mila simulation scripts.

IMPORT ORDER MATTERS. Every module in here comes from extensions that only exist
once `SimulationApp` has booted the Omniverse Kit runtime. So the entry-point
scripts construct SimulationApp first and import this module *after* — which is
why this file has no top-level isaacsim imports of its own and does them inside
the functions instead.

Targets Isaac Sim 6.0, which is what the team's stage references
(.../Assets/Isaac/6.0/... inside FirstMilaStage_light.usda). The namespace
changed at 4.5: anything you find on `omni.isaac.*` is older and will not import.
"""

from __future__ import annotations

import os
from typing import List, Optional, Tuple

REPO_SIM_DIR = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "simulation"))
MILA_STAGE = os.path.join(REPO_SIM_DIR, "FirstMilaStage_light.usda")
MILA_VEHICLE_USD = os.path.join(REPO_SIM_DIR, "assets", "buick",
                                "buick rivera-ANKA3DCITY_.usd")

# NVIDIA's Ackermann-steered sample. Carter and Jetbot are differential drive and
# cannot represent a steering angle at all, so they are the wrong fallback.
LEATHERBACK_SUBPATH = "/Isaac/Robots/Leatherback/leatherback.usd"


def check_lfs(path: str) -> Tuple[bool, str]:
    """Is this a real USD, or an un-pulled Git LFS pointer?

    The repo tracks *.usd through LFS (.gitattributes). A fresh clone without
    `git lfs pull` leaves ~130-byte text pointers in place of the assets, and
    Isaac's failure mode for that is an obscure USD parse error rather than
    anything that says "LFS". Catch it here with a clear message.
    """
    if not os.path.exists(path):
        return False, f"missing: {path}"
    size = os.path.getsize(path)
    if size < 1024:
        with open(path, "rb") as fh:
            head = fh.read(200)
        if b"git-lfs" in head:
            return False, (f"{os.path.basename(path)} is an un-pulled Git LFS "
                           f"pointer ({size} B). Run:  git lfs install && git lfs pull")
    return True, f"{os.path.basename(path)} ({size/1e6:.1f} MB)"


def make_world(physics_dt: float, add_ground: bool = True):
    from isaacsim.core.api import World
    world = World(stage_units_in_meters=1.0,
                  physics_dt=physics_dt, rendering_dt=physics_dt)
    if add_ground:
        world.scene.add_default_ground_plane()
    return world


def load_vehicle(world, which: str, prim_path: str = "/World/Vehicle"):
    """Put a vehicle on the stage and return it as an Articulation.

    `which` is 'leatherback' or 'mila'.

    On 'mila': FirstMilaStage_light.usda currently loads the Buick as a *payload
    reference* — a visual mesh. Grepping it for PhysicsRevoluteJoint,
    ArticulationRoot or any wheel/steer joint returns nothing. That means the
    Buick as committed CANNOT BE DRIVEN: there is no articulation to send joint
    targets to. Someone has to add a physics articulation (or a PhysX Vehicle)
    to that asset before closed-loop control can run on it.

    Until then 'leatherback' is the honest choice for proving the controller
    works, and swapping back is one flag.
    """
    from isaacsim.core.prims import Articulation
    from isaacsim.core.utils.stage import add_reference_to_stage
    from isaacsim.storage.native import get_assets_root_path

    if which == "mila":
        ok, msg = check_lfs(MILA_VEHICLE_USD)
        if not ok:
            raise RuntimeError(msg)
        add_reference_to_stage(usd_path=MILA_VEHICLE_USD, prim_path=prim_path)
    elif which == "leatherback":
        root = get_assets_root_path()
        if root is None:
            raise RuntimeError("Isaac assets server unreachable — check network "
                               "or set the local assets root.")
        add_reference_to_stage(usd_path=root + LEATHERBACK_SUBPATH,
                               prim_path=prim_path)
    else:
        raise ValueError(f"unknown vehicle: {which!r}")

    veh = Articulation(prim_paths_expr=prim_path, name="vehicle")
    world.scene.add(veh)
    return veh


def resolve_joints(vehicle, steer_names: List[str], drive_names: List[str],
                   list_only: bool = False) -> Optional[Tuple]:
    """Map joint names to indices, with a usable error when they do not exist.

    Joint names vary between assets and between Isaac releases. A wrong name is
    the single most common first-run failure and it fails SILENTLY — the car
    simply never steers, with no exception — so this refuses to continue rather
    than letting you debug a controller that is working fine.

    Call only AFTER world.reset(): articulations are not queryable until the
    physics scene has been initialised, and dof_names is None before that.
    """
    import numpy as np

    names = list(vehicle.dof_names or [])
    if list_only or not names:
        print(f"\n{len(names)} joints on this articulation:")
        for i, n in enumerate(names):
            print(f"  [{i:2d}] {n}")
        if not names:
            print("  (none — this prim has no articulation. If this is the Mila\n"
                  "   Buick, it is a visual payload with no physics joints yet;\n"
                  "   see load_vehicle().)")
        return None

    missing = [j for j in steer_names + drive_names if j not in names]
    if missing:
        print(f"\nERROR: these joints are not on the asset: {missing}")
        print("Available:")
        for i, n in enumerate(names):
            print(f"  [{i:2d}] {n}")
        print("\nUpdate STEER_JOINTS / DRIVE_JOINTS in the calling script.")
        return None

    return (np.array([names.index(j) for j in steer_names]),
            np.array([names.index(j) for j in drive_names]))


def quat_to_yaw(q) -> float:
    """Isaac returns quaternions as (w, x, y, z) — NOT the (x, y, z, w) that
    scipy and ROS use. Getting this backwards gives a yaw that looks almost
    right near zero and diverges as the car turns, which is a miserable bug to
    chase. Verify the ordering on your build before trusting a tracking result.
    """
    import math
    w, x, y, z = float(q[0]), float(q[1]), float(q[2]), float(q[3])
    return math.atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z))
