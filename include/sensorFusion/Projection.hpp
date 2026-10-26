#pragma once

#include "types.hpp"

#include <Eigen/Core>

#include <optional>
#include <vector>

// Intrinsics for a rectified pinhole image at exactly width x height pixels.
// Raw distorted imagery must be rectified before using this API. Detection
// boxes must use the same image geometry, including any resizing or cropping.
struct CameraCalibration {
    double fx, fy, cx, cy;
    int width, height;
};

// P_camera = rotation * P_lidar + translation. XYZ and translation are meters.
// Camera optical axes: +X right, +Y down, +Z forward.
struct RigidTransform {
    Eigen::Matrix3d rotation = Eigen::Matrix3d::Identity();
    Eigen::Vector3d translation = Eigen::Vector3d::Zero();
};

struct ProjectedPoint {
    float u, v;
    Point3D lidar_xyz;       // Original LiDAR-frame coordinates, meters.
    float camera_depth;     // Camera optical Z, meters (not Euclidean range).
    float lidar_range;      // Euclidean distance from the LiDAR origin, meters.
};

// Caller precondition for both APIs: supply already-validated calibration and
// extrinsics (finite values, positive focal lengths and image dimensions, and
// an orthonormal rotation with determinant +1). Configuration is not validated here.
// Rejects non-finite points/results, unrepresentable results, Z <= 0,
// and pixels outside [0, width) x [0, height).
std::optional<ProjectedPoint> projectPoint(
    const Point3D& lidarPoint,
    const CameraCalibration& camera,
    const RigidTransform& lidarToCamera);

// Returns accepted points in input order. Same caller precondition as projectPoint.
std::vector<ProjectedPoint> projectCloud(
    const std::vector<Point3D>& lidarPoints,
    const CameraCalibration& camera,
    const RigidTransform& lidarToCamera);
