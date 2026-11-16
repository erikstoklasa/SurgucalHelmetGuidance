
#include "viewport.hpp"
#include "../core/scene.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <cstddef>
#include <cstdio>
#include <cmath>

#include "../core/movement_trails.hpp"

namespace {
struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

void line(std::vector<Vertex>& vertices,
          glm::vec3 a, glm::vec3 b, glm::vec3 color)
{
    vertices.push_back({a, color});
    vertices.push_back({b, color});
}

GLuint shader(GLenum type, const char* source)
{
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    GLint success = GL_FALSE;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);

    if (!success) {
        char log[1024];
        glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader error: %s\n", log);
        glDeleteShader(id);
        return 0;
    }
    return id;
}
}

bool Viewport::init()
{
    const char* vertexSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec3 aColor;

        uniform mat4 uVP;
        out vec3 vColor;

        void main() {
            vColor = aColor;
            gl_Position = uVP * vec4(aPosition, 1.0);
        }
    )";

    const char* fragmentSource = R"(
        #version 330 core
        in vec3 vColor;
        out vec4 FragColor;

        void main() {
            FragColor = vec4(vColor, 1.0);
        }
    )";

    GLuint vs = shader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = shader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vs || !fs) return false;

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) return false;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex), (void*)offsetof(Vertex, position)
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex), (void*)offsetof(Vertex, color)
    );

    glBindVertexArray(0);

    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &colorTexture);
    glGenRenderbuffers(1, &depthBuffer);

    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return true;
}

void Viewport::render(
    const Scene& scene,
    uint32_t selectedId,
    int w,
    int h,
    AxisDisplayMode axisMode,
    const MovementTrails& trails,
    bool showTrails
)
{
    if (w <= 0 || h <= 0) return;

    if (w != width || h != height) {
        width = w;
        height = h;

        glBindTexture(GL_TEXTURE_2D, colorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
        glRenderbufferStorage(
            GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h
        );

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, colorTexture, 0
        );
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, depthBuffer
        );

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
            != GL_FRAMEBUFFER_COMPLETE) {
            std::fprintf(stderr, "Viewport framebuffer incomplete\n");
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }
    }

    std::vector<Vertex> vertices;

    for (int i = -5; i <= 5; ++i) {
        float x = static_cast<float>(i);
        glm::vec3 gray(0.25f);

        line(vertices, {x, 0, -5}, {x, 0, 5}, gray);
        line(vertices, {-5, 0, x}, {5, 0, x}, gray);
    }

    line(vertices, {0,0,0}, {2,0,0}, {1,0.2f,0.2f});
    line(vertices, {0,0,0}, {0,2,0}, {0.2f,1,0.2f});
    line(vertices, {0,0,0}, {0,0,2}, {0.2f,0.5f,1});

    const glm::vec3 corners[8] = {
        {-0.2f,-0.2f,-0.2f}, {0.2f,-0.2f,-0.2f},
        {0.2f,0.2f,-0.2f}, {-0.2f,0.2f,-0.2f},
        {-0.2f,-0.2f,0.2f}, {0.2f,-0.2f,0.2f},
        {0.2f,0.2f,0.2f}, {-0.2f,0.2f,0.2f}
    };

    const int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    for (const auto& body : scene.getBodies()) {
        if (!body.visible) continue;

        glm::vec3 p(
            body.position.x, body.position.y, body.position.z
        );

        glm::quat q(
            body.rotation.w, body.rotation.x,
            body.rotation.y, body.rotation.z
        );

        if (glm::dot(q, q) < 1e-8f)
            q = glm::quat(1, 0, 0, 0);

        glm::mat4 model =
            glm::translate(glm::mat4(1), p) *
            glm::mat4_cast(glm::normalize(q));

        glm::vec3 color = body.id == selectedId
            ? glm::vec3(1.0f, 0.85f, 0.2f)
            : glm::vec3(0.3f, 0.7f, 1.0f);

        for (const auto& edge : edges) {
            glm::vec3 a = glm::vec3(
                model * glm::vec4(corners[edge[0]], 1)
            );
            glm::vec3 b = glm::vec3(
                model * glm::vec4(corners[edge[1]], 1)
            );
            line(vertices, a, b, color);
        }
        bool showAxes =
            axisMode == AxisDisplayMode::All ||
            (axisMode == AxisDisplayMode::SelectedOnly &&
            body.id == selectedId);

        if (showAxes) {
            float axisLength = 0.5f;

            glm::vec3 x = glm::vec3(
                model * glm::vec4(axisLength, 0, 0, 1)
            );

            glm::vec3 y = glm::vec3(
                model * glm::vec4(0, axisLength, 0, 1)
            );

            glm::vec3 z = glm::vec3(
                model * glm::vec4(0, 0, axisLength, 1)
            );

            line(vertices, p, x, {1.0f, 0.2f, 0.2f});
            line(vertices, p, y, {0.2f, 1.0f, 0.2f});
            line(vertices, p, z, {0.2f, 0.5f, 1.0f});
        }
        if (showTrails) {
            const auto* trail = trails.get(body.id);

            if (trail) {
                glm::vec3 trailColor =
                    body.id == selectedId
                    ? glm::vec3(1.0f, 0.8f, 0.2f)
                    : glm::vec3(0.3f, 0.7f, 1.0f);

                for (size_t i = 1; i < trail->size(); ++i) {
                    const Vec3& a = (*trail)[i - 1];
                    const Vec3& b = (*trail)[i];

                    line(
                        vertices,
                        {a.x, a.y, a.z},
                        {b.x, b.y, b.z},
                        trailColor
                    );
                }
            }
        }
    }

    glm::vec3 cameraPosition = cameraTarget + glm::vec3(
        distance * std::cos(pitch) * std::sin(yaw),
        distance * std::sin(pitch),
        distance * std::cos(pitch) * std::cos(yaw)
    );

    glm::mat4 view = glm::lookAt(
        cameraPosition,
        cameraTarget,
        glm::vec3(0, 1, 0)
    );

    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        static_cast<float>(w) / static_cast<float>(h),
        0.01f, 100.0f
    );

    glm::mat4 vp = projection * view;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, w, h);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(program);
    glUniformMatrix4fv(
        glGetUniformLocation(program, "uVP"),
        1, GL_FALSE, glm::value_ptr(vp)
    );

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
}

void Viewport::shutdown()
{
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &colorTexture);
    glDeleteRenderbuffers(1, &depthBuffer);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(program);
}


void Viewport::orbit(float dx, float dy)
{
    yaw -= dx * 0.005f;
    pitch += dy * 0.005f;

    pitch = glm::clamp(pitch, -1.5f, 1.5f);
}

void Viewport::zoom(float amount)
{
    distance *= std::pow(0.9f, amount);
    distance = glm::clamp(distance, 0.3f, 100.0f);
}

void Viewport::pan(float dx, float dy)
{
    glm::vec3 eye = cameraTarget + glm::vec3(
        distance * std::cos(pitch) * std::sin(yaw),
        distance * std::sin(pitch),
        distance * std::cos(pitch) * std::cos(yaw)
    );

    glm::vec3 forward = glm::normalize(cameraTarget - eye);
    glm::vec3 right = glm::normalize(
        glm::cross(forward, glm::vec3(0, 1, 0))
    );
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    float scale = distance * 0.002f;

    cameraTarget += (-dx * right + dy * up) * scale;
}

void Viewport::resetCamera()
{
    yaw = 0.52f;
    pitch = 0.46f;
    distance = 9.0f;
    cameraTarget = glm::vec3(0.0f);
}
