
#pragma once

#include "../core/tracking_frame.hpp"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

inline std::vector<TrackingFrame> loadTrackingCSV(
    const std::string& path)
{
    std::ifstream file(path);

    if (!file)
        throw std::runtime_error("Cannot open CSV: " + path);

    std::vector<TrackingFrame> frames;
    std::string line;

    std::getline(file, line); // Skip header

    while (std::getline(file, line)) {
        if (line.empty())
            continue;

        std::stringstream stream(line);
        std::vector<std::string> values;
        std::string value;

        while (std::getline(stream, value, ','))
            values.push_back(value);

        if (values.size() != 12)
            throw std::runtime_error("Invalid CSV row: " + line);

        uint32_t frameNumber =
            static_cast<uint32_t>(std::stoul(values[0]));

        double timestamp = std::stod(values[1]);

        TrackedRigidBody body;
        body.id = static_cast<uint32_t>(std::stoul(values[2]));

        body.position = {
            std::stof(values[3]),
            std::stof(values[4]),
            std::stof(values[5])
        };

        body.rotation = {
            std::stof(values[9]), // qw
            std::stof(values[6]), // qx
            std::stof(values[7]), // qy
            std::stof(values[8])  // qz
        };

        body.meanError = std::stof(values[10]);
        body.trackingValid = std::stoi(values[11]) != 0;

        if (frames.empty() ||
            frames.back().frameNumber != frameNumber) {

            TrackingFrame frame;
            frame.frameNumber = frameNumber;
            frame.timestampSeconds = timestamp;
            frames.push_back(frame);
        }

        frames.back().rigidBodies.push_back(body);
    }

    if (frames.empty())
        throw std::runtime_error("CSV contains no tracking frames");

    return frames;
}
