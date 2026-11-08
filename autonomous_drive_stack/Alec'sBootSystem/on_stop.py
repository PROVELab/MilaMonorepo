def run(runtime):
    print("Stopping LiDAR run...")

    lidar = runtime.get("lidar")

    if lidar is not None:
        try:
            lidar.detach_writer("draw-point-cloud")
        except Exception:
            pass