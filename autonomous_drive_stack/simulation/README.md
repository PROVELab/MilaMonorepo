# Simulation

NVIDIA Isaac Sim setup for the PROVE autonomous EV.

## Main scene

**Open this file in Isaac Sim:** `isaac_sim/Prove_sim.usd`

This is the only scene you should use. All other `.usd` files in this directory are legacy and will be removed.

## Prerequisites

- NVIDIA Isaac Sim 6.0+ on Windows
- Git LFS enabled before cloning this repo (the `.usd` files are LFS)

## Running

1. Open Isaac Sim
2. **File → Open** → `autonomous_drive_stack/simulation/isaac_sim/Prove_sim.usd`
3. Press **Play ▶** (triangle in the top toolbar)

Scene starts publishing these ROS2 topics automatically:

| Topic | Type | Notes |
|---|---|---|
| `/camera/front/image_raw` + `/camera/front/camera_info` | sensor_msgs/Image | 120° HFOV |
| `/camera/left/image_raw` + `/camera/left/camera_info` | sensor_msgs/Image | 62° HFOV |
| `/camera/right/image_raw` + `/camera/right/camera_info` | sensor_msgs/Image | 62° HFOV |
| `/point_cloud` | sensor_msgs/PointCloud2 | Ouster OS1 LiDAR |
| `/laser_scan` | sensor_msgs/LaserScan | 2D slice |

## Drive the car manually (optional)

1. **Window → Script Editor**
2. Open `isaac_sim/car_controls.py` in the Script Editor
3. Click **Run** (▶ in the script editor, NOT the main sim play)
4. Click once in the viewport to give it focus, then:
   - **I** = forward
   - **K** = backward
   - **J** = turn left
   - **L** = turn right

## Files in this directory

- `isaac_sim/Prove_sim.usd` — **the scene**
- `isaac_sim/Sensor Placement Model (2).usd` — car CAD model (payload of Prove_sim)
- `isaac_sim/car_controls.py` — IJKL keyboard driving

## Legacy files to remove

- `FirstMilaStage_light.usda` — superseded by Prove_sim.usd
- `assets/buick/buick rivera-ANKA3DCITY_.usd` — old car model, unused
