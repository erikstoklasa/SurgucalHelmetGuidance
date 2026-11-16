#pragma once

#include <cstdint>
#include <vector>
#include "rigid_body.hpp"

struct TrackedRigidBody {
    uint32_t id = 0;
    Vec3 position;
    Quaternion rotation;

    float meanError = 0.0f;
    bool trackingValid = true;
};

struct TrackingFrame {
    uint32_t frameNumber = 0;
    double timestampSeconds = 0.0;

    std::vector<TrackedRigidBody> rigidBodies;
};