#include "sensorFusion/Projection.hpp"

#include <cmath>
#include <limits>

namespace {

bool fitsFloat(double value) {
    return std::isfinite(value) &&
           std::abs(value) <= std::numeric_limits<float>::max();
}

} // namespace

std::optional<ProjectedPoint> projectPoint(
    const Point3D& point, const CameraCalibration& camera,
    const RigidTransform& transform) {
    const Eigen::Vector3d lidar(point.x, point.y, point.z);
    if (!lidar.allFinite()) {
        return std::nullopt;
    }
    const Eigen::Vector3d optical = transform.rotation * lidar + transform.translation;
    if (!optical.allFinite() || optical.z() <= 0) {
        return std::nullopt;
    }
    const double u = camera.fx * (optical.x() / optical.z()) + camera.cx;
    const double v = camera.fy * (optical.y() / optical.z()) + camera.cy;
    const double range = std::hypot(lidar.x(), lidar.y(), lidar.z());
    if (!fitsFloat(u) || !fitsFloat(v) || !fitsFloat(optical.z()) ||
        !fitsFloat(range) || u < 0 || u >= camera.width ||
        v < 0 || v >= camera.height) {
        return std::nullopt;
    }
    const ProjectedPoint result{static_cast<float>(u), static_cast<float>(v), point,
                                static_cast<float>(optical.z()), static_cast<float>(range)};
    // Float rounding can move an otherwise valid pixel onto the upper boundary,
    // or underflow a strictly positive optical depth to zero.
    if (result.u < 0 || static_cast<double>(result.u) >= camera.width ||
        result.v < 0 || static_cast<double>(result.v) >= camera.height ||
        result.camera_depth <= 0) {
        return std::nullopt;
    }
    return result;
}


std::vector<ProjectedPoint> projectCloud(
    const std::vector<Point3D>& lidarPoints, 
    const CameraCalibration& camera,
    const RigidTransform& lidarToCamera) 
    {
    std::vector<ProjectedPoint> output;
    output.reserve(lidarPoints.size());
    for (const auto& point : lidarPoints) {
        if (auto projected = projectPoint(point, camera, lidarToCamera)) {
            output.push_back(*projected);
        }
    }
    return output;
}
