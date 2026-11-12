#!/usr/bin/env python3
"""
launch_perception.py
--------------------
Starts the full perception stack and prints the web-viewer URL.

Runs three ROS2 nodes as subprocesses:
  - detector_node.py       (YOLOv8 per-camera object detection)
  - three_camera_viewer.py (web viewer on :8765 with YOLO boxes overlaid)

Run this AFTER opening Prove_sim.usd in Isaac Sim and pressing Play ▶.
Ctrl+C to stop all three.

Usage:
    source /opt/ros/humble/setup.bash
    python3 launch_perception.py
"""

import os
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).parent.resolve()
DETECTOR = HERE / "detector_node.py"
VIEWER = HERE / "three_camera_viewer.py"
VIEWER_PORT = 8765


def get_lan_ip() -> str:
    """Best-effort LAN IP so we can print a browser-reachable URL."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        return ip
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


def main() -> None:
    env = os.environ.copy()
    if "ROS_DISTRO" not in env:
        print(
            "[warn] ROS_DISTRO not set — did you `source /opt/ros/humble/setup.bash`?",
            file=sys.stderr,
        )

    procs: list[tuple[str, subprocess.Popen]] = []

    def launch(name: str, path: Path) -> None:
        if not path.exists():
            print(f"[skip] {name}: {path.name} not found", file=sys.stderr)
            return
        print(f"[start] {name}")
        p = subprocess.Popen(
            [sys.executable, str(path)],
            cwd=str(HERE),
            stdout=sys.stdout,
            stderr=sys.stderr,
            env=env,
        )
        procs.append((name, p))

    launch("detector_node", DETECTOR)
    time.sleep(1.5)  # let detector register subscriptions first
    launch("three_camera_viewer", VIEWER)

    url = f"http://{get_lan_ip()}:{VIEWER_PORT}"
    print()
    print("=" * 60)
    print("Perception running. Open in a browser:")
    print(f"  {url}")
    print("Ctrl+C to stop everything.")
    print("=" * 60)
    print()

    def shutdown(signum=None, frame=None) -> None:
        for name, p in procs:
            if p.poll() is None:
                print(f"[stop] {name}")
                p.terminate()
        for _, p in procs:
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                p.kill()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    # If any child dies, shut everything down
    while True:
        for name, p in procs:
            if p.poll() is not None:
                print(
                    f"[exit] {name} exited (code {p.returncode}); stopping others."
                )
                shutdown()
        time.sleep(1)


if __name__ == "__main__":
    main()
