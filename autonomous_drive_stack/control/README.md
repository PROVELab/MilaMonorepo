# Lateral control for Mila — Pure Pursuit

**Chris** · `user/chris/pure-pursuit` · October 2026

> **Revised after finding the real vehicle.** Every number in this document was
> re-measured against Mila's actual geometry, read out of
> `autonomous-drive-stack/simulation/parking_lot/mila_vehicle.usd`. The earlier
> version of this report used placeholder parameters that were wrong by up to
> 74%, and two of its test paths turn out to be physically undrivable by the
> real car. Section 10 records what changed and why.

---

## Contents

1. [Summary](#1-summary)
2. [Mila's real parameters](#2-milas-real-parameters)
3. [How it's put together, and why](#3-how-its-put-together-and-why)
4. [Deliverable 1 — open loop, no controller](#4-deliverable-1--open-loop-no-controller)
5. [Deliverable 2 — Pure Pursuit](#5-deliverable-2--pure-pursuit)
6. [Does it work? The analytic check](#6-does-it-work-the-analytic-check)
7. [Tuning the lookahead — measured, not guessed](#7-tuning-the-lookahead--measured-not-guessed)
8. [When the path is the problem](#8-when-the-path-is-the-problem)
9. [Code walkthrough](#9-code-walkthrough)
10. [What changed when the real parameters arrived](#10-what-changed-when-the-real-parameters-arrived)
11. [Isaac Sim integration](#11-isaac-sim-integration)
12. [Toward MPC](#12-toward-mpc)
13. [How to run it](#13-how-to-run-it)

---

## 1. Summary

A Pure Pursuit lateral controller, implemented from the geometry, verified against
closed-form answers, and tuned against Mila's real dimensions.

| | |
|---|---|
| Steering on a circle vs `δ = atan(L/R)` | matches to **<10⁻³ °** across R = 10…40 m |
| Straight line, started on the path | **0.0000 m** |
| Straight line, started 3 m off | settles to **0.0007 m**, 3 reversals |
| Slalom (4 m amplitude, 40 m wavelength) at 6 m/s | **0.282 m** RMS, 0.473 m max |
| Figure-eight R = 30 m through its own crossing | **0.740 m** max, no branch jump |
| Full check suite | ~3 s on a laptop, no GPU |

Not yet executed inside Isaac Sim — section 11.

---

## 2. Mila's real parameters

Read directly out of the simulation asset, not measured and not assumed:

**Source:** `PROVELab/autonomous-drive-stack`,
`simulation/parking_lot/mila_vehicle.usd`, prim `/Mila/Mila_Root`
(sha256 `6831b62e…b44e4e2`, 7,044,754 bytes)

| parameter | value | USD attribute |
|---|---|---|
| wheelbase | **2.78 m** | `physxVehicleAckermannSteering:wheelBase` |
| track width | **1.73 m** | `physxVehicleAckermannSteering:trackWidth` |
| max steer angle | **0.5 rad (28.65°)** | `physxVehicleAckermannSteering:maxSteerAngle` |
| wheel radius | **0.35025 m** | `physxVehicleWheel:radius` |
| sprung mass | **907.2 kg** | `physics:mass` |
| centre of mass | **(−0.139, 0, 0.4)** | `physics:centerOfMass` |
| drive | rear wheels, 1000 N·m peak | `physxVehicleDriveBasic:peakTorque` |
| brakes | 1500 N·m all four / 3000 N·m rear | `physxVehicleBrakes:brakes0/1` |

Two derived numbers that matter more than they look:

**`R_min = L / tan(δ_max) = 2.78 / tan(0.5) = 5.09 m.`** This is the tightest circle
Mila can physically drive, and it decides which reference paths are trackable at all.

**CG to rear axle = 1.251 m, not 1.39 m.** The centre of mass is *not* at the
wheelbase midpoint — it sits 0.139 m behind it. Pure Pursuit's geometry is derived
about the rear axle, so converting a CG pose using `wheel_base/2` would inject a
fixed 14 cm bias into every pose the controller sees.

> ### ❓ Open question for the team
>
> **Is `mila_vehicle.usd` the canonical vehicle definition?**
>
> It lives in `autonomous-drive-stack`, **not** in `MilaMonorepo`. The monorepo's own
> stage (`FirstMilaStage_light.usda`) still loads the Buick placeholder, which has no
> physics at all. The two repos also disagree on Isaac version — **6.1** in
> `autonomous-drive-stack`, **6.0** in the monorepo.
>
> If a newer or different vehicle definition is authoritative, the six numbers in
> `VehicleParams` are the only thing that has to change — everything else derives
> from them. But every result below assumes this file is the right one.
>
> `max_speed = 15 m/s` is still a genuine placeholder; nothing in the asset sets it.

---

## 3. How it's put together, and why

```
control/
├── mila_control/          no Isaac imports — runs anywhere, in ~3 s
│   ├── path.py            reference paths, nearest-point search, feasibility
│   ├── pure_pursuit.py    the controller
│   ├── vehicle.py         kinematic bicycle (RK4) + Mila's real parameters
│   └── sim.py             closed-loop runner, logging, error metrics
├── isaac/                 the only code that knows Isaac exists
├── tools/
│   ├── verify.py          known-answer checks — run this first
│   └── plots.py           the six figures below
└── docs/figures/
```

**The controller imports nothing from Isaac Sim.** Isaac needs Ubuntu 22.04/24.04 or
Windows 11 and an NVIDIA GPU with RT cores; it does not run on macOS in any version.
Separating the layers meant the algorithm, the tests, the tuning and every figure here
got built on a machine that cannot launch the simulator.

The second benefit is diagnostic. When tracking looks wrong in a simulator, the first
question is *is the controller broken or is the integration broken?* Because the
controller is proven against analytic answers on a model I control, that question has
an answer before anyone starts guessing.

---

## 4. Deliverable 1 — open loop, no controller

Steering is a sine wave in time. No path, no feedback — the command depends only on the
clock. What it proves is the plumbing: physics ticks, steering responds, sign
conventions are what you think.

![Open loop: command, heading, and resulting path](docs/figures/01_open_loop.png)

**The thing worth stopping on.** The steering command (left) is perfectly symmetric,
±18°. The *heading* is not: it swings between −12° and +98°, mean **+44°**. So the car
staircases diagonally instead of weaving about the start line.

That isn't a bug. Heading is the *integral* of steering:

```
yaw(t) = ∫ (v/L)·tan(δ) dt
```

Starting a sine at phase zero makes that integral non-negative for the whole first
half-period, so the heading never crosses to the other side. The steering reverses
correctly throughout — which is what the task asks — but the path inherits the bias.

I had this wrong in my first draft, wrote "expect a weave about the start line" in the
docstring, generated the figure, and the figure disagreed. The dashed line is the
cosmetic fix (`center_heading=True`); the steering command is identical in both runs.

---

## 5. Deliverable 2 — Pure Pursuit

Pick a point on the path a fixed distance ahead, steer along the circular arc through
the rear axle and that point:

```
κ = 2·y_v / ld²          curvature of that arc
δ = atan(L · κ)          steering angle for wheelbase L
```

Four details decide whether it works or merely runs:

1. **The reference point is the rear axle** — the point whose velocity is always along
   the body x-axis in a bicycle model. Use the CG and you get a persistent bias that
   looks like a tuning problem. Mila's CG offset is 1.251 m (§2).
2. **Lookahead scales with speed.** `ld = 0.45·v`, clamped to [3, 15] m — §7.
3. **The goal search is forward-only *and* bounded.** Forward-only stops a
   self-crossing path jumping branches; bounded stops a closed path matching its own
   endpoint and declaring itself finished before moving.
4. **The actuator has limits.** 28.65° and 120°/s.

![Tracking on three paths](docs/figures/02_tracking.png)

- **Left — straight, started 3 m off.** One overshoot, flat by ~4 s, settles to
  0.0007 m. A second or third overshoot would mean the lookahead is too short.
- **Middle — constant-radius circle.** Error settles at a steady **+0.285 m** and stays
  there. Not noise and not a tuning failure: that is Pure Pursuit's known steady-state
  error on constant curvature, aiming slightly inside the arc.
- **Right — slalom.** Error peaks at the crests, crosses zero on the straights. That's
  corner-cutting.

**Pure Pursuit cuts corners.** Steady-state cross-track error grows with lookahead and
with curvature. It's inherent to the geometry, not a defect here, and it's one of the
concrete reasons the team wants MPC.

---

## 6. Does it work? The analytic check

Every test case has an answer known **independently of the code under test**. For a
kinematic bicycle holding a circle of radius R, the exact steering angle is:

```
δ = atan(L / R)
```

That comes from the geometry, not from my implementation.

![Measured steering vs the closed-form answer](docs/figures/03_analytic_check.png)

Measured steady-state steering tracks the analytic curve with a maximum residual below
**10⁻³ degrees** — floating-point noise. The controller reproduces the closed-form
answer rather than approximating it, and it still does so after the wheelbase changed
by 74%, which is the point of testing against a formula instead of a golden number.

| # | check | known answer | measured |
|---|---|---|---|
| 1 | straight line | zero error | 0.0000 m |
| 1b | started 3 m off | converges, no ringing | 0.0007 m, 3 reversals |
| 2 | circle R = 10/15/25/40 | `δ = atan(L/R)` | 0.000° |
| 3 | lookahead sweep | *(behavioural)* | §7 |
| 4 | figure-eight R = 30 | no branch jump | 0.740 m max |
| 5 | Ackermann split | inner > outer | holds at 5°, 15°, 25° |

```bash
uv run tools/verify.py      # ~3 s, must print "all checks passed"
```

---

## 7. Tuning the lookahead — measured, not guessed

Lookahead is the one parameter that matters. Too short and it oscillates; too long and
it cuts every corner. "Scale it with speed" is correct and unhelpfully vague about how
much — so I measured the stability boundary instead.

![Stability map over lookahead and speed](docs/figures/06_stability_map.png)

Each cell is a straight-line run started 3 m off. On a straight line a settled
controller has exactly zero error, so max |error| over the *second half* of the run is
a direct stability readout: blue converged, red never will.

| speed | boundary (smallest stable ld) | shipped `ld` | margin |
|---|---|---|---|
| 2–4 m/s | 1.5 m | 3.0 m | +1.5 m |
| 6 m/s | 2.0 m | 3.0 m | +1.0 m |
| 8 m/s | 2.0 m | 3.6 m | +1.6 m |
| 10 m/s | 3.0 m | 4.5 m | +1.5 m |
| 12 m/s | 3.0 m | 5.4 m | +2.4 m |

The boundary rises with speed, so a fixed lookahead cannot be safe across the range.
`ld_min = 3.0` clears it at every speed on its own, and `k = 0.45` keeps at least 1.0 m
of margin to 12 m/s while still giving 0.28 m RMS on the slalom.

![Lookahead sweep on the slalom](docs/figures/04_lookahead_sweep.png)

The cost side: going from `ld = 2 m` to `ld = 15 m` takes tracking error from 0.118 m
to 5.909 m while steering effort falls. You buy accuracy with actuator work.

> **A note on measuring "smoothness."** The first version of the right-hand panel
> plotted *steering reversals*. It looked like a clean trade-off until I read the axis:
> the count moved from 8 to 7 across the whole sweep. The metric was saturated — a
> smooth slalom gives one reversal per crest regardless — and the figure implied a
> relationship the data didn't contain. RMS steering rate actually varies, so that's
> what it shows now.

---

## 8. When the path is the problem

A reference path can demand a turn the vehicle cannot make:

```
R_min = L / tan(δ_max) = 2.78 / tan(0.5 rad) = 5.09 m
```

![Figure-eight feasibility](docs/figures/05_feasibility.png)

`path.check_feasible()` runs before every Isaac run and refuses to start:

```
path 'figure_eight': path needs R >= 4.18 m, vehicle can do R >= 5.09 m  <-- INFEASIBLE
REFUSING TO RUN: the path is not physically trackable by this vehicle.
```

This is the check that caught the problem in §10. Ten seconds of arithmetic instead of
an afternoon tuning something that was never trackable.

---

## 9. Code walkthrough

### `mila_control/vehicle.py` — the plant

Two wheels, one steered; no tyre slip, no load transfer. **If Pure Pursuit cannot track
a path on a bicycle model, the problem is the controller.** Isaac's PhysX adds the
dynamics afterwards.

```
x' = v·cos(θ)     y' = v·sin(θ)     θ' = (v/L)·tan(δ)     v' = a
```

| function | what it does |
|---|---|
| `VehicleParams` | Mila's real geometry (§2), each value carrying its USD attribute name. |
| `KinematicBicycle.step()` | One RK4 step. Clamps both commands to actuator limits first, wraps yaw. |
| `.ackermann_wheel_angles()` | Splits one virtual angle into left/right. A real axle can't give both wheels the same angle — the inner wheel traces a tighter circle and must turn further, or it scrubs. |
| `speed_controller()` | Deliberately trivial. The assignment is lateral control; keeping this dumb makes it obvious any tracking error is steering, not throttle. |

**Why RK4 and not Euler.** On a curved path, Euler's truncation error shows up as a
steady *outward drift* — indistinguishable from controller tracking error. RK4 is ten
extra lines and deletes that category of confusion.

### `mila_control/path.py` — the reference

| function | what it does |
|---|---|
| `.nearest_index(x, y, start, window_m=15)` | **The most load-bearing function here.** Forward-only *and* within a bounded window. Both constraints are essential — §10. |
| `.lookahead_point()` | First path point at least `ld` ahead; `None` when exhausted. |
| `.cross_track_error()` | Signed perpendicular distance. The **sign** is the diagnostic: large signed mean = systematic bias (corner-cutting, wrong reference point); near-zero signed mean with large RMS = oscillation. |
| `.check_feasible()` | Tightest path radius vs `L/tan(δ_max)`. §8. |
| `straight`, `circle`, `slalom`, `figure_eight` | Zero, constant, alternating and self-intersecting curvature. Each isolates a different failure mode. |

### `mila_control/pure_pursuit.py` — the controller

| function | what it does |
|---|---|
| `.lookahead_for_speed(v)` | `clip(0.45·v, 3, 15)`. §7. |
| `.rear_axle(..., from_cg)` | Shifts a CG pose back by **1.251 m** — Mila's real offset, not `L/2`. |
| `.step(x, y, yaw, v, dt)` | One control cycle, returns `(steer, debug)`. |
| `analytic_steer_for_radius(L, R)` | `atan(L/R)` — the known answer §6 checks against, kept beside the code it tests. |

Inside `.step()`: compute `ld` from speed → find goal and signed error → if the path is
exhausted, hold the last command and flag `finished` (requiring **both** "near the end"
and "walked to the end of the array", since a closed path's end sits on its start) →
rotate the goal into the vehicle frame → `κ = 2·y_v/ld_actual²` using the **true**
distance to the goal, not the requested `ld` → `δ = atan(L·κ)` → clamp angle, then rate.

### `isaac/` — the adapter

See §11; this layer needs rewriting for PhysX Vehicle.

---

## 10. What changed when the real parameters arrived

The vehicle was found in a repo nobody had checked. Three things came out of it.

**My placeholders were badly wrong.**

| | placeholder | actual | error |
|---|---|---|---|
| wheelbase | 1.6 m | 2.78 m | **+74%** |
| track width | 1.2 m | 1.73 m | +44% |
| wheel radius | 0.23 m | 0.350 m | +52% |
| max steer | 30° | 28.65° | −5% |

**`R_min` nearly doubled: 2.77 m → 5.09 m.** Which made two of my own test paths
undrivable:

- the **figure-eight at R = 20** needs 4.18 m — now infeasible, moved to R = 30
- the **slalom (4 m amplitude, 20 m wavelength)** needs 2.53 m — **now infeasible**

That second one matters, and it's worth being blunt about: the headline **0.406 m RMS**
in the previous version of this report was measured on a path the real Mila physically
cannot drive. The controller did what it did, but the number didn't transfer. The
slalom is now 4 m amplitude over a **40 m** wavelength (needs 10.13 m, comfortably
drivable), and the honest figure is **0.282 m RMS**.

**The lookahead had to be re-tuned.** `ld = 0.35·v` clamped to [1.5, 10] was fitted to
the 1.6 m placeholder car and is unstable on the real one at speed. The re-measured law
is `ld = 0.45·v` clamped to **[3, 15] m** (§7).

**What didn't change: the analytic check.** `δ = atan(L/R)` still matches to within
10⁻³ degrees after a 74% change in wheelbase. That's the argument for testing against a
formula rather than a stored golden number — the test was still valid when the vehicle
underneath it turned out to be a different size.

### Earlier bugs, kept for the record

**Closed paths terminated instantly.** The circle gave a constant 2.400° command at
*every* radius — impossible, since `atan(L/R)` depends on R. The forward-only search
scanned to the array end, and on a closed path the endpoint *is* the start point, so it
always won as nearest. Fixed with a bounded window plus requiring `walked_to_end`.

**R = 25 showed 21.6 m error with correct steering.** Both true: the car completed a
lap, returned to origin, and drove a second lap while error was measured against the
exhausted path's final point. Fixed by terminating on `travelled >= path.length`.

**A figure implied a trade-off the data didn't support.** §7 — the reversal metric was
saturated. Caught by reading my own axis instead of my own caption.

The pattern: every one was caught by *computing a number and comparing it to one known
in advance*, not by looking at a plot and deciding it seemed reasonable.

---

## 11. Isaac Sim integration

**Mila is drivable.** `mila_vehicle.usd` is a **PhysX Vehicle** — suspension raycasts
and wheel attachments, not a jointed articulation. That's why searching the monorepo
for `ArticulationRoot` and revolute joints found nothing: wrong mechanism entirely.

**The adapter in `isaac/` is written for the wrong interface and needs rewriting.** It
currently builds an `AckermannController` and writes joint targets. PhysX Vehicle takes
a single normalized steering input:

```python
# steer is normalized [-1, 1] and scaled by maxSteerAngle in the engine
prim.GetAttribute("physxVehicleController:steer").Set(delta / 0.5)
prim.GetAttribute("physxVehicleController:accelerator").Set(throttle)
prim.GetAttribute("physxVehicleController:brake0").Set(brake)
```

The asset already declares `physxVehicleAckermannSteering`, so the inner/outer split
happens in the engine — the `AckermannController` code path can be deleted. **This
makes the integration simpler than planned, not harder.**

Still outstanding:

- **No machine that can run Isaac Sim.** macOS is unsupported in every version.
- **Which stage?** `parking_lot.usda` (Isaac 6.1, has the vehicle, cones and collision)
  or the monorepo's `FirstMilaStage_light.usda` (Isaac 6.0, Buick placeholder)? See the
  open question in §2.
- `.usd` assets are Git LFS pointers; `stage.py` detects this and says so, because
  Isaac's own error doesn't mention LFS.

---

## 12. Toward MPC

The pieces carry over directly:

- `KinematicBicycle` becomes the **prediction model**.
- `path.py` already supplies a reference horizon.
- `VehicleParams` already encodes the **constraints** — and they're now real numbers.
- `verify.py`'s analytic cases become MPC's regression tests. Same known answers.

The comparison to run on the same slalom is **tracking error against corner-cutting**.
Pure Pursuit's signed-mean bias is structural; MPC should remove it, because it
optimises over a horizon instead of chasing a single point. Measure it rather than
assume it.

---

## 13. How to run it

```bash
cd autonomous_drive_stack/control
uv run tools/verify.py              # ~3 s, must print "all checks passed"
uv run tools/plots.py --out docs/figures
```

Isaac entry points (once a machine exists, and after the §11 rewrite):

```bash
./python.sh  isaac/run_openloop.py --list-joints   # start here on any new asset
./python.sh  isaac/run_purepursuit.py --path slalom --csv run.csv
```

`--csv` writes the same columns `RunLog` records, so an Isaac run can be compared
directly against the bicycle-model run of the same path. Divergence is the integration;
agreement means PhysX is only adding the dynamics the kinematic model deliberately
omits.
