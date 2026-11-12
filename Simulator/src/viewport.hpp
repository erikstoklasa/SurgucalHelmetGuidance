#pragma once

#include <cstdint>
#include <glad/gl.h>

class Scene;

class Viewport {
public:
    bool init();
    void render(const Scene& scene, uint32_t selectedId, int width, int height);
    void shutdown();

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
};