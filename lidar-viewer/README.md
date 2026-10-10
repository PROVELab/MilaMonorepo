# LiDAR Visualization: Initial Design

Author: Rayan Khan

## Goal

Display the OS1 SR LiDAR point cloud in real time on any team member's computer, as a standalone Python program separate from the Linux-only user dashboard.

Sensor: Ouster OS1 SR, 128 channels (per Torrey), 42.4° vertical FOV.

## Architecture

1. The OS1 SR sends UDP packets over Gigabit Ethernet to the AGX Orin (LiDAR on port 7502, IMU on port 7503, per Rohith's writeup).
2. The Orin sends the data over wifi to the viewer computer. [Format to confirm with Torrey: raw Ouster packets or already-converted x, y, z.]
3. A Python receiver program listens for the incoming UDP data.
4. It converts the data into (x, y, z) points in meters, relative to the sensor. If the data arrives as raw packets, the Ouster SDK does this conversion using the sensor's calibration and beam angles.
5. An Open3D viewer displays the points, colored by distance (blue = close, red = far), and refreshes each time a new scan arrives.

## What's built so far

- A Python/Open3D viewer with the function visualize_scan(points). It accepts any array of (x, y, z) points, so it doesn't depend on where the data comes from.
- A fake-scan generator (128 channels, 42.4° vertical FOV, 1,024 azimuth steps) used to test the viewer.
- Distance-based color coding.
- Reviewed David's point-map-dashboard branch: a static sample point cloud now renders next to the car model on the user dashboard (single color, hard-coded scale, no live data). The sample points come from a real Ouster Studio capture (per David); the scale was tuned by trial and error.

## What's next

- Switch the viewer from a static window to a live one that updates for each new scan (Open3D's Visualizer class).
- Test with real Ouster sample data using the Ouster SDK, instead of fake data.
- Match the real beam angles from the sensor's metadata instead of assuming even spacing.
- Decide with the telemetry owner whether the Java telemetry dashboard launches this program or whether they run side by side.

## Porting from Raspberry Pi to AGX Orin

Both are ARM64 Linux, so the Tauri app and a Python viewer should mostly carry over. [Not tested yet.] Needs a test build on the Orin, and watch the GPU/rendering settings David hit (he needed WEBKIT_DISABLE_DMABUF_RENDERER=1 on his machine).

## Code

fake_lidar.py (in this folder)
