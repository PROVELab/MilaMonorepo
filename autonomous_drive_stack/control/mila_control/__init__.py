"""Lateral control for Mila. Simulator-agnostic: no Isaac imports in here."""
from .pure_pursuit import PurePursuit, PurePursuitConfig, analytic_steer_for_radius
from .vehicle import KinematicBicycle, VehicleParams, VehicleState, speed_controller
from .path import Path, straight, circle, slalom, figure_eight
from .sim import run, run_open_loop, RunLog

# MPC needs osqp. Import it lazily-but-eagerly: if osqp is missing we still want
# `import mila_control` to work, so Pure Pursuit keeps running on a machine that
# has not been synced yet. MPC then raises a clear error when constructed.
try:
    from .mpc import MPC, MPCConfig, MPCDebug
    HAVE_MPC = True
except ImportError:                                        # pragma: no cover
    HAVE_MPC = False

__all__ = [n for n in dir() if not n.startswith("_")]
