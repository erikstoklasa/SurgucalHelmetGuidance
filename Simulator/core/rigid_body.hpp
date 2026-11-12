#include <cstdint>
#include <string>

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Quaternion {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct RigidBody {
    uint32_t id;
    std::string name;
    Vec3 position;
    Quaternion rotation;
    bool visible = true;
};