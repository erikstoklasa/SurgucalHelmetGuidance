
#pragma once

#include "tracking_frame.hpp"

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

struct BodyDiagnostics {
    float speedMps = 0.0f;
    bool present = false;
    bool valid = false;

    Vec3 previousPosition{};
    double previousTime = 0.0;
    bool hasPrevious = false;

    std::vector<float> speedHistory;
};

class TrackingDiagnostics {
public:
    void reset() {
        bodies.clear();
        hasLastFrame = false;
        frameRateHz = 0.0;
    }

    void update(const TrackingFrame& frame) {
        if (hasLastFrame &&
            frame.frameNumber == lastFrameNumber &&
            frame.timestampSeconds == lastTimestamp)
            return;

        if (hasLastFrame &&
            (frame.frameNumber < lastFrameNumber ||
             frame.timestampSeconds < lastTimestamp))
            reset();

        if (hasLastFrame) {
            double dt = frame.timestampSeconds - lastTimestamp;

            if (dt > 0.0) {
                frameRateHz =
                    (frame.frameNumber - lastFrameNumber) / dt;
            }
        }

        for (auto& [id, stats] : bodies)
            stats.present = false;

        for (const auto& tracked : frame.rigidBodies) {
            auto& stats = bodies[tracked.id];

            stats.present = true;
            stats.valid = tracked.trackingValid;

            if (tracked.trackingValid) {
                double dt = frame.timestampSeconds - stats.previousTime;

                if (stats.hasPrevious && dt > 0.0) {
                    const Vec3& a = stats.previousPosition;
                    const Vec3& b = tracked.position;

                    float dx = b.x - a.x;
                    float dy = b.y - a.y;
                    float dz = b.z - a.z;

                    stats.speedMps =
                        std::sqrt(dx*dx + dy*dy + dz*dz) / dt;
                } else {
                    stats.speedMps = 0.0f;
                }

                stats.previousPosition = tracked.position;
                stats.previousTime = frame.timestampSeconds;
                stats.hasPrevious = true;
            } else {
                stats.speedMps = 0.0f;
                stats.hasPrevious = false;
            }

            stats.speedHistory.push_back(stats.speedMps);

            if (stats.speedHistory.size() > 120)
                stats.speedHistory.erase(stats.speedHistory.begin());
        }

        for (auto& [id, stats] : bodies) {
            if (!stats.present) {
                stats.valid = false;
                stats.speedMps = 0.0f;
                stats.hasPrevious = false;
            }
        }

        lastFrameNumber = frame.frameNumber;
        lastTimestamp = frame.timestampSeconds;
        hasLastFrame = true;
    }

    const BodyDiagnostics* get(uint32_t trackingId) const {
        auto it = bodies.find(trackingId);
        return it != bodies.end() ? &it->second : nullptr;
    }

    double getFrameRateHz() const {
        return frameRateHz;
    }

private:
    std::unordered_map<uint32_t, BodyDiagnostics> bodies;

    uint32_t lastFrameNumber = 0;
    double lastTimestamp = 0.0;
    double frameRateHz = 0.0;
    bool hasLastFrame = false;
};
