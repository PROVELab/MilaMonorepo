import numpy as np
import open3d as o3d

# gets our math toolbox from np and our 3D viewer toolbox to show points in 3D


def get_beam_direction(v_angle, h_angle):
    """
    Given vertical angle and horizontal angle in degrees,
    returns direction the laser beam points as (dx, dy, dz).
    """

    # converting degrees to radians, since sin/cos needs radians
    v_rad = np.radians(v_angle)
    h_rad = np.radians(h_angle)

    # turns the 2 angles into direction
    dx = np.cos(v_rad) * np.cos(h_rad)
    dy = np.cos(v_rad) * np.sin(h_rad)
    dz = np.sin(v_rad)

    return dx, dy, dz


def distance_to_wall(dx, dy, dz, room_half_width, room_half_length, room_half_height):
    """
    How far the beam travels before hitting a wall, floor, or ceiling.
    """

    dist_x_wall = room_half_width / abs(dx) if dx != 0 else float('inf')
    dist_y_wall = room_half_length / abs(dy) if dy != 0 else float('inf')
    dist_z_wall = room_half_height / abs(dz) if dz != 0 else float('inf')

    return min(dist_x_wall, dist_y_wall, dist_z_wall)


def generate_fake_scan(num_channels, vertical_fov, num_azimuth_steps,
                       room_half_width, room_half_length, room_half_height):
    """
    Creates a fake LiDAR scan of a rectangular room and returns points
    as (x, y, z) coords in meters, relative to the sensor at (0, 0, 0).
    """

    # tilt angle of each beam spread up and down
    vertical_angles = np.linspace(-vertical_fov/2, vertical_fov/2, num_channels)

    # spin angle as it goes around from 0-360
    azimuth_angles = np.linspace(0, 360, num_azimuth_steps, endpoint=False)

    points = []

    for v_angle in vertical_angles:
        for h_angle in azimuth_angles:
            dx, dy, dz = get_beam_direction(v_angle, h_angle)

            # distance beam travels before hitting
            distance = distance_to_wall(dx, dy, dz, room_half_width, room_half_length, room_half_height)

            # calculating 3d point
            x = distance * dx
            y = distance * dy
            z = distance * dz

            points.append([x, y, z])

    return np.array(points)


def visualize_scan(points):
    """
    Takes an array of (x, y, z) points and shows them in an interactive
    3D viewer. Points are colored by distance: blue = close, red = far.
    """

    print("Displaying", len(points), "points")

    # how far each point is from the sensor at (0, 0, 0)
    distances = np.linalg.norm(points, axis=1)

    # scale distances to 0-1 (0 = closest point, 1 = farthest point)
    t = (distances - distances.min()) / (distances.max() - distances.min())

    # one [red, green, blue] color per point: close = blue, far = red
    colors = np.stack([t, np.full_like(t, 0.2), 1 - t], axis=1)

    # makes empty 3D dots container, fills it with points and colors
    cloud = o3d.geometry.PointCloud()
    cloud.points = o3d.utility.Vector3dVector(points)
    cloud.colors = o3d.utility.Vector3dVector(colors)

    # opens interactive window
    o3d.visualization.draw_geometries([cloud])


points = generate_fake_scan(
    num_channels=128,
    vertical_fov=42.4,
    num_azimuth_steps=1024,
    room_half_width=4.0,
    room_half_length=6.0,
    room_half_height=1.5
)

visualize_scan(points)


