#pragma once

#include <cstdint>
#include <vector>

struct Point3D {
    float x;
    float y;
    float z;
};

struct LidarFrame {
    uint64_t timestamp_ns;
    std::vector<Point3D> points;
};


// Structure of information hoping to get from camera team
struct Detection2D {
    int class_id;
    float confidence;

    float xmin;
    float ymin;
    float xmax;
    float ymax;
};

struct DetectionFrame {
    uint64_t timestamp_ns;
    int camera_id;

    std::vector<Detection2D> detections;
};