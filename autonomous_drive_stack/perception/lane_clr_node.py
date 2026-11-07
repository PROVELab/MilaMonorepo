#!/usr/bin/env python3
"""
lane_clr_node.py -- learned lane detection (CLRerNet) -> metric lanes in base_link.

   image --resize--> [ BACKEND: CLRerNet | mock ] --lane polylines (pixels)-->
         --ground-plane projection (intrinsics + mount pose)--> per-lane polynomial fit y=f(x) in metres
         --pick ego left/right lane--> centerline  --> Path / MarkerArray / state / debug image

Only the BACKEND is a neural network. Everything after it is plain geometry that does not care
which model produced the polylines, so CLRerNet can later be swapped for DiffusionLane etc.

Backends (param `backend`):
  clrernet : real model (needs the CLRerNet checkout + weights, see README notes)
  mock     : draws two straight ground-truth lines at +/- mock.half_width metres. NO model needed.
             Use it first: if the mock lines don't land on the real road in /lane_clr/debug, your
             camera mount pose is wrong -- fix that before blaming the network.

Frame convention (base_link, REP-103): x forward, y left, z up.
Camera pose: x,y,z (m), yaw_deg (+left), pitch_deg (+nose DOWN).
"""
from __future__ import annotations

import math
import os
import sys
import tempfile
import time
from typing import List, Optional

import cv2
import numpy as np

try:                                   # ROS is optional so the maths can be unit-tested anywhere
    import rclpy
    from geometry_msgs.msg import Point, PoseStamped
    from nav_msgs.msg import Path
    from rclpy.node import Node
    from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy, qos_profile_sensor_data
    from sensor_msgs.msg import CameraInfo, Image
    from std_msgs.msg import Float32MultiArray
    from visualization_msgs.msg import Marker, MarkerArray
    HAVE_ROS = True
except ImportError:                    # pragma: no cover
    HAVE_ROS = False
    Node = object

# optical (x right, y down, z fwd) -> body (x fwd, y left, z up)
R_OPT2BODY = np.array([[0, 0, 1], [-1, 0, 0], [0, -1, 0]], float)


def _rot_y(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])


def _rot_z(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])


# =============================================================================
# Geometry
# =============================================================================
class GroundCam:
    """Pinhole camera over a flat ground plane (z=0 in base_link)."""

    def __init__(self, pos, yaw, pitch):
        self.pos = np.array(pos, float)
        self.R = _rot_z(yaw) @ _rot_y(pitch) @ R_OPT2BODY   # optical -> base_link
        self.K = None                                       # fx, fy, cx, cy

    def pixels_to_ground(self, uv: np.ndarray) -> np.ndarray:
        """(N,2) pixels -> (M,2) ground xy. Pixels at/above the horizon are dropped."""
        fx, fy, cx, cy = self.K
        d = np.stack([(uv[:, 0] - cx) / fx, (uv[:, 1] - cy) / fy, np.ones(len(uv))], 1)
        rays = d @ self.R.T
        ok = rays[:, 2] < -1e-3
        t = -self.pos[2] / rays[ok, 2]
        return self.pos[:2] + rays[ok, :2] * t[:, None]

    def ground_to_pixels(self, xy: np.ndarray) -> np.ndarray:
        """(N,2) ground xy -> (M,2) pixels (only points in front of the camera)."""
        fx, fy, cx, cy = self.K
        p = (np.c_[xy, np.zeros(len(xy))] - self.pos) @ self.R
        ok = p[:, 2] > 1e-3
        return np.stack([fx * p[ok, 0] / p[ok, 2] + cx, fy * p[ok, 1] / p[ok, 2] + cy], 1)


class GroundLane:
    """One lane fitted as lateral y = poly(x), valid for x in [x_min, x_max] (metres, base_link)."""

    def __init__(self, coef, x_min, x_max, px=None):
        self.coef, self.x_min, self.x_max, self.px = np.asarray(coef, float), x_min, x_max, px

    def y(self, x):
        return np.polyval(self.coef, x)


class LaneResult:
    def __init__(self):
        self.lanes: List[GroundLane] = []
        self.left: Optional[GroundLane] = None
        self.right: Optional[GroundLane] = None
        self.center: Optional[GroundLane] = None
        self.conf = 0.0
        self.stale = False


# =============================================================================
# Backends: the only part that touches a neural network
# =============================================================================
class MockBackend:
    """Two straight lines at +/- half_width, projected through the camera model."""

    def __init__(self, cam: GroundCam, half_width=1.75, x_min=3.0, x_max=20.0):
        self.cam, self.hw, self.x0, self.x1 = cam, half_width, x_min, x_max

    def infer(self, bgr):
        h, w = bgr.shape[:2]
        xs = np.linspace(self.x0, self.x1, 40)
        out = []
        for s in (+1.0, -1.0):
            uv = self.cam.ground_to_pixels(np.c_[xs, s * self.hw * np.ones_like(xs)])
            uv = uv[(uv[:, 0] >= 0) & (uv[:, 0] < w) & (uv[:, 1] >= 0) & (uv[:, 1] < h)]
            if len(uv) >= 2:
                out.append(uv)
        return out


class CLRerNetBackend:
    """Wraps the official CLRerNet repo (mmdetection 3.x based).

    !! Written WITHOUT being able to read the repo's inference source (GitHub blocked automated
    !! access). Assumptions, all checked by clr_probe.py before you rely on them:
    !!   * `libs.api.inference_one_image(model, img)` exists and returns (image, lanes) or lanes
    !!   * each lane is an (N,2) array of x,y pixels in the coordinates of the image you passed in
    !!   * it may only accept a file path -> 'auto' mode retries with a RAM-disk file if arrays fail
    """

    def __init__(self, repo, config, checkpoint, device='cuda:0', input_mode='auto'):
        if repo:
            sys.path.insert(0, os.path.abspath(os.path.expanduser(repo)))
        from mmdet.apis import init_detector               # mmdet 3.x
        try:
            from libs.api.inference import inference_one_image
        except ImportError:
            from libs.api import inference_one_image
        self._fn = inference_one_image
        self.model = init_detector(os.path.expanduser(config), os.path.expanduser(checkpoint), device=device)
        self.mode = input_mode
        shm = '/dev/shm'
        self.tmp = os.path.join(shm if os.path.isdir(shm) else tempfile.gettempdir(), 'clr_frame.bmp')
        self.info = ''

    @staticmethod
    def _norm(result):
        preds = result[1] if isinstance(result, tuple) and len(result) == 2 else result
        lanes = []
        for l in preds:
            a = np.asarray(l, float)
            if a.ndim == 2 and a.shape[1] == 2 and len(a) >= 2:
                lanes.append(a)
        return lanes

    def infer(self, bgr):
        if self.mode in ('auto', 'array'):
            try:
                res = self._norm(self._fn(self.model, bgr))
                self.info = f'array input OK, {len(res)} lanes'
                return res
            except Exception as e:
                if self.mode == 'array':
                    raise
                self.info = f'array input failed ({type(e).__name__}); using file input'
                self.mode = 'file'
        cv2.imwrite(self.tmp, bgr)
        return self._norm(self._fn(self.model, self.tmp))


# =============================================================================
# Pipeline: resize -> backend -> ground projection -> fits -> ego lane + centerline
# =============================================================================
DEFAULTS = dict(range_min=2.0, range_max=25.0, y_max=8.0, min_pts=8, min_span_m=3.0, poly_deg=2,
                outlier_m=0.3, ref_x=6.0, w_min=2.0, w_max=8.0, w_nom=3.5, smooth=0.5, max_lost=5)


class LanePipeline:
    def __init__(self, backend, cam: GroundCam, in_size=None, **p):
        self.backend, self.cam, self.in_size = backend, cam, in_size
        self.p = {**DEFAULTS, **p}
        self.prev: Optional[LaneResult] = None
        self.lost = 0

    def process(self, frame) -> LaneResult:
        H, W = frame.shape[:2]
        if self.in_size and tuple(self.in_size) != (W, H):
            iw, ih = self.in_size
            raw = self.backend.infer(cv2.resize(frame, (iw, ih), interpolation=cv2.INTER_LINEAR))
            scale = np.array([W / iw, H / ih])               # map model-space pixels back to the frame
            raw = [np.asarray(l, float).reshape(-1, 2) * scale for l in raw]
        else:
            raw = [np.asarray(l, float).reshape(-1, 2) for l in self.backend.infer(frame)]
        return self.postprocess(raw)

    def _fit(self, px) -> Optional[GroundLane]:
        p = self.p
        g = self.cam.pixels_to_ground(px)
        if len(g) == 0:
            return None
        g = g[(g[:, 0] >= p['range_min']) & (g[:, 0] <= p['range_max']) & (np.abs(g[:, 1]) <= p['y_max'])]
        if len(g) < p['min_pts'] or g[:, 0].max() - g[:, 0].min() < p['min_span_m']:
            return None
        x, y = g[:, 0], g[:, 1]
        deg = min(p['poly_deg'], len(g) - 1)
        coef = np.polyfit(x, y, deg)
        keep = np.abs(np.polyval(coef, x) - y) < p['outlier_m']          # one robust refit pass
        if p['min_pts'] <= keep.sum() < len(g):
            coef, x = np.polyfit(x[keep], y[keep], deg), x[keep]
        return GroundLane(coef, float(x.min()), float(x.max()), px)

    def postprocess(self, raw) -> LaneResult:
        p, res = self.p, LaneResult()
        res.lanes = [gl for gl in (self._fit(px) for px in raw) if gl is not None]

        # ego lane = nearest lane on each side of the car, judged at ref_x (clamped to what's visible)
        lat = [(float(l.y(np.clip(p['ref_x'], l.x_min, l.x_max))), l) for l in res.lanes]
        left = min([t for t in lat if t[0] > 0], key=lambda t: t[0], default=None)
        right = max([t for t in lat if t[0] < 0], key=lambda t: t[0], default=None)
        L, R = (left[1] if left else None), (right[1] if right else None)

        if L is not None and R is not None:                              # plausibility of the pair
            xa, xb = max(L.x_min, R.x_min), min(L.x_max, R.x_max)
            if xb - xa >= 2.0:
                xs = np.linspace(xa, xb, 10)
                w = L.y(xs) - R.y(xs)
                if not (w.min() > 0 and p['w_min'] <= w.mean() <= p['w_max']):
                    if (L.x_max - L.x_min) >= (R.x_max - R.x_min): R = None
                    else: L = None
            else:                                                        # no overlap: keep the longer one
                if (L.x_max - L.x_min) >= (R.x_max - R.x_min): R = None
                else: L = None

        if L is not None and R is not None:
            xa, xb = max(L.x_min, R.x_min), min(L.x_max, R.x_max)
            res.center = GroundLane((L.coef + R.coef) / 2.0, xa, xb)
            full = 1.0
        elif L is not None or R is not None:
            S, sign = (L, -1.0) if L is not None else (R, +1.0)          # centre is right of left line, left of right line
            c = S.coef.copy(); c[-1] += sign * p['w_nom'] / 2.0
            res.center = GroundLane(c, S.x_min, S.x_max)
            full = 0.5
        else:
            full = 0.0
        res.left, res.right = L, R
        if res.center is not None:
            res.conf = full * min(1.0, (res.center.x_max - res.center.x_min) / max(1e-6, p['range_max'] - p['range_min']))

        return self._smooth(res)

    def _smooth(self, res: LaneResult) -> LaneResult:
        p = self.p
        if res.center is None:
            self.lost += 1
            if self.prev is not None and self.lost <= p['max_lost']:
                self.prev.stale = True
                return self.prev
            self.prev = None
            return res
        self.lost = 0
        a = p['smooth']
        if self.prev is not None:
            for k in ('left', 'right', 'center'):
                n, o = getattr(res, k), getattr(self.prev, k)
                if n is not None and o is not None and len(n.coef) == len(o.coef):
                    n.coef = a * n.coef + (1 - a) * o.coef
        self.prev = res
        return res


# =============================================================================
# ROS 2 node
# =============================================================================
ENC_CH = {'rgb8': 3, 'bgr8': 3, 'rgba8': 4, 'bgra8': 4}


def image_to_bgr(msg) -> np.ndarray:
    enc = msg.encoding.lower()
    ch = ENC_CH.get(enc)
    if ch is None:
        raise ValueError(f'Unsupported encoding: {msg.encoding}')
    raw = np.frombuffer(msg.data, np.uint8).reshape(msg.height, msg.step)
    img = np.ascontiguousarray(raw[:, :msg.width * ch].reshape(msg.height, msg.width, ch))
    conv = {'rgb8': cv2.COLOR_RGB2BGR, 'rgba8': cv2.COLOR_RGBA2BGR, 'bgra8': cv2.COLOR_BGRA2BGR}.get(enc)
    return cv2.cvtColor(img, conv) if conv is not None else img


def to_msg(img, header):
    m = Image()
    m.header = header
    m.height, m.width = img.shape[:2]
    m.encoding, m.step = 'bgr8', m.width * 3
    m.data = img.tobytes()
    return m


class LaneCLRNode(Node):
    def __init__(self):
        super().__init__('prove_lane_clr')
        d = self.declare_parameter
        d('backend', 'mock')                       # 'mock' | 'clrernet'
        d('image_topic', '/camera/front/image_raw')
        d('info_topic', '/camera/front/camera_info')
        d('base_frame', 'base_link')
        d('rate_hz', 10.0)
        d('model.repo', '')                        # path to your CLRerNet checkout
        d('model.config', 'configs/clrernet/culane/clrernet_culane_dla34_ema.py')
        d('model.checkpoint', 'clrernet_culane_dla34_ema.pth')
        d('model.device', 'cuda:0')
        d('model.input_mode', 'auto')              # auto | array | file
        d('model.in_w', 1640); d('model.in_h', 590)    # CULane geometry the model was trained on (0 = native)
        d('mock.half_width', 1.75)
        for k, v in dict(x=1.0, y=0.0, z=1.0, yaw_deg=0.0, pitch_deg=15.0).items():   # PLACEHOLDER pose
            d(f'camera.{k}', v)
        for k, v in DEFAULTS.items():
            d(k, v)

        g = lambda k: self.get_parameter(k).value
        self.base = g('base_frame')
        self.period = 1.0 / float(g('rate_hz'))
        self.cam = GroundCam((g('camera.x'), g('camera.y'), g('camera.z')),
                             math.radians(g('camera.yaw_deg')), math.radians(g('camera.pitch_deg')))
        backend_name = g('backend')
        if backend_name == 'mock':
            backend, in_size = MockBackend(self.cam, g('mock.half_width')), None
        elif backend_name == 'clrernet':
            self.get_logger().info('Loading CLRerNet (first start compiles/loads CUDA ops, be patient)...')
            backend = CLRerNetBackend(g('model.repo'), g('model.config'), g('model.checkpoint'),
                                      g('model.device'), g('model.input_mode'))
            in_size = (int(g('model.in_w')), int(g('model.in_h'))) if g('model.in_w') else None
        else:
            raise ValueError(f"backend must be 'mock' or 'clrernet', got {backend_name!r}")
        self.backend = backend
        self.pipe = LanePipeline(backend, self.cam, in_size, **{k: g(k) for k in DEFAULTS})
        self.info_size = self.k_raw = self.geom_size = None
        self._last = 0.0
        self._ms, self._n_lanes, self._frames = [], 0, 0
        self._warmed = False

        qos = QoSProfile(reliability=ReliabilityPolicy.BEST_EFFORT, history=HistoryPolicy.KEEP_LAST, depth=1)
        self.create_subscription(CameraInfo, g('info_topic'), self._on_info, qos_profile_sensor_data)
        self.create_subscription(Image, g('image_topic'), self._on_image, qos)
        self.path_pub = self.create_publisher(Path, '/lane_clr/centerline', 10)
        self.mark_pub = self.create_publisher(MarkerArray, '/lane_clr/lanes', 10)
        self.state_pub = self.create_publisher(Float32MultiArray, '/lane_clr/state', 10)
        self.dbg_pub = self.create_publisher(Image, '/lane_clr/debug', 1)
        self.create_timer(5.0, self._log_stats)
        self.get_logger().info(f'backend={backend_name}; waiting for CameraInfo + first image...')

    def _on_info(self, m):
        self.info_size = (m.width, m.height)
        self.k_raw = (m.k[0], m.k[4], m.k[2], m.k[5])

    def _on_image(self, msg):
        now = self.get_clock().now().nanoseconds * 1e-9
        if self.info_size is None or now - self._last < self.period:
            return
        self._last = now
        try:
            img = image_to_bgr(msg)
        except Exception as e:
            self.get_logger().error(f'image conversion failed: {e}')
            return
        size = (msg.width, msg.height)
        if size != self.geom_size:                      # (re)scale intrinsics to the actual image size
            sx, sy = size[0] / self.info_size[0], size[1] / self.info_size[1]
            fx, fy, cx, cy = self.k_raw
            self.cam.K = (fx * sx, fy * sy, cx * sx, cy * sy)
            self.geom_size = size
        if not self._warmed:                            # first CUDA call is slow; keep it out of the stats
            try:
                self.pipe.process(img)
            except Exception:
                pass
            self._warmed = True
            info = getattr(self.backend, 'info', '')
            if info:
                self.get_logger().info(f'backend: {info}')
        t0 = time.perf_counter()
        try:
            res = self.pipe.process(img)
        except Exception as e:
            self.get_logger().error(f'inference/post-process failed: {type(e).__name__}: {e}',
                                    throttle_duration_sec=5.0)
            return
        self._ms.append((time.perf_counter() - t0) * 1e3)
        self._frames += 1
        self._n_lanes += len(res.lanes)
        self._publish(res, msg.header, img)

    def _log_stats(self):
        if self._frames:
            self.get_logger().info(f'[lane_clr] {self._frames/5.0:.1f} fps | total {np.mean(self._ms):.1f} ms '
                                   f'(max {np.max(self._ms):.0f}) | {self._n_lanes/self._frames:.1f} lanes/frame')
        self._ms, self._n_lanes, self._frames = [], 0, 0

    # ------------------------------------------------------------------ out --
    def _publish(self, r: LaneResult, header, img):
        p = self.pipe.p
        stamp = header.stamp
        path = Path()
        path.header.frame_id, path.header.stamp = self.base, stamp
        if r.center is not None:
            xs = np.linspace(r.center.x_min, r.center.x_max, 25)
            for x, y in zip(xs, r.center.y(xs)):
                ps = PoseStamped()
                ps.header = path.header
                ps.pose.position.x, ps.pose.position.y = float(x), float(y)
                ps.pose.orientation.w = 1.0
                path.poses.append(ps)
        self.path_pub.publish(path)

        # same layout as /lane_lines/state so downstream can switch lane sources freely:
        # [lateral_offset_m (+ = lane centre is LEFT of car), heading_err_rad, curvature_1pm, confidence]
        st = Float32MultiArray()
        if r.center is not None:
            c = r.center.coef
            curv = float(2 * c[-3]) if len(c) >= 3 else 0.0
            st.data = [float(np.polyval(c, 0.0)), math.atan(float(np.polyval(np.polyder(c), 0.0))), curv, float(r.conf)]
        else:
            st.data = [float('nan')] * 3 + [0.0]
        self.state_pub.publish(st)

        ma = MarkerArray()
        wipe = Marker(); wipe.action = Marker.DELETEALL; ma.markers.append(wipe)

        def strip(lane, mid, rgb, w):
            xs = np.linspace(lane.x_min, lane.x_max, 25)
            mk = Marker()
            mk.header.frame_id, mk.header.stamp = self.base, stamp
            mk.ns, mk.id, mk.type = 'clr_lanes', mid, Marker.LINE_STRIP
            mk.scale.x = w
            mk.color.r, mk.color.g, mk.color.b, mk.color.a = (*rgb, 1.0)
            mk.points = [Point(x=float(x), y=float(y), z=0.02) for x, y in zip(xs, lane.y(xs))]
            return mk

        for i, ln in enumerate(r.lanes):
            if ln is not r.left and ln is not r.right:
                ma.markers.append(strip(ln, 10 + i, (0.6, 0.6, 0.6), 0.05))
        if r.left:   ma.markers.append(strip(r.left, 1, (1.0, 0.2, 0.2), 0.10))
        if r.right:  ma.markers.append(strip(r.right, 2, (0.2, 0.6, 1.0), 0.10))
        if r.center: ma.markers.append(strip(r.center, 3, (0.1, 1.0, 0.1), 0.10))
        self.mark_pub.publish(ma)

        if self.dbg_pub.get_subscription_count():
            self.dbg_pub.publish(to_msg(self._overlay(r, img), header))

    def _overlay(self, r: LaneResult, img):
        out = img.copy()
        for ln in r.lanes:
            col = (0, 0, 255) if ln is r.left else (255, 150, 0) if ln is r.right else (0, 220, 255)
            cv2.polylines(out, [ln.px.astype(np.int32).reshape(-1, 1, 2)], False, col,
                          3 if col != (0, 220, 255) else 2)
        if r.center is not None:
            xs = np.linspace(r.center.x_min, r.center.x_max, 40)
            uv = self.cam.ground_to_pixels(np.c_[xs, r.center.y(xs)])
            if len(uv) >= 2:
                cv2.polylines(out, [uv.astype(np.int32).reshape(-1, 1, 2)], False, (0, 255, 0), 3)
        txt = f'lanes {len(r.lanes)}  conf {r.conf:.2f}' + ('  (HELD)' if r.stale else '')
        cv2.putText(out, txt, (15, 35), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 255), 2)
        return out


def main(args=None):
    rclpy.init(args=args)
    node = LaneCLRNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
