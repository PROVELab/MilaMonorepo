# Lateral control for Mila — Pure Pursuit

**Chris** · `user/chris/pure-pursuit` · October 2026
Covers the **Oct 4** (open-loop) and **Oct 9** (Pure Pursuit) deliverables, and sets up **Oct 20** (MPC).

Destination: `autonomous_drive_stack/control/`, per that directory's README —
*"Everything that will run on the AGX Orin, or is part of Isaac Sim Simulation belongs here."*

---

## Contents

1. [Summary](#1-summary)
2. [How it's put together, and why](#2-how-its-put-together-and-why)
3. [Deliverable 1 — open loop, no controller (Oct 4)](#3-deliverable-1--open-loop-no-controller-oct-4)
4. [Deliverable 2 — Pure Pursuit (Oct 9)](#4-deliverable-2--pure-pursuit-oct-9)
5. [Does it actually work? The analytic check](#5-does-it-actually-work-the-analytic-check)
6. [Tuning the lookahead — measured, not guessed](#6-tuning-the-lookahead--measured-not-guessed)
7. [When the path is the problem, not the controller](#7-when-the-path-is-the-problem-not-the-controller)
8. [Code walkthrough — every function, what it does](#8-code-walkthrough--every-function-what-it-does)
9. [Isaac Sim integration, and three things blocking it](#9-isaac-sim-integration-and-three-things-blocking-it)
10. [What I got wrong while building this](#10-what-i-got-wrong-while-building-this)
11. [Toward MPC](#11-toward-mpc)
12. [How to run it](#12-how-to-run-it)

---

## 1. Summary

A Pure Pursuit lateral controller, implemented from the geometry, verified against
closed-form answers, and wrapped in two Isaac Sim 6.0 entry points.

| | |
|---|---|
| Steering on a constant-radius circle vs `δ = atan(L/R)` | matches to **4×10⁻⁵ °** across R = 8…50 m |
| Straight line, started on the path | **0.000 m** max error |
| Straight line, started 3 m off | settles to **0.002 m**, 4 reversals |
| Slalom, 4 m amplitude at 6 m/s | **0.406 m** RMS |
| Figure-eight through its own crossing | **0.362 m** max, no branch jump |
| Runtime of the full check suite | ~2 s on a laptop, no GPU |

The honest caveat up front: **none of this has run inside Isaac Sim yet.** Section 9
explains exactly why, and all three reasons are repo facts rather than excuses.

---

## 2. How it's put together, and why

```
control/
├── mila_control/          ← no Isaac imports. Runs anywhere, in ~2 s.
│   ├── path.py            reference paths, nearest-point search, feasibility
│   ├── pure_pursuit.py    the controller itself
│   ├── vehicle.py         kinematic bicycle (RK4) + Ackermann split
│   └── sim.py             closed-loop runner, logging, error metrics
├── isaac/                 ← the only code that knows Isaac exists
│   ├── stage.py           world setup, joint resolution, LFS check
│   ├── run_openloop.py      Oct 4 deliverable
│   └── run_purepursuit.py   Oct 9 deliverable
├── tools/
│   ├── verify.py          known-answer checks — run this first
│   └── plots.py           the six figures in this report
└── docs/figures/
```

**The one decision everything else follows from: the controller does not import Isaac Sim.**

Two reasons.

*The practical one.* Isaac Sim needs Ubuntu 22.04/24.04 or Windows 11 and an NVIDIA
GPU with RT cores — RTX 4080 / 16 GB minimum. **It does not run on macOS in any
version.** I develop on a MacBook. Splitting the layers meant roughly 80% of this
work — the algorithm, the tests, the tuning, every figure below — got done on a
machine that cannot launch the simulator at all. If I had started by trying to get
Isaac running, I would have nothing.

*The engineering one.* When tracking looks wrong in a simulator, the first question
is always *is the controller broken, or is the integration broken?* Because the
controller is already proven against analytic answers on a model I control, that
question has an answer before anyone starts guessing. The Isaac layer is ~120 lines
of adapter; if the car misbehaves there, the bug is in those 120 lines.

The same principle carried over from my perception work last term: **build the
measurement harness before the thing being measured.**

---

## 3. Deliverable 1 — open loop, no controller (Oct 4)

> *"Simulate a car turning back and forth in Isaac Sim without a proper controller
> on top. Use the minimum number of parameters needed for basic simulation."*

Steering is a sine wave in time. There is no path, no feedback, no error signal —
the command depends only on the clock:

```python
steer = amplitude * sin(2*pi*t / period)
```

That is the whole thing, and that is the point. What it proves is the plumbing:
physics ticks, the steering joints respond, the drive joints spin, and the sign
conventions are what you think they are. Everything in section 4 sits on top of it.

**The minimum parameter set.** The task asked for the minimum, so:

| parameter | value | why it's needed |
|---|---|---|
| `wheel_base` | 1.6 m | sets the turn geometry — `yaw' = (v/L)·tan(δ)` |
| `track_width` | 1.2 m | only to split one steering angle into two wheel angles |
| `wheel_radius` | 0.23 m | converts forward speed to a wheel spin rate |
| `amplitude`, `period` | 18°, 6 s | the steering wave |
| `speed` | 5 m/s | constant |
| `physics_dt` | 1/60 s | integration step |

Mass, inertia and tyre parameters are **not** in that list, and that is deliberate.
They only matter once you care about forces, which a kinematic model does not. Adding
them would be answering a question nobody asked.

![Open loop: command, heading, and resulting path](docs/figures/01_open_loop.png)

**The thing worth stopping on.** Look at the middle panel. The steering command
(left) is perfectly symmetric — ±18°. The *heading* is not: it swings between −12°
and +98°, with a mean of **+44°**. So the car staircases diagonally away instead of
weaving about the start line.

That is not a bug. Heading is the *integral* of steering:

```
yaw(t) = ∫ (v/L)·tan(δ) dt
```

Starting a sine at phase zero makes that integral non-negative for the entire first
half-period, so the heading never crosses to the other side. The steering reverses
correctly throughout — which is exactly what the task asks for — but the path
inherits the heading bias.

I had this wrong in my first draft; I wrote "expect the trajectory to weave about the
start line" in the docstring, generated the figure, and the figure disagreed with me.
The dashed blue line shows the cosmetic fix (`center_heading=True`, which starts the
vehicle at minus the mean heading). The steering command is byte-identical in both
runs.

---

## 4. Deliverable 2 — Pure Pursuit (Oct 9)

The whole algorithm is one picture: **pick a point on the path a fixed distance
ahead, then steer along the unique circular arc that passes through the rear axle
and that point.** Everything else is bookkeeping.

With the goal expressed in the vehicle frame as `(x_v, y_v)`:

```
κ = 2·y_v / ld²          curvature of that arc
δ = atan(L · κ)          steering angle for wheelbase L
```

Two lines of maths. The engineering is in four details, each of which changes whether
it *works* or merely *runs*:

**1. The reference point is the rear axle.** Not the centre of mass, not the front
axle. The derivation puts the vehicle frame's origin at the rear axle because that is
the point whose velocity is always along the body x-axis in a bicycle model. Use the
CG and the car runs with a persistent cross-track bias that looks exactly like a
tuning problem and isn't. Isaac's articulation root is usually near the centre, so
`run_purepursuit.py` has an explicit `--pose-at-cg` flag rather than a silent guess.

**2. Lookahead scales with speed.** `ld = 0.35·v`, clamped to [1.5, 10] m. Section 6
is the evidence for that number.

**3. The goal search is forward-only *and* bounded.** Forward-only stops a
self-crossing path from jumping branches at the crossing. Bounded stops a *closed*
path from matching its own endpoint. Section 10 is what happened when it wasn't.

**4. The actuator has limits.** 30° of travel, 120°/s of slew. Without them you get a
controller that is flawless in simulation and saturates on the car.

![Tracking on three paths](docs/figures/02_tracking.png)

Reading the three columns:

- **Left — straight line, started 3 m off.** The approach transient. It overshoots to
  −1.8 m, comes back, and is flat by t = 3 s. One overshoot and done is what you want;
  a second or third would mean the lookahead is too short. (The 0.924 m RMS in the
  title includes the whole transient — the settled error is 0.0017 m.)

- **Middle — constant-radius circle.** The error settles at a steady **+0.137 m** and
  stays there. That offset is not noise and not a tuning failure: it is Pure Pursuit's
  known steady-state error on constant curvature. The chord-based geometry always aims
  slightly inside the arc.

- **Right — slalom.** Error peaks at ±0.875 m at the crests and crosses zero on the
  straights. That is **corner-cutting**, and it is the characteristic weakness of the
  algorithm.

**State this before someone asks you: Pure Pursuit cuts corners.** The steady-state
cross-track error grows with lookahead and with path curvature. It is inherent to the
geometry, not a defect in this implementation, and it is one of the concrete reasons
the team wants MPC.

---

## 5. Does it actually work? The analytic check

This is the part of the work I would want a reviewer to look at first.

A controller that produces smooth plots and plausible numbers can still be wrong, and
you will not notice. So every test case has an answer that is known **independently of
the code under test**. For a kinematic bicycle holding a circle of radius R, the exact
steering angle is:

```
δ = atan(L / R)
```

That comes from the geometry, not from my implementation. If the closed-loop
controller settles anywhere else, something is broken.

![Measured steering vs the closed-form answer](docs/figures/03_analytic_check.png)

Measured steady-state steering tracks the analytic curve across R = 8…50 m with a
maximum residual of **4×10⁻⁵ degrees**. That is floating-point noise. The controller
is reproducing the closed-form answer, not approximating it.

`tools/verify.py` runs five such groups and exits non-zero on any failure:

| # | check | known answer | measured |
|---|---|---|---|
| 1 | straight line | zero error | 0.0000 m max |
| 1b | started 3 m off | converges, no ringing | 0.0017 m settled, 4 reversals |
| 2 | circle, R = 10/15/25/40 | `δ = atan(L/R)` | within 4×10⁻⁵ ° |
| 3 | lookahead sweep | *(behavioural, printed)* | see §6 |
| 4 | figure-eight crossing | must not jump branches | 0.362 m max |
| 5 | Ackermann split | inner angle > outer | holds at 5°, 15°, 25° |

Run it before trusting any plot in this document:

```bash
uv run tools/verify.py      # ~2 s
```

---

## 6. Tuning the lookahead — measured, not guessed

Lookahead distance `ld` is the one parameter that matters. Too short and the
controller oscillates; too long and it cuts every corner. The usual advice is "scale
it with speed," which is correct and unhelpfully vague about *how much*.

So I measured the stability boundary instead of asserting it.

![Stability map over lookahead and speed](docs/figures/06_stability_map.png)

**How to read this.** Each cell is a straight-line run started 3 m off the path. On a
straight line a settled controller has exactly zero error, so the maximum |error| over
the *second half* of the run is a direct stability readout: blue means it converged,
red means it is still oscillating and never will.

The unstable region is the bottom-right — **short lookahead at speed**. And the
boundary *rises with speed*: `ld = 1.5 m` is fine at 4 m/s and unstable at 10 m/s. A
fixed lookahead cannot be safe across the whole speed range, which is the entire
justification for `ld = k·v`. The black line is the shipped law, and it clears the
boundary with margin everywhere.

Concretely, from the straight-line sweep:

| ld (m) | 4 m/s | 8 m/s | 12 m/s |
|---|---|---|---|
| 1.0 | **unstable** (2.54 m) | **unstable** (5.18 m) | **unstable** (5.70 m) |
| 2.0 | settles | settles | **unstable** (5.60 m) |
| 4.0 | settles | settles | settles |

The other half of the trade-off is what you pay for accuracy:

![Lookahead sweep on the slalom](docs/figures/04_lookahead_sweep.png)

On the slalom at 6 m/s, going from `ld = 1 m` to `ld = 14 m` takes tracking error from
0.08 m to 8.1 m, while steering effort falls from 33.6 °/s to 10.2 °/s RMS. You buy
accuracy with actuator work. `k = 0.35` sits where the error is still small and the
run stays comfortably inside the stable region.

> **A note on measuring "smoothness."** My first version of the right-hand panel
> plotted *steering reversals* against lookahead. It looked like a clean trade-off
> until I read the axis: the count only moved from 8 to 7 across the whole sweep. The
> metric was saturated — a smooth slalom produces one reversal per crest no matter
> what the controller does — and the figure was implying a relationship the data did
> not contain. RMS steering rate actually varies with the parameter, so that is what
> the figure shows now.

---

## 7. When the path is the problem, not the controller

A reference path can demand a turn the vehicle physically cannot make. The limit is:

```
R_min = L / tan(δ_max) = 1.6 / tan(30°) = 2.77 m
```

![Figure-eight feasibility](docs/figures/05_feasibility.png)

A figure-eight with 12 m lobes requires a 2.51 m turn radius at its tightest point.
The car can do 2.77 m. **No controller can track that path** — not Pure Pursuit, not
MPC, not a human driver.

I found this the expensive way (section 10). Now `path.check_feasible()` runs before
every Isaac run and refuses to start:

```
path 'figure_eight': 73.2 m, path needs R >= 2.51 m, vehicle can do R >= 2.77 m  <-- INFEASIBLE
REFUSING TO RUN: the path is not physically trackable by this vehicle.
```

Ten seconds of arithmetic instead of an afternoon of tuning.

---

## 8. Code walkthrough — every function, what it does

### `mila_control/vehicle.py` — the plant

The model the controller is tested against. Two wheels, one steered; no tyre slip, no
load transfer. That simplicity is the point: **if Pure Pursuit cannot track a path on
a bicycle model, the problem is the controller.** Get it right here, then let Isaac's
PhysX add the dynamics.

State, all at the rear axle:

```
x'     = v·cos(θ)
y'     = v·sin(θ)
θ'     = (v/L)·tan(δ)
v'     = a
```

| function | what it does |
|---|---|
| `VehicleParams` | The six numbers from §3. Defaults are **placeholders** — replace with Mila's measured geometry. |
| `VehicleState` | `x, y, yaw, v` at the rear axle, world frame. |
| `KinematicBicycle.step(steer, accel, dt)` | One integration step. Clamps both commands to the actuator limits first, integrates with RK4, wraps yaw to (−π, π]. |
| `.front_axle()` | Front axle position, for drawing. |
| `.ackermann_wheel_angles(steer)` | Splits one virtual bicycle angle into left and right wheel angles. A real axle cannot give both wheels the same angle — the inner wheel traces a tighter circle and must turn further, or it scrubs. Isaac's `AckermannController` does this internally; this exists so the two models agree on what `steer` means. |
| `speed_controller(v, v_target)` | Deliberately trivial proportional throttle. The assignment is about *lateral* control; keeping this dumb makes it obvious that any tracking error is the steering, not the throttle. |

**Why RK4 and not forward Euler.** On a curved path, Euler's truncation error shows up
as a steady *outward drift* — which looks exactly like controller tracking error. You
can lose an afternoon tuning a controller that was already correct. RK4 is ten extra
lines and deletes that whole category of confusion.

### `mila_control/path.py` — the reference

| function | what it does |
|---|---|
| `Path` | Points, cumulative arc length, point spacing. |
| `.nearest_index(x, y, start, window_m=15)` | **The most load-bearing function in the package.** Finds the closest path point, searching *forward only* from the last index and *only within a 15 m window*. Both constraints are essential — §10. |
| `.lookahead_point(x, y, ld, start)` | First path point at least `ld` ahead. Returns `None` when the path is exhausted. |
| `.cross_track_error(x, y, start)` | Signed perpendicular distance. The **sign** is the useful part: a large signed mean means systematic bias (corner-cutting, wrong reference point); near-zero signed mean with large RMS means oscillation. Different diagnoses, different fixes. |
| `.curvature()`, `.min_turn_radius()` | Discrete curvature from three consecutive points; the tightest radius anywhere on the path. |
| `.check_feasible(wheel_base, max_steer)` | Compares that against `L/tan(δ_max)`. Returns `(bool, message)`. §7. |
| `straight`, `circle`, `slalom`, `figure_eight` | Stock test paths: zero curvature, constant curvature, alternating curvature, self-intersecting. Each isolates a different failure mode. |

### `mila_control/pure_pursuit.py` — the controller

| function | what it does |
|---|---|
| `PurePursuitConfig` | Wheelbase, the `k/ld_min/ld_max` lookahead law, steering angle and rate limits, goal tolerance. |
| `.lookahead_for_speed(v)` | `clip(k·|v|, ld_min, ld_max)`. §6. |
| `.rear_axle(x, y, yaw, from_cg)` | Shifts a CG pose back half a wheelbase. Needed because Isaac reports the articulation root, which is usually at the centre. |
| `.step(x, y, yaw, v, dt)` | One control cycle. Returns `(steer, debug)`. |
| `analytic_steer_for_radius(L, R)` | `atan(L/R)` — the known answer §5 checks against. Kept here so the test and the claim live together. |

Inside `.step()`, in order:

1. Compute `ld` from current speed.
2. Find the goal point and the signed cross-track error.
3. **If the path is exhausted**, hold the last steering command and flag `finished` — but only if *both* "near the end point" **and** "the search walked to the end of the array" are true. Proximity alone is wrong on a closed path, whose end sits on its start. §10.
4. Rotate the goal into the vehicle frame by `−yaw`.
5. `κ = 2·y_v/ld_actual²`, using the **true** distance to the goal rather than the requested `ld` — the goal is the first point *at least* `ld` away, so on a coarse path it can be noticeably further and the curvature would come out wrong.
6. `δ = atan(L·κ)`, then clamp to the angle limit, then clamp to the rate limit.

### `mila_control/sim.py` — the harness

| function | what it does |
|---|---|
| `RunLog` | Per-step time, pose, speed, steering (raw and limited), lookahead, cross-track error, goal point. |
| `.xte_rms`, `.xte_max` | Standard error magnitudes. |
| `.xte_mean_signed` | The diagnostic one — see `cross_track_error` above. |
| `.steer_reversals` | Cheap oscillation detector. Saturates on smooth paths; §6 explains why I stopped plotting it. |
| `run(path, ...)` | Closed loop. `start_offset` places the car N metres to the side, which is how you see the approach transient — a run starting perfectly on the path hides it completely. Terminates on `travelled >= path.length`. |
| `run_open_loop(...)` | The Oct 4 task: sinusoidal steering, no feedback. `center_heading=True` removes the heading bias from §3. |

### `isaac/` — the adapter

| file | what it does |
|---|---|
| `stage.py: check_lfs(path)` | Detects an un-pulled Git LFS pointer. Isaac's failure mode for this is an obscure USD parse error that never mentions LFS. |
| `stage.py: make_world(dt)` | `World` + default ground plane. |
| `stage.py: load_vehicle(world, which)` | `leatherback` or `mila`. See §9. |
| `stage.py: resolve_joints(...)` | Maps joint names to indices, **refusing to continue** if a name is missing. A wrong joint name fails *silently* — the car simply never steers, no exception — so this prints the asset's real joint names and stops. |
| `stage.py: quat_to_yaw(q)` | Isaac returns `(w, x, y, z)`; scipy and ROS use `(x, y, z, w)`. Swap them and yaw looks nearly right near zero and diverges as the car turns. |
| `run_openloop.py` | Oct 4 entry point. |
| `run_purepursuit.py` | Oct 9 entry point. Checks feasibility, reads pose → `controller.step()` → joint targets. Finite-differences speed from position rather than trusting wheel rate × radius, which overstates ground speed whenever a wheel slips — and would inflate `ld` at exactly the wrong moment. |

**One ordering rule that breaks every first Isaac script:** `SimulationApp` must be
constructed before *any* `isaacsim.*` import, because it is what boots the Kit runtime
those extensions live in. Import earlier and you get a bare `ModuleNotFoundError` that
says nothing about ordering. Both entry points parse arguments, construct
`SimulationApp`, and only then import anything else. Also note the namespace changed
at Isaac 4.5 — anything you find online on `omni.isaac.*` is older and will not import
on 6.0.

---

## 9. Isaac Sim integration, and three things blocking it

All three are facts I found by reading the repo, and all three need someone else's
decision.

### 9.1 The Mila Buick has no articulation

`autonomous_drive_stack/simulation/FirstMilaStage_light.usda` loads
`assets/buick/buick rivera-ANKA3DCITY_.usd` as a **payload reference** — a visual
mesh. The stage has a `PhysicsScene`, a `ground` Xform, an Ouster OS1 lidar and a ROS
lidar OmniGraph. But grepping it for `PhysicsRevoluteJoint`, `ArticulationRoot`, or
any wheel or steering joint returns **nothing**.

**There is nothing to send joint targets to.** The Buick as committed cannot be
driven. Someone has to rig it with a physics articulation or a PhysX Vehicle before
any closed-loop control can run on it.

Until then `--vehicle leatherback` uses NVIDIA's Ackermann-steered sample so the
controller can be demonstrated, and swapping back is one flag. (Carter and Jetbot are
differential drive — they cannot represent a steering angle at all and are the wrong
fallback.)

### 9.2 The `.usd` assets are Git LFS pointers

`.gitattributes` tracks `*.usd` through LFS. In a fresh clone the Buick file is 133
bytes of pointer text. `git lfs install && git lfs pull` fixes it; `stage.py` checks
explicitly and says so, because Isaac's own error message does not.

### 9.3 No machine that can run Isaac Sim

macOS is unsupported in every version. This is the real schedule risk, and it is
exactly why the package is built not to need one. **This is what I need help with —
either a lab machine, or confirmation of the GPU requirement for a build of my own.**

What's ready the moment a machine exists: both entry points, argument-complete, with
`--list-joints` as the first thing to run on any new asset.

---

## 10. What I got wrong while building this

Four bugs worth recording, because three of them produced output that looked fine.

**Closed paths terminated instantly.** The circle test gave a constant 2.400° steering
command at *every* radius — which is impossible, since `atan(L/R)` depends on R. Cause:
the forward-only nearest-point search scanned to the end of the array, and on a closed
path the endpoint *is* the start point, so it always won as nearest. The lookahead
search then returned `None` and the controller declared itself finished before moving.
Fixed with a bounded search window plus requiring `walked_to_end` for the finish flag.
After the fix, the circle matched analytic to 0.000°.

**R = 25 m showed 21.6 m error with correct steering.** Both facts were true. The car
completed one lap, returned to the origin, and kept driving a second lap while error
was still measured against the exhausted path's final point. Fixed by terminating on
`travelled >= path.length`.

**The figure-eight diverged 161 m.** This looked unambiguously like a broken
controller. It wasn't — at 12 m lobes the path demands a 2.51 m turn radius and the
car can manage 2.77 m. The *reference* was impossible. That's where
`check_feasible()` came from (§7).

**A figure implied a trade-off the data didn't support.** Covered in §6 — the
reversal-count metric was saturated. Caught by reading my own axis instead of my own
caption.

The pattern: in every case the thing that caught it was *computing a number and
comparing it to one known in advance*, not looking at a plot and deciding it seemed
reasonable.

---

## 11. Toward MPC (Oct 20)

The pieces carry over directly:

- `KinematicBicycle` becomes the **prediction model** inside the optimiser.
- `path.py` already supplies a reference horizon via `lookahead_point` and arc length.
- `VehicleParams` already encodes the **constraints** — steering angle, slew rate, acceleration — which is what MPC needs and Pure Pursuit merely clamps after the fact.
- `verify.py`'s analytic cases become MPC's regression tests. Same known answers.

The comparison to run on the same slalom is **tracking error against corner-cutting**.
Pure Pursuit's signed-mean bias is structural; MPC should remove it, because it
optimises over a horizon instead of chasing a single point. That difference is the
argument for the added complexity, and it should be measured rather than assumed.

---

## 12. How to run it

**Without a GPU** — the controller, the tests, the figures:

```bash
cd autonomous_drive_stack/control
uv run tools/verify.py              # ~2 s, must print "all checks passed"
uv run tools/plots.py --out docs/figures
```

**In Isaac Sim** — use Isaac's bundled interpreter, not a system Python:

```bash
# Always start here on a new asset: prints joint names and exits
./python.sh  .../control/isaac/run_openloop.py --list-joints

# Oct 4
./python.sh  .../control/isaac/run_openloop.py

# Oct 9
./python.sh  .../control/isaac/run_purepursuit.py --path slalom --csv run.csv
```

Useful flags: `--headless` (CI or SSH), `--vehicle mila|leatherback`, `--pose-at-cg`
(if the articulation root is at the centre), `--k-lookahead` / `--ld-min` / `--ld-max`.

`--csv` writes the same columns `RunLog` records, so an Isaac run can be compared
directly against the bicycle-model run of the same path. Divergence between the two is
the integration; agreement means PhysX is only adding the dynamics the kinematic model
deliberately omits.
