"""
Closed-loop runner: bicycle plant + Pure Pursuit, logging everything.

This is what lets the controller be developed and proven on a laptop with no GPU
and no Isaac Sim. When the GPU machine appears, the controller does not change —
only the adapter that feeds it poses and takes steering commands.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import List, Optional

import numpy as np

from .path import Path
from .pure_pursuit import PurePursuit, PurePursuitConfig
from .vehicle import KinematicBicycle, VehicleParams, VehicleState, speed_controller


@dataclass
class RunLog:
    t: List[float] = field(default_factory=list)
    x: List[float] = field(default_factory=list)
    y: List[float] = field(default_factory=list)
    yaw: List[float] = field(default_factory=list)
    v: List[float] = field(default_factory=list)
    steer: List[float] = field(default_factory=list)
    steer_raw: List[float] = field(default_factory=list)
    lookahead: List[float] = field(default_factory=list)
    cross_track: List[float] = field(default_factory=list)
    goal_x: List[float] = field(default_factory=list)
    goal_y: List[float] = field(default_factory=list)

    def arr(self, name: str) -> np.ndarray:
        return np.asarray(getattr(self, name), dtype=np.float64)

    @property
    def xte_rms(self) -> float:
        e = self.arr("cross_track")
        return float(np.sqrt(np.mean(e ** 2))) if len(e) else float("nan")

    @property
    def xte_max(self) -> float:
        e = self.arr("cross_track")
        return float(np.max(np.abs(e))) if len(e) else float("nan")

    @property
    def xte_mean_signed(self) -> float:
        """Mean SIGNED error. A large signed mean means a systematic bias to one
        side — corner-cutting, or a wrong reference point. Near-zero signed mean
        with large RMS means oscillation instead. The two need different fixes."""
        e = self.arr("cross_track")
        return float(np.mean(e)) if len(e) else float("nan")

    @property
    def steer_reversals(self) -> int:
        """Cheap oscillation detector: a smooth slalom has a handful, a
        controller hunting about the path has hundreds."""
        d = np.diff(self.arr("steer"))
        d = d[np.abs(d) > 1e-4]
        return int(np.sum(np.diff(np.sign(d)) != 0)) if len(d) > 1 else 0

    def summary(self) -> str:
        return (f"xte rms {self.xte_rms:6.3f} m | max {self.xte_max:6.3f} m | "
                f"mean signed {self.xte_mean_signed:+6.3f} m | "
                f"reversals {self.steer_reversals:4d} | "
                f"t {self.t[-1] if self.t else 0:5.1f} s")


def run(path: Path, controller: Optional[PurePursuit] = None,
        vehicle: Optional[KinematicBicycle] = None,
        v_target: float = 6.0, dt: float = 0.02, t_max: float = 120.0,
        start_offset: float = 0.0) -> RunLog:
    """Drive `path` closed-loop and return the log.

    `start_offset` places the vehicle that many metres to the left of the start,
    which is how you see the approach transient — behaviour a run starting
    perfectly on the path hides completely.
    """
    veh = vehicle or KinematicBicycle(VehicleParams())
    ctl = controller or PurePursuit(path, PurePursuitConfig(
        wheel_base=veh.p.wheel_base, max_steer=veh.p.max_steer))

    p0, p1 = path.xy[0], path.xy[min(5, len(path.xy) - 1)]
    yaw0 = math.atan2(p1[1] - p0[1], p1[0] - p0[0])
    veh.state = VehicleState(x=float(p0[0] - start_offset * math.sin(yaw0)),
                             y=float(p0[1] + start_offset * math.cos(yaw0)),
                             yaw=yaw0, v=v_target * 0.5)

    log, t, travelled = RunLog(), 0.0, 0.0
    while t < t_max:
        s = veh.state

        # Stop once the vehicle has covered the path's arc length. This is what
        # terminates a CLOSED path: "am I near the end point" is true before a
        # loop has gone anywhere. Without it the run keeps driving a second lap
        # while cross-track error is silently measured against the path's final
        # point — which reads as a 50 m tracking failure that never happened.
        if travelled >= path.length:
            break

        steer, dbg = ctl.step(s.x, s.y, s.yaw, s.v, dt)
        accel = speed_controller(s.v, v_target)

        log.t.append(t); log.x.append(s.x); log.y.append(s.y)
        log.yaw.append(s.yaw); log.v.append(s.v)
        log.steer.append(steer); log.steer_raw.append(dbg.steer_raw)
        log.lookahead.append(dbg.lookahead); log.cross_track.append(dbg.cross_track)
        if dbg.goal_xy is not None:
            log.goal_x.append(float(dbg.goal_xy[0])); log.goal_y.append(float(dbg.goal_xy[1]))
        else:
            log.goal_x.append(float("nan")); log.goal_y.append(float("nan"))

        if ctl.state.finished:
            break
        veh.step(steer, accel, dt)
        travelled += abs(veh.state.v) * dt
        t += dt

    return log


def run_open_loop(vehicle: Optional[KinematicBicycle] = None,
                  amplitude_deg: float = 18.0, period_s: float = 6.0,
                  v_target: float = 5.0, dt: float = 0.02,
                  t_max: float = 24.0, yaw0: float = 0.0,
                  center_heading: bool = False) -> RunLog:
    """No controller at all: steering is a sine wave in time.

    The October 4th task in its simplest honest form — 'a car turning back and
    forth, without a proper controller on top'. No path and no feedback, so
    cross-track error stays zero; the point is that the vehicle model, the
    steering convention and the integrator behave.

    EXPECT A DIAGONAL STAIRCASE, NOT A WEAVE ABOUT THE START LINE. A symmetric
    steering input does not give a symmetric path, and this surprises people:

        yaw(t) = integral of (v/L)*tan(delta) dt

    Starting the sine at phase zero makes that integral non-negative for the
    whole first half-period, so the heading swings between 0 and +98 deg rather
    than about zero. The steering is symmetric; the HEADING is not, and the
    trajectory inherits the bias. Steering reverses correctly throughout — which
    is what the task asks for — but the car drifts steadily to one side.

    `center_heading=True` removes the bias by measuring the mean heading in a
    throwaway pass and starting the vehicle at minus that value, which makes the
    path weave about the x-axis. It is cosmetic: the steering command is
    identical either way.
    """
    if center_heading:
        probe = run_open_loop(vehicle=KinematicBicycle(
            (vehicle or KinematicBicycle()).p), amplitude_deg=amplitude_deg,
            period_s=period_s, v_target=v_target, dt=dt, t_max=t_max)
        yaw0 = -float(np.mean(probe.arr("yaw")))

    veh = vehicle or KinematicBicycle(VehicleParams())
    veh.state = VehicleState(yaw=yaw0, v=v_target * 0.5)
    amp, log, t = math.radians(amplitude_deg), RunLog(), 0.0

    while t < t_max:
        steer = amp * math.sin(2.0 * math.pi * t / period_s)
        s = veh.state
        log.t.append(t); log.x.append(s.x); log.y.append(s.y)
        log.yaw.append(s.yaw); log.v.append(s.v)
        log.steer.append(steer); log.steer_raw.append(steer)
        log.lookahead.append(0.0); log.cross_track.append(0.0)
        log.goal_x.append(float("nan")); log.goal_y.append(float("nan"))
        veh.step(steer, speed_controller(s.v, v_target), dt)
        t += dt

    return log
