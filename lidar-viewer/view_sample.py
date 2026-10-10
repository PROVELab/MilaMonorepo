import numpy as np
import open3d as o3d

# load the real Ouster sample points that David exported from Ouster Studio
cloud = o3d.io.read_point_cloud("sample_points.ply")

# turn the points into a numpy array so we can do math on them
points = np.asarray(cloud.points)
print("Loaded", len(points), "points")

# how far each point is from the sensor at (0, 0, 0)
distances = np.linalg.norm(points, axis=1)

# scale distances to 0-1, treating the farthest 5% of points as "max red"
# (a few far-away points would otherwise squash all the other colors together)
t = np.clip(distances / np.percentile(distances, 95), 0, 1)

# one [red, green, blue] color per point: close = blue, far = red
colors = np.stack([t, np.full_like(t, 0.2), 1 - t], axis=1)
cloud.colors = o3d.utility.Vector3dVector(colors)

# open the interactive window
o3d.visualization.draw_geometries([cloud])