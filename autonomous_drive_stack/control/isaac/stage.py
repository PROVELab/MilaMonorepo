"""
Shared Isaac Sim setup for the Mila simulation scripts.

Targets Isaac Sim 6.1 (what the team is running — the title bar says
"Isaac Sim Python 6.1.0"). The `isaacsim.*` namespace replaced `omni.isaac.*`
at 4.5; anything you find online on the old namespace will not import.

IMPORT ORDER MATTERS. Everything here comes from extensions that only exist once
`SimulationApp` has booted the Kit runtime, which is why this module has no
top-level isaacsim imports and does them inside functions instead. The entry
points construct SimulationApp first, then import this.

--------------------------------------------------------------------------
WHAT CHANGED, AND WHY THE FIRST VERSION LOADED THE BUICK AND CRASHED
--------------------------------------------------------------------------
Two bugs, both mine, both now fixed:

1.  IT POINTED AT THE WRONG STAGE. The old MILA_STAGE constant pointed at
    `autonomous_drive_stack/simulation/FirstMilaStage_light.usda`, because when
    I wrote it that was the only stage in the monorepo. That stage loads the
    Buick as a *visual payload* with no physics at all. The drivable Mila lives
    in a different repo entirely — PROVELab/autonomous-drive-stack, at
    `simulation/parking_lot/parking_lot.usda`, which sublayers
    `mila_vehicle.usd`. If you only have the monorepo checked out, the Buick
    stage is the only thing there is, so that is what it loaded.

2.  IT DROVE THE WRONG INTERFACE. The old code built an `AckermannController`
    and wrote joint position/velocity targets. Mila is a **PhysX Vehicle** —
    suspension raycasts and wheel attachments, NOT a jointed articulation.
    There are no joints to write to. Wrapping it in `Articulation` and asking
    for `dof_names` is what produced the crash: the code then indexes into
    something that does not exist.

    PhysX Vehicle instead takes three normalised floats on the vehicle prim:

        physxVehicleController:steer         [-1, 1], scaled by maxSteerAngle
        physxVehicleController:accelerator   [0, 1]
        physxVehicleController:brake0        [0, 1]

    The asset already declares `physxVehicleAckermannSteering`, so the
    inner/outer wheel split happens inside the engine. The whole Ackermann code
    path is deleted.

--------------------------------------------------------------------------
UNVERIFIED — I cannot run Isaac Sim, so this has not been executed.
Every Isaac API call below is marked [VERIFIED] where it is read straight out
of the USD we have in hand, or [ASSUMED] where it is an Isaac API I believe is
right but have not run. Start with --list-vehicle, which only reads the stage.
--------------------------------------------------------------------------
"""

from __future__ import annotations

import os
from typing import List, Optional, Tuple

# [VERIFIED] Prim paths read directly out of mila_vehicle.usd.
VEHICLE_PRIM = "/Mila/Mila_Root"
VEHICLE_ROOT = "/Mila"

# [VERIFIED] from the USD: maxSteerAngle = 0.5 rad. The steer input is
# normalised, so a command in radians is divided by this.
MAX_STEER_ANGLE = 0.5

# Where the drivable Mila stage might be. The monorepo does NOT contain it
# today — it is in the separate autonomous-drive-stack repo — so we search
# several plausible layouts rather than hardcoding one and failing obscurely.
_HERE = os.path.dirname(os.path.abspath(__file__))
_CONTROL = os.path.dirname(_HERE)
_ADS = os.path.dirname(_CONTROL)                 # autonomous_drive_stack/
_REPO = os.path.dirname(_ADS)                    # MilaMonorepo/

STAGE_SEARCH_PATHS = [
    # If the parking lot stage ever moves into the monorepo:
    os.path.join(_ADS, "simulation", "parking_lot", "parking_lot.usda"),
    # Sibling checkout of autonomous-drive-stack next to the monorepo:
    os.path.join(_REPO, "..", "autonomous-drive-stack", "simulation",
                 "parking_lot", "parking_lot.usda"),
    os.path.join(os.path.expanduser("~"), "Projects", "autonomous-drive-stack",
                 "simulation", "parking_lot", "parking_lot.usda"),
]

# The Buick stage. Kept ONLY so the error message can tell you that you have
# found this one instead of the one you want. It is not drivable.
BUICK_STAGE = os.path.join(_ADS, "simulation", "FirstMilaStage_light.usda")


def find_stage(explicit: Optional[str] = None) -> str:
    """Locate the drivable Mila stage, or explain exactly what to do.

    An explicit --stage path always wins. Otherwise we search the known
    layouts. If we only find the Buick stage, say so loudly rather than loading
    it and letting the user wonder why there is a 1960s coupe on screen.
    """
    if explicit:
        if not os.path.exists(explicit):
            raise FileNotFoundError(f"--stage given but not found: {explicit}")
        return os.path.normpath(explicit)

    for p in STAGE_SEARCH_PATHS:
        if os.path.exists(p):
            return os.path.normpath(p)

    msg = [
        "",
        "Could not find the drivable Mila stage (parking_lot.usda).",
        "",
        "It is NOT in MilaMonorepo. It lives in a separate repository:",
        "",
        "    git clone https://github.com/PROVELab/autonomous-drive-stack.git",
        "    cd autonomous-drive-stack && git lfs install && git lfs pull",
        "",
        "then point this script at it:",
        "",
        "    --stage /path/to/autonomous-drive-stack/simulation/parking_lot/parking_lot.usda",
        "",
        "Searched:",
    ]
    msg += [f"    {os.path.normpath(p)}" for p in STAGE_SEARCH_PATHS]
    if os.path.exists(BUICK_STAGE):
        msg += [
            "",
            "NOTE: FirstMilaStage_light.usda IS present in this repo, but that",
            "stage loads the Buick as a visual payload with no physics — no",
            "articulation, no PhysX vehicle, nothing to steer. Loading it is",
            "what the previous version of this script did by mistake.",
        ]
    raise FileNotFoundError("\n".join(msg))


def check_lfs(path: str) -> Tuple[bool, str]:
    """Is this a real USD, or an un-pulled Git LFS pointer?

    Both repos track *.usd through LFS. A clone without `git lfs pull` leaves
    ~130-byte text pointers in place of the assets, and Isaac's failure mode is
    an obscure USD parse error that never mentions LFS.
    """
    if not os.path.exists(path):
        return False, f"missing: {path}"
    size = os.path.getsize(path)
    if size < 1024:
        with open(path, "rb") as fh:
            if b"git-lfs" in fh.read(200):
                return False, (f"{os.path.basename(path)} is an un-pulled Git LFS "
                               f"pointer ({size} B). Run: git lfs install && git lfs pull")
    return True, f"{os.path.basename(path)} ({size/1e6:.1f} MB)"


def open_stage(stage_path: str, physics_dt: float):
    """Open the team's stage and wrap it in a World. [ASSUMED]

    We OPEN the existing stage rather than building a new one and referencing
    the vehicle into it: the parking lot already carries the PhysicsScene,
    collision geometry, cones and the Ouster sensor rig, and rebuilding that
    by hand would drift from what the team actually uses.
    """
    from isaacsim.core.api import World
    from isaacsim.core.utils.stage import open_stage as _open

    _open(stage_path)
    world = World(stage_units_in_meters=1.0,
                  physics_dt=physics_dt, rendering_dt=physics_dt)
    return world


def get_vehicle(prim_path: str = VEHICLE_PRIM, list_only: bool = False):
    """Return the vehicle prim, verifying it is actually a PhysX vehicle.

    Checks for the controller attributes rather than assuming — a wrong prim
    path otherwise fails silently later, with the car simply never moving.
    """
    import omni.usd
    stage = omni.usd.get_context().get_stage()
    prim = stage.GetPrimAtPath(prim_path)

    if not prim or not prim.IsValid():
        print(f"\nERROR: no prim at {prim_path}")
        print("Prims on this stage (top two levels):")
        for p in stage.Traverse():
            if str(p.GetPath()).count("/") <= 2:
                print(f"    {p.GetPath()}  [{p.GetTypeName()}]")
        return None

    needed = ["physxVehicleController:steer",
              "physxVehicleController:accelerator",
              "physxVehicleController:brake0"]
    missing = [a for a in needed if not prim.GetAttribute(a).IsValid()]

    if list_only or missing:
        print(f"\nprim: {prim.GetPath()}  [{prim.GetTypeName()}]")
        print(f"applied schemas: {list(prim.GetAppliedSchemas())}")
        phys = sorted(a.GetName() for a in prim.GetAttributes()
                      if a.GetName().startswith("physx"))
        print(f"{len(phys)} physx attributes:")
        for a in phys[:30]:
            print(f"    {a}")
        if missing:
            print(f"\nERROR: missing vehicle controller attributes: {missing}")
            print("This prim is not a PhysX vehicle. Is the stage the right one?")
            return None
        return None if list_only else prim

    return prim


def drive(prim, steer_rad: float, accelerator: float = 0.0,
          brake: float = 0.0, steer_sign: float = 1.0) -> float:
    """Write one control command to the PhysX vehicle. [ASSUMED]

    `steer_rad` is a steering angle in radians, the same quantity both
    controllers produce. PhysX wants it normalised against maxSteerAngle, so
    this does that conversion in one place.

    steer_sign: I have NOT verified whether positive steer turns left or right
    in this asset. If the car tracks mirror-image, flip this to -1 rather than
    negating the controller, which would be the wrong place to fix it.
    """
    s = float(max(-1.0, min(1.0, steer_sign * steer_rad / MAX_STEER_ANGLE)))
    prim.GetAttribute("physxVehicleController:steer").Set(s)
    prim.GetAttribute("physxVehicleController:accelerator").Set(
        float(max(0.0, min(1.0, accelerator))))
    prim.GetAttribute("physxVehicleController:brake0").Set(
        float(max(0.0, min(1.0, brake))))
    return s


def get_pose(prim_path: str = VEHICLE_PRIM):
    """World pose of the vehicle as (x, y, yaw). [ASSUMED]

    Read through the USD xform, which PhysX writes back each step. Returns yaw
    about Z — the stage is Z-up (`upAxis = "Z"`, verified in the USD).
    """
    import math
    import omni.usd
    from pxr import Usd, UsdGeom

    stage = omni.usd.get_context().get_stage()
    prim = stage.GetPrimAtPath(prim_path)
    m = UsdGeom.Xformable(prim).ComputeLocalToWorldTransform(
        Usd.TimeCode.Default())
    t = m.ExtractTranslation()
    q = m.ExtractRotationQuat()
    w = q.GetReal()
    i, j, k = q.GetImaginary()
    yaw = math.atan2(2.0 * (w * k + i * j), 1.0 - 2.0 * (j * j + k * k))
    return float(t[0]), float(t[1]), float(yaw)
