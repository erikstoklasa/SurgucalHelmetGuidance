#pragma once

#include <cstdint>
#include <cstddef>
#include <deque>
#include <unordered_map>

#include "rigid_body.hpp"

class MovementTrails {
public:
    void add(uint32_t id, const Vec3& position, size_t maxPoints)
    {
        auto& trail = history[id];
        trail.push_back(position);

        while (trail.size() > maxPoints)
            trail.pop_front();
    }

    const std::deque<Vec3>* get(uint32_t id) const
    {
        auto it = history.find(id);
        return it != history.end() ? &it->second : nullptr;
    }

    void clear()
    {
        history.clear();
    }

private:
    std::unordered_map<uint32_t, std::deque<Vec3>> history;
};