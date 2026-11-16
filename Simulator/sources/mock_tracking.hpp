#pragma once

#include "../core/tracking_frame.hpp"
#include <cmath>

inline TrackingFrame makeMockFrame(
    double time,
    const std::vector<uint32_t>& ids
) {
    TrackingFrame frame;

    frame.frameNumber = static_cast<uint32_t>(time * 120.0);
    frame.timestampSeconds = time;

    for (size_t i = 0; i < ids.size(); ++i) {
        float phase = static_cast<float>(time * 1.2 + i * 0.5);

        TrackedRigidBody body;
        body.id = ids[i];

        body.position = {
            static_cast<float>(i % 5) * 0.5f - 1.0f
                + 0.05f * std::sin(phase),
            0.25f + 0.02f * std::sin(phase),
            static_cast<float>(i / 5) * 0.5f
                + 0.04f * std::cos(phase)
        };

        float angle = 0.25f * std::sin(phase);
        body.rotation = {
            std::cos(angle / 2.0f),
            0.0f,
            std::sin(angle / 2.0f),
            0.0f
        };

        body.trackingValid = true;
        body.meanError = 0.0f;

        frame.rigidBodies.push_back(body);
    }

    return frame;
}