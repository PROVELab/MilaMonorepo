"""
Model Predictive Control for lateral tracking.

Pure Pursuit answers "where do I steer to reach a point ahead?". MPC answers a
different question: "what sequence of steering commands over the next couple of
seconds minimises tracking error, given everything I know about the path and
everything the actuator cannot do?". It then applies only the first command and
re-solves on the next tick.

Two consequences follow, and they are the entire argument for the extra
complexity:

1.  IT SEES THE CORNER COMING. The path's curvature over the whole horizon
    enters the problem as a known disturbance. Pure Pursuit only ever looks at
    one point, so it is always reacting; MPC can start turning before the corner
    arrives. This is what removes the steady-state corner-cutting bias that is
    structural in Pure Pursuit.

2.  LIMITS ARE PART OF THE PROBLEM, NOT AN AFTERTHOUGHT. Pure Pursuit computes a
    steering angle and then clamps it, which means the clamped command is no
    longer the one the geometry asked for. MPC optimises subject to the angle
    and slew limits, so it plans a trajectory the actuator can actually execute.

WHY THE KINEMATIC MODEL AND NOT A DYNAMIC ONE
---------------------------------------------
We now have the parameters for a dynamic bicycle model — mass, yaw inertia and
cornering stiffnesses are all in VehicleParams. This uses the kinematic model
anyway, on purpose: the asset's cornering stiffnesses are synthetic (exactly
proportional to static load, giving a perfectly neutral-steer car, which is the
signature of auto-generated defaults). A dynamic model would inherit that
assumption and quietly bake it into the controller. The kinematic model does not
use cornering stiffness at all, so it cannot. Revisit when real tyre data exists.

THE FORMULATION
---------------
Error state relative to the path, with e_y the cross-track error, e_psi the
heading error against the path tangent, kappa the path curvature, v the speed
and L the wheelbase:

    e_y'   = v sin(e_psi)       ~  v e_psi
    e_psi' = (v/L) tan(delta)   -  v kappa      ~  (v/L) delta - v kappa

Discretised at dt, with x = [e_y, e_psi] and u = delta:

    x[k+1] = A x[k] + B u[k] + d[k]

    A = [[1, dt*v],        B = [[0],            d[k] = [0, -dt*v*kappa[k]]
         [0,   1 ]]             [dt*v/L]]

The small-angle approximations are honest here because a tracking controller
that is working keeps both errors small — and if they are not small, the
linearisation error is the least of your problems. `kappa[k]` is read from the
path ahead, which is where the preview advantage comes from.

That makes the problem a quadratic program: quadratic cost, linear dynamics,
linear constraints. It is solved with OSQP, which is a few hundred microseconds
for a horizon this size and runs on ARM, so the same code can go on the Orin.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Optional, Tuple

import numpy as np
import scipy.sparse as sp

from .path import Path

try:
    import osqp
    _HAVE_OSQP = True
except ImportError:                                        # pragma: no cover
    _HAVE_OSQP = False


@dataclass
class MPCConfig:
    wheel_base: float = 2.78
    cg_to_rear_axle: float = 1.251

    # Horizon. 30 steps at 0.05 s = 1.5 s of preview. At 6 m/s that is 9 m
    # ahead, comparable to Pure Pursuit's lookahead — the difference is that MPC
    # uses the whole interval rather than the single point at the end of it.
    horizon: int = 30
    dt: float = 0.05                 # s, 20 Hz

    # Cost weights. Only their RATIOS matter, so q_cross is pinned at 1.0 and
    # everything else is expressed relative to it.
    q_cross: float = 1.0             # penalty on cross-track error (m)
    q_heading: float = 0.5           # penalty on heading error (rad)
    r_steer: float = 0.5             # penalty on absolute steering
    r_steer_rate: float = 12.0       # penalty on CHANGE in steering

    # Terminal weight. Without it the controller is happy to end the horizon
    # badly placed, because nothing past the horizon is in the cost — which
    # shows up as drift that reappears every time the horizon slides forward.
    q_terminal: float = 6.0

    max_steer: float = 0.5           # rad
    max_steer_rate: float = math.radians(120.0)   # rad/s


@dataclass
class MPCDebug:
    cross_track: float
    heading_error: float
    curvature_preview: np.ndarray    # kappa over the horizon
    planned_steer: np.ndarray        # the whole planned sequence
    solve_time_ms: float
    solver_status: str
    goal_xy: Optional[np.ndarray]
    lookahead: float                 # horizon distance, for log compatibility
    steer_raw: float
    steer_cmd: float
    curvature: float
    alpha: float
    goal_index: int


class MPC:
    """Linear time-varying MPC for lateral tracking.

    Interface-compatible with PurePursuit: construct with a Path, call
    `.step(x, y, yaw, v, dt)`, get back `(steer, debug)`. That is deliberate —
    `sim.run()` and the Isaac adapters take either without modification, which
    is what makes a like-for-like comparison possible.
    """

    def __init__(self, path: Path, cfg: Optional[MPCConfig] = None):
        if not _HAVE_OSQP:
            raise ImportError(
                "MPC needs osqp. Install it with `uv sync`, or `pip install osqp`.")
        self.path = path
        self.cfg = cfg or MPCConfig()
        self.last_index = 0
        self.last_steer = 0.0
        self.finished = False
        self._prob = None            # OSQP instance, built once and warm-started
        self._nz = None

    # -- geometry -----------------------------------------------------------

    def rear_axle(self, x: float, y: float, yaw: float,
                  from_cg: bool = False) -> Tuple[float, float]:
        if not from_cg:
            return x, y
        d = self.cfg.cg_to_rear_axle
        return x - d * math.cos(yaw), y - d * math.sin(yaw)

    def _errors(self, x: float, y: float, yaw: float) -> Tuple[float, float, int]:
        """Cross-track and heading error against the path, plus the index.

        Uses the same forward-only bounded search as Pure Pursuit, for the same
        two reasons: it stops a self-crossing path jumping branches, and it stops
        a closed path matching its own start point.
        """
        i = self.path.nearest_index(x, y, start=self.last_index)
        self.last_index = i
        e_y = self.path.cross_track_error(x, y, start=i)

        j = min(i + 1, len(self.path.xy) - 1)
        if j == i:
            j, i2 = len(self.path.xy) - 1, len(self.path.xy) - 2
            tang = self.path.xy[j] - self.path.xy[i2]
        else:
            tang = self.path.xy[j] - self.path.xy[i]
        path_yaw = math.atan2(tang[1], tang[0])
        e_psi = math.atan2(math.sin(yaw - path_yaw), math.cos(yaw - path_yaw))
        return e_y, e_psi, i

    def _curvature_preview(self, i: int, v: float) -> np.ndarray:
        """Path curvature at each horizon step, sampled by how far the car will
        actually have travelled — not by path index. This is the preview that
        Pure Pursuit structurally cannot use."""
        cfg = self.cfg
        kap = self.path.curvature()
        n = len(kap)
        step_m = max(abs(v) * cfg.dt, 1e-6)
        spacing = max(self.path.spacing, 1e-6)
        out = np.empty(cfg.horizon)
        for k in range(cfg.horizon):
            idx = int(round(i + k * step_m / spacing))
            out[k] = kap[min(idx, n - 1)]
        return out

    # -- the QP -------------------------------------------------------------

    def _build(self, x0: np.ndarray, v: float, kappa: np.ndarray,
               last_u: float):
        """Assemble the sparse QP.

        Decision vector z = [x_0 ... x_N, u_0 ... u_{N-1}], so the dynamics are
        linear equality constraints rather than being substituted out. That
        keeps the matrices sparse and banded, which is what OSQP is fast at, and
        it is far easier to get right than a condensed formulation.
        """
        cfg = self.cfg
        N, nx, nu = cfg.horizon, 2, 1
        nz = nx * (N + 1) + nu * N

        A = np.array([[1.0, cfg.dt * v], [0.0, 1.0]])
        B = np.array([[0.0], [cfg.dt * v / cfg.wheel_base]])

        # --- cost -----------------------------------------------------------
        # Stage cost on the states, terminal weight on the last one, plus the
        # steering-rate term which couples neighbouring inputs (hence the
        # off-diagonal entries below).
        P = sp.lil_matrix((nz, nz))
        q = np.zeros(nz)
        for k in range(N + 1):
            w = cfg.q_terminal if k == N else 1.0
            P[nx * k, nx * k] = 2.0 * cfg.q_cross * w
            P[nx * k + 1, nx * k + 1] = 2.0 * cfg.q_heading * w

        u0 = nx * (N + 1)
        for k in range(N):
            P[u0 + k, u0 + k] += 2.0 * (cfg.r_steer + 2.0 * cfg.r_steer_rate)
            if k + 1 < N:
                P[u0 + k, u0 + k + 1] += -2.0 * cfg.r_steer_rate
                P[u0 + k + 1, u0 + k] += -2.0 * cfg.r_steer_rate
        # u_{-1} is the command actually applied last tick, so the first input
        # is penalised for jumping away from it. Without this the controller can
        # emit a step change on every solve and still think it is being smooth.
        P[u0, u0] += 0.0
        q[u0] += -2.0 * cfg.r_steer_rate * last_u
        # The last input has no successor, so it carries one rate term, not two.
        P[u0 + N - 1, u0 + N - 1] += -2.0 * cfg.r_steer_rate

        # --- equality constraints: dynamics ---------------------------------
        Aeq = sp.lil_matrix((nx * (N + 1), nz))
        leq = np.zeros(nx * (N + 1))
        Aeq[0:nx, 0:nx] = -sp.eye(nx)
        leq[0:nx] = -x0
        for k in range(N):
            r = nx * (k + 1)
            Aeq[r:r + nx, nx * k:nx * (k + 1)] = A
            Aeq[r:r + nx, nx * (k + 1):nx * (k + 2)] = -sp.eye(nx)
            Aeq[r:r + nx, u0 + k:u0 + k + 1] = B
            # Affine term: the path bending away under the car.
            leq[r:r + nx] = -np.array([0.0, -cfg.dt * v * kappa[k]])

        # --- inequality constraints: angle and slew limits ------------------
        Ain = sp.lil_matrix((2 * N, nz))
        lin = np.zeros(2 * N)
        uin = np.zeros(2 * N)
        for k in range(N):
            Ain[k, u0 + k] = 1.0
            lin[k], uin[k] = -cfg.max_steer, cfg.max_steer
        dmax = cfg.max_steer_rate * cfg.dt
        for k in range(N):
            Ain[N + k, u0 + k] = 1.0
            if k == 0:
                lin[N + k], uin[N + k] = last_u - dmax, last_u + dmax
            else:
                Ain[N + k, u0 + k - 1] = -1.0
                lin[N + k], uin[N + k] = -dmax, dmax

        Acon = sp.vstack([Aeq, Ain], format="csc")
        lower = np.hstack([leq, lin])
        upper = np.hstack([leq, uin])
        return sp.csc_matrix(P), q, Acon, lower, upper, nz

    def step(self, x: float, y: float, yaw: float, v: float, dt: float
             ) -> Tuple[float, MPCDebug]:
        """One control cycle. Signature matches PurePursuit.step()."""
        import time
        cfg = self.cfg
        t_start = time.perf_counter()

        e_y, e_psi, i = self._errors(x, y, yaw)

        # Path exhausted: hold the last command and flag it, same rule as Pure
        # Pursuit — both conditions, so a closed path does not finish instantly.
        near_end = i >= len(self.path.xy) - 2
        if near_end:
            end = self.path.xy[-1]
            if float(np.linalg.norm(end - np.array([x, y]))) < 2.0:
                self.finished = True
            return self.last_steer, MPCDebug(
                e_y, e_psi, np.zeros(cfg.horizon),
                np.full(cfg.horizon, self.last_steer), 0.0, "path_end",
                None, 0.0, self.last_steer, self.last_steer, 0.0, 0.0,
                len(self.path.xy) - 1)

        # Below a crawl the error dynamics go singular (both A and B scale with
        # v), and steering a stationary car is meaningless anyway.
        if abs(v) < 0.1:
            return self.last_steer, MPCDebug(
                e_y, e_psi, np.zeros(cfg.horizon),
                np.full(cfg.horizon, self.last_steer), 0.0, "too_slow",
                None, 0.0, self.last_steer, self.last_steer, 0.0, 0.0, i)

        kappa = self._curvature_preview(i, v)
        x0 = np.array([e_y, e_psi])
        P, q, A, l, u, nz = self._build(x0, v, kappa, self.last_steer)

        if self._prob is None or self._nz != nz:
            self._prob = osqp.OSQP()
            self._prob.setup(P=P, q=q, A=A, l=l, u=u, verbose=False,
                             warm_starting=True, eps_abs=1e-5, eps_rel=1e-5,
                             max_iter=4000)
            self._nz = nz
        else:
            # Warm start: re-solving a problem that barely changed from the one
            # solved 50 ms ago is much cheaper than starting cold, which is what
            # makes MPC viable at 20 Hz on modest hardware.
            self._prob.update(Px=sp.triu(P, format="csc").data, q=q,
                              Ax=A.data, l=l, u=u)

        res = self._prob.solve()
        status = str(res.info.status)

        u0_index = 2 * (cfg.horizon + 1)
        if res.x is None or not np.isfinite(res.x[u0_index]):
            # Infeasible or failed: hold the last command rather than applying a
            # NaN. Report it honestly in the debug record instead of hiding it.
            steer = self.last_steer
            plan = np.full(cfg.horizon, steer)
            status = f"FAILED({status})"
        else:
            plan = np.asarray(res.x[u0_index:u0_index + cfg.horizon])
            steer = float(plan[0])

        steer = float(np.clip(steer, -cfg.max_steer, cfg.max_steer))
        self.last_steer = steer
        ms = (time.perf_counter() - t_start) * 1e3

        goal = None
        gi = min(i + int(cfg.horizon * abs(v) * cfg.dt /
                         max(self.path.spacing, 1e-6)), len(self.path.xy) - 1)
        goal = self.path.xy[gi]

        return steer, MPCDebug(
            cross_track=e_y, heading_error=e_psi, curvature_preview=kappa,
            planned_steer=plan, solve_time_ms=ms, solver_status=status,
            goal_xy=goal, lookahead=float(cfg.horizon * abs(v) * cfg.dt),
            steer_raw=steer, steer_cmd=steer, curvature=float(kappa[0]),
            alpha=e_psi, goal_index=gi)

    def reset(self) -> None:
        self.last_index = 0
        self.last_steer = 0.0
        self.finished = False
        self._prob = None

    # -- compatibility with sim.run(), which reads controller.state ----------

    @property
    def state(self):
        return self

    @property
    def last_steer_value(self) -> float:
        return self.last_steer
