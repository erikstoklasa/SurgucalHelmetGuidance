#pragma once

#include <cstdint>
#include <glad/gl.h>
#include <glm/vec3.hpp>

class Scene;
class MovementTrails;

enum class AxisDisplayMode {
    All,
    SelectedOnly,
    Hidden
};

class Viewport {
public:
    bool init();
    void render(
        const Scene& scene,
        uint32_t selectedId,
        int width,
        int height,
        AxisDisplayMode axisMode,
        const MovementTrails& trails,
        bool showTrails
    );
    void shutdown();
    void orbit(float dx, float dy);
    void pan(float dx, float dy);
    void zoom(float amount);
    void resetCamera();

    GLuint texture() const { return colorTexture; }

private:
    GLuint framebuffer = 0;
    GLuint colorTexture = 0;
    GLuint depthBuffer = 0;
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;

    int width = 0;
    int height = 0;

    float yaw = 0.52f;
    float pitch = 0.46f;
    float distance = 9.0f;

    glm::vec3 cameraTarget{0.0f};
};