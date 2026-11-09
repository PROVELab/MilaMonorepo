# Perception

YOLOv8 object detection + CLRerNet lane detection + live web viewer for the PROVE sim.

## Quick start

With Isaac Sim already running (see `../simulation/README.md`), on a Linux machine with ROS2 Humble on the same network:

```bash
source /opt/ros/humble/setup.bash
python3 launch_perception.py
```

Open the printed URL in a browser (e.g. `http://192.168.100.2:8765`). You'll see a 3-camera grid with YOLO bounding boxes live.

## What this does

- Subscribes to `/camera/{front,left,right}/image_raw` from Isaac Sim
- Runs YOLOv8n on each frame
- Publishes `vision_msgs/Detection2DArray` on `/detections/{front,left,right}`
- Runs CLRerNet lane detection on the front camera, publishes centerline + lane markers
- Serves a multipart-JPEG web stream on port **8765** showing the 3 feeds with YOLO boxes overlaid

## Prerequisites

- ROS2 Humble sourced
- Python 3.10+
- `pip install ultralytics opencv-python numpy`
- CUDA-capable GPU recommended (works on CPU, slow)
- For `clrernet` backend only: clone of CLRerNet + weights (see `lane_clr_node.py` docstring). The default `mock` backend needs nothing extra.

## Files

- `launch_perception.py` — **run this**; orchestrates the three nodes below
- `detector_node.py` — ROS2 node, YOLO → Detection2DArray per camera
- `lane_clr_node.py` — ROS2 node, CLRerNet lane detection; subscribes `/camera/front/image_raw`, publishes:
  - `/lane_clr/centerline` (nav_msgs/Path) — drivable centerline
  - `/lane_clr/lanes` (visualization_msgs/MarkerArray) — lane polylines
  - `/lane_clr/state` (std_msgs/Float32MultiArray) — lateral + heading error
  - `/lane_clr/debug` (sensor_msgs/Image) — overlay for debugging
- `three_camera_viewer.py` — web viewer, 3-cam grid + YOLO boxes, port 8765
- `yolov8n.pt` — YOLO weights (auto-downloaded on first run)

## Lane detection backend

`lane_clr_node.py` has two backends set via the `backend` param:

- `mock` (default) — draws two straight ground-truth lines, no model required. Use first to verify camera mount pose is correct.
- `clrernet` — real CLRerNet neural net; needs the CLRerNet checkout + weights (see node docstring).

To switch backends, pass the param on launch, e.g.:
```bash
ros2 run <...> lane_clr_node --ros-args -p backend:=clrernet
```

## Topics summary

**Subscribes:**
- `/camera/{front,left,right}/image_raw`
- `/camera/{front,left,right}/camera_info`

**Publishes:**
- `/detections/{front,left,right}` (vision_msgs/Detection2DArray)
- `/lane_clr/centerline` (nav_msgs/Path)
- `/lane_clr/lanes` (visualization_msgs/MarkerArray)
- `/lane_clr/state` (std_msgs/Float32MultiArray)
- `/lane_clr/debug` (sensor_msgs/Image)

## Ports

- `8765` — web viewer. Binds to `0.0.0.0`, so reachable from any machine on your network.
