
#pragma once

#include "../core/tracking_frame.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

class CSVPlayback {
public:
    explicit CSVPlayback(const std::vector<TrackingFrame>& frames)
        : frames(frames) {}

    bool playing = true;
    float speed = 1.0f;
    double time = 0.0;

    double duration() const {
        if (frames.size() < 2)
            return 0.0;

        return frames.back().timestampSeconds
             - frames.front().timestampSeconds;
    }

    void update(double dt) {
        if (!playing || duration() <= 0.0)
            return;

        time += std::max(0.0, dt) * speed;

        if (time > duration())
            time = std::fmod(time, duration());
    }

    const TrackingFrame* currentFrame() const {
        if (frames.empty())
            return nullptr;

        double timestamp = frames.front().timestampSeconds + time;

        auto it = std::upper_bound(
            frames.begin(),
            frames.end(),
            timestamp,
            [](double t, const TrackingFrame& frame) {
                return t < frame.timestampSeconds;
            }
        );

        if (it == frames.begin())
            return &frames.front();

        return &*(it - 1);
    }

private:
    const std::vector<TrackingFrame>& frames;
};
