"""
Reference paths, and the lookahead-point search that Pure Pursuit runs on.

A path here is an ordered list of (x, y) waypoints, densely resampled so that
"find the point ld metres ahead" is a search over points rather than an analytic
solve. That is what every real implementation does, because the path arrives
from a planner as waypoints, not as a formula.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

import numpy as np


@dataclass
class Path:
    """A dense polyline in the world frame, with cumulative arc length.

    `resample()` is not cosmetic. Pure Pursuit finds its goal by walking the
    path for the first point at least `ld` away. If waypoints are 5 m apart and
    your lookahead is 3 m, that search can only ever return a point 5 m away,
    and the controller silently runs at the wrong lookahead. Resample finer than
    your smallest lookahead.
    """

    xy: np.ndarray                      # (N, 2)
    s: np.ndarray = field(init=False)   # (N,) cumulative arc length, metres

    def __post_init__(self):
        self.xy = np.asarray(self.xy, dtype=np.float64).reshape(-1, 2)
        if len(self.xy) < 2:
            raise ValueError("a path needs at least two points")
        seg = np.linalg.norm(np.diff(self.xy, axis=0), axis=1)
        self.s = np.concatenate([[0.0], np.cumsum(seg)])
        self.spacing = float(np.mean(seg)) if len(seg) else 1.0

    def _window_points(self, metres: float) -> int:
        return max(int(metres / max(self.spacing, 1e-6)), 2)

    @property
    def length(self) -> float:
        return float(self.s[-1])

    def resample(self, spacing: float = 0.05) -> "Path":
        n = max(int(self.length / spacing) + 1, 2)
        s_new = np.linspace(0.0, self.length, n)
        return Path(np.column_stack([np.interp(s_new, self.s, self.xy[:, 0]),
                                     np.interp(s_new, self.s, self.xy[:, 1])]))

    # ---------------------------------------------------------------- queries

    def nearest_index(self, x: float, y: float, start: int = 0,
                      window_m: float = 15.0) -> int:
        """Closest path point, searched forward within a bounded window.

        Both properties are needed, and each fixes a different real bug.

        *Forward-only* stops a self-crossing path snapping to the far branch at
        the crossing, which makes the controller yank the wheel.

        *Bounded* stops a CLOSED path — whose last point sits on top of its
        first — from always matching its own endpoint a few centimetres away,
        which makes the controller decide it has finished on step two. This code
        had exactly that bug; the window is the fix. Keep the window comfortably
        larger than the distance covered between control cycles, and comfortably
        smaller than the loop.
        """
        lo = max(0, min(start, len(self.xy) - 1))
        hi = min(len(self.xy), lo + self._window_points(window_m))
        if lo >= hi:
            return len(self.xy) - 1
        d = np.linalg.norm(self.xy[lo:hi] - np.array([x, y]), axis=1)
        return int(lo + np.argmin(d))

    def lookahead_point(self, x: float, y: float, ld: float, start: int = 0,
                        window_m: float = 15.0
                        ) -> Tuple[Optional[np.ndarray], int]:
        """First path point at least `ld` away, searching forward.

        Point is None when the search reaches the end of the path — a real case
        the caller must handle, not an error.
        """
        i0 = self.nearest_index(x, y, start=start, window_m=window_m)
        p = np.array([x, y])
        hi = min(len(self.xy), i0 + self._window_points(window_m + 4.0 * ld))
        for i in range(i0, hi):
            if np.linalg.norm(self.xy[i] - p) >= ld:
                return self.xy[i], i
        return (None, len(self.xy) - 1) if hi >= len(self.xy) else (self.xy[hi - 1], hi - 1)

    def curvature(self) -> np.ndarray:
        dx, dy = np.gradient(self.xy[:, 0]), np.gradient(self.xy[:, 1])
        ddx, ddy = np.gradient(dx), np.gradient(dy)
        return (dx * ddy - dy * ddx) / np.maximum((dx * dx + dy * dy) ** 1.5, 1e-12)

    def min_turn_radius(self) -> float:
        return float(1.0 / max(np.abs(self.curvature()).max(), 1e-9))

    def check_feasible(self, wheel_base: float, max_steer: float) -> Tuple[bool, str]:
        """Can a bicycle with these limits physically track this path?

        Run this on every reference before blaming the controller. A car with
        wheelbase L and steering limit d cannot turn tighter than
        R_min = L / tan(d). A path asking for less is impossible, and the
        tracking error is the reference's fault. This repo's figure-of-eight
        test failed exactly this way: the lobes needed 2.51 m, the car could do
        2.77 m.
        """
        needed = self.min_turn_radius()
        possible = wheel_base / math.tan(max_steer)
        ok = needed >= possible
        return ok, (f"path needs R >= {needed:.2f} m, vehicle can do "
                    f"R >= {possible:.2f} m{'' if ok else '  <-- INFEASIBLE'}")

    def cross_track_error(self, x: float, y: float, start: int = 0) -> float:
        """Signed perpendicular distance. Positive = left of the path.

        Signed, not absolute: a large signed mean means a systematic bias to one
        side, which is the signature of corner-cutting or a wrong reference
        point. An absolute error hides that completely.
        """
        i = self.nearest_index(x, y, start=start)
        j = min(i + 1, len(self.xy) - 1)
        if j == i:
            j, i = i, max(i - 1, 0)
        seg = self.xy[j] - self.xy[i]
        n = np.linalg.norm(seg)
        if n < 1e-9:
            return float(np.linalg.norm(self.xy[i] - np.array([x, y])))
        t = seg / n
        d = np.array([x, y]) - self.xy[i]
        return float(t[0] * d[1] - t[1] * d[0])


# --------------------------------------------------------------------------- #
#  Stock paths
# --------------------------------------------------------------------------- #

def straight(length: float = 50.0, spacing: float = 0.05) -> Path:
    n = int(length / spacing) + 1
    return Path(np.column_stack([np.linspace(0, length, n), np.zeros(n)]))


def circle(radius: float = 15.0, turns: float = 1.0, spacing: float = 0.05) -> Path:
    """Constant curvature, the one path with a known analytic answer: a bicycle
    of wheelbase L holding radius R needs exactly delta = atan(L/R), forever."""
    n = max(int(2 * math.pi * radius * turns / spacing) + 1, 2)
    th = np.linspace(0.0, 2 * math.pi * turns, n)
    return Path(np.column_stack([radius * np.sin(th), radius * (1.0 - np.cos(th))]))


def slalom(length: float = 60.0, amplitude: float = 4.0,
           wavelength: float = 20.0, spacing: float = 0.05) -> Path:
    """A sine weave — 'a car turning back and forth' as a trackable reference."""
    n = int(length / spacing) + 1
    x = np.linspace(0.0, length, n)
    return Path(np.column_stack([x, amplitude * np.sin(2 * math.pi * x / wavelength)]))


def figure_eight(radius: float = 20.0, spacing: float = 0.05) -> Path:
    """Crosses itself — the case that breaks a naive global nearest-point search.

    Mind the radius: at 12 m the lobes demand a 2.51 m turn radius, tighter than
    a 1.6 m-wheelbase car at 30 degrees can manage (2.77 m). Check
    `check_feasible` before using a tighter one.
    """
    n = max(int(4 * math.pi * radius / spacing) + 1, 2)
    t = np.linspace(0.0, 2 * math.pi, n)
    return Path(np.column_stack([radius * np.sin(t), radius * np.sin(t) * np.cos(t)]))
