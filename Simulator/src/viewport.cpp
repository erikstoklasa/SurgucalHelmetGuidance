
#include "viewport.hpp"
#include "../core/scene.hpp"
#include "../core/scenario_reference.hpp"
#include "../core/movement_trails.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>
#include <cstddef>
#include <cstdio>
#include <cmath>
#include <string>
#include <cmath>

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

    const char* modelFiles[] = {
        "",
        "head.glb",
        "drill_pen.glb",
        "target.glb"
    };

    for (int role = 1; role <= 3; ++role) {
        std::string path =
            std::string(SIM_MODEL_DIR) + "/" + modelFiles[role];

        Assimp::Importer importer;

        const aiScene* imported = importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_PreTransformVertices
        );

        if (!imported) {
            std::fprintf(stderr, "Model load failed: %s\n",
                        path.c_str());
            std::fprintf(stderr, "%s\n",
                        importer.GetErrorString());
            continue;
        }

        for (unsigned int m = 0; m < imported->mNumMeshes; ++m) {
            const aiMesh* mesh = imported->mMeshes[m];

            for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
                const aiFace& face = mesh->mFaces[f];

                if (face.mNumIndices != 3)
                    continue;

                for (unsigned int j = 0; j < 3; ++j) {
                    unsigned int index = face.mIndices[j];

                    const aiVector3D& p = mesh->mVertices[index];

                    glm::vec3 color(0.75f);

                    if (mesh->HasVertexColors(0)) {
                        const aiColor4D& c = mesh->mColors[0][index];
                        color = {c.r, c.g, c.b};
                    }

                    models[role].push_back({
                        {p.x, p.y, p.z},
                        color
                    });
                }
            }
        }

        std::printf("Loaded %s: %zu vertices\n",
                    modelFiles[role], models[role].size());
    }

    return true;
}

void Viewport::render(
    const Scene& scene,
    uint32_t selectedId,
    int w,
    int h,
    AxisDisplayMode axisMode,
    const MovementTrails& trails,
    bool showTrails,
    const std::unordered_map<uint32_t, int>& trackerRoles,
    const ScenarioReference& reference
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
    std::vector<Vertex> modelTriangles;

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


        int role = 0;

        if (auto it = trackerRoles.find(body.id);
            it != trackerRoles.end()) {
            role = it->second;
        }

        if (role >= 1 && role <= 3 && !models[role].empty()) {
            const auto& mesh = models[role];

            for (size_t i = 0; i + 2 < mesh.size(); i += 3) {
                glm::vec3 points[3];

                for (int j = 0; j < 3; ++j) {
                    points[j] = glm::vec3(
                        model * glm::vec4(mesh[i + j].position, 1.0f)
                    );
                }

                glm::vec3 normal = glm::cross(
                    points[1] - points[0],
                    points[2] - points[0]
                );

                float lighting = 0.65f;

                if (glm::length(normal) > 1e-8f) {
                    normal = glm::normalize(normal);

                    lighting = 0.4f + 0.6f * std::abs(
                        glm::dot(
                            normal,
                            glm::normalize(glm::vec3(0.5f, 1.0f, 0.7f))
                        )
                    );
                }

                for (int j = 0; j < 3; ++j) {
                    glm::vec3 color =
                        mesh[i + j].color * lighting;

                    if (body.id == selectedId)
                        color += glm::vec3(0.15f, 0.12f, 0.0f);

                    color = glm::clamp(
                        color, glm::vec3(0.0f), glm::vec3(1.0f)
                    );

                    modelTriangles.push_back({points[j], color});
                }
            }
        } else {
            for (const auto& edge : edges) {
                glm::vec3 a = glm::vec3(
                    model * glm::vec4(corners[edge[0]], 1)
                );

                glm::vec3 b = glm::vec3(
                    model * glm::vec4(corners[edge[1]], 1)
                );

                line(vertices, a, b, color);
            }
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


    
    if (reference.visible) {
        const RigidBody* target = nullptr;

        for (const auto& body : scene.getBodies()) {
            auto it = trackerRoles.find(body.id);

            if (body.visible &&
                it != trackerRoles.end() &&
                it->second == 3) {
                target = &body;
                break;
            }
        }

        if (target) {
            glm::vec3 origin(
                target->position.x,
                target->position.y,
                target->position.z
            );

            glm::quat q(
                target->rotation.w,
                target->rotation.x,
                target->rotation.y,
                target->rotation.z
            );

            if (glm::dot(q, q) < 1e-8f)
                q = glm::quat(1, 0, 0, 0);

            q = glm::normalize(q);

            float yaw = glm::radians(reference.yawDeg);
            float elevation = glm::radians(reference.elevationDeg);

            glm::vec3 localDirection(
                std::sin(yaw) * std::cos(elevation),
                std::sin(elevation),
                std::cos(yaw) * std::cos(elevation)
            );

            glm::vec3 direction = q * localDirection;

            glm::vec3 tip = origin + direction * 0.8f;
            glm::vec3 color(1.0f, 0.9f, 0.1f);

            line(vertices, origin, tip, color);

            glm::vec3 side = glm::cross(
                direction, glm::vec3(0, 1, 0)
            );

            if (glm::length(side) < 0.001f)
                side = q * glm::vec3(1, 0, 0);
            else
                side = glm::normalize(side);

            line(
                vertices,
                tip,
                tip - direction * 0.15f + side * 0.08f,
                color
            );

            line(
                vertices,
                tip,
                tip - direction * 0.15f - side * 0.08f,
                color
            );
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

    if (!modelTriangles.empty()) {
        glBufferData(
            GL_ARRAY_BUFFER,
            modelTriangles.size() * sizeof(Vertex),
            modelTriangles.data(),
            GL_DYNAMIC_DRAW
        );

        glDrawArrays(
            GL_TRIANGLES,
            0,
            static_cast<GLsizei>(modelTriangles.size())
        );
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);

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
