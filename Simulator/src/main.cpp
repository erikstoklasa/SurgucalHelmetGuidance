
#include <cstdio>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <optional>
#include <cfloat>
#include <unordered_set>

#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

#include "../core/scene.hpp"
#include "viewport.hpp"

#include "../sources/mock_tracking.hpp"
#include "../sources/csv_tracking.hpp"
#include "../sources/csv_playback.hpp"
#include "../core/movement_trails.hpp"
#include "../core/tracking_diagnostics.hpp"
#include "../sources/natnet_tracking.hpp"

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "Surgical Helmet Simulator",
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::fprintf(stderr, "Window error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    if (!gl_context) {
        std::fprintf(stderr, "OpenGL error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_MakeCurrent(window, gl_context);

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        std::fprintf(stderr, "Failed to load OpenGL\n");
        SDL_GL_DestroyContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    Scene scene;
    auto recordedFrames = loadTrackingCSV("data/sample_tracking.csv");

    std::printf(
        "Loaded %zu frames with %zu bodies in the first frame\n",
        recordedFrames.size(),
        recordedFrames.front().rigidBodies.size()
    );

    std::unordered_map<uint32_t, uint32_t> csvToSceneId;

    for (const auto& frame : recordedFrames) {
        for (const auto& tracked : frame.rigidBodies) {
            if (!csvToSceneId.contains(tracked.id)) {
                uint32_t sceneId = scene.addBody(
                    "Tracker " + std::to_string(tracked.id)
                );

                csvToSceneId[tracked.id] = sceneId;
            }
        }
    }

    CSVPlayback playback(recordedFrames);

    int sourceMode = 0; // 0 = CSV, 1 = MOCK, 2 = LIVE
    int previousSourceMode = 0;

    NatNetTracking natnet;
    bool liveConnected = false;
    bool liveConnectionFailed = false;

    std::unordered_map<uint32_t, uint32_t> liveToSceneId;

    char serverIP[64] = {};
    char localIP[64] = {};

    Uint64 lastLiveFrameTicks = 0;

    MovementTrails trails;
    bool showTrails = true;
    int trailLength = 120;

    std::optional<uint32_t> previousFrameNumber;


    Uint64 previousTicks = SDL_GetTicks();

    uint32_t selectedBodyId = 0;
    uint32_t renameEditingId = 0;
    char renameBuffer[128] = {};

    std::unordered_map<uint32_t, int> trackerRoles;

    const char* roleNames[] = {
        "Unassigned",
        "Head",
        "Drill",
        "Target",
        "Reference"
    };

    Viewport viewport;

    if (!viewport.init()) {
        std::fprintf(stderr, "Viewport initialization failed\n");
        viewport.shutdown();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DestroyContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;

    TrackingFrame latestFrame;
    int axisMode = 1;

    TrackingDiagnostics diagnostics;

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            if (event.type == SDL_EVENT_QUIT)
                running = false;

            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        
 
        Uint64 now = SDL_GetTicks();
        double dt = (now - previousTicks) / 1000.0;
        previousTicks = now;

        bool sourceChanged = sourceMode != previousSourceMode;

        if (sourceChanged) {
            if (previousSourceMode == 2) {
                natnet.disconnect();
                liveConnected = false;
            }

            liveConnectionFailed = false;
            latestFrame = TrackingFrame{};
            diagnostics.reset();
            trails.clear();
            previousFrameNumber.reset();
            lastLiveFrameTicks = 0;
            previousSourceMode = sourceMode;

            if (sourceMode == 0) {
                for (auto& [csvId, sceneId] : csvToSceneId) {
                    if (!scene.findBody(sceneId)) {
                        sceneId = scene.addBody(
                            "Tracker " + std::to_string(csvId)
                        );
                    }
                }
            }
        }

        bool haveFrame = false;

        if (sourceMode == 0) {
            playback.update(dt);

            if (const TrackingFrame* frame = playback.currentFrame()) {
                latestFrame = *frame;
                haveFrame = true;
            }
        } else if (sourceMode == 1) {
            std::vector<uint32_t> ids;

            for (const auto& body : scene.getBodies()) {
                bool liveBody = false;

                for (const auto& [trackingId, sceneId] : liveToSceneId) {
                    if (sceneId == body.id) {
                        liveBody = true;
                        break;
                    }
                }

                if (!liveBody)
                    ids.push_back(body.id);
            }

            latestFrame = makeMockFrame(now / 1000.0, ids);
            haveFrame = true;
        } else if (liveConnected) {
            if (natnet.poll(latestFrame)) {
                haveFrame = true;
                lastLiveFrameTicks = now;
            }
        }

        auto findSceneId = [&](uint32_t trackingId) -> uint32_t {
            if (sourceMode == 0) {
                auto it = csvToSceneId.find(trackingId);
                return it != csvToSceneId.end() ? it->second : 0;
            }

            if (sourceMode == 2) {
                auto it = liveToSceneId.find(trackingId);

                if (it != liveToSceneId.end() &&
                    scene.findBody(it->second))
                    return it->second;

                uint32_t sceneId = scene.addBody(
                    "Tracker " + std::to_string(trackingId)
                );

                liveToSceneId[trackingId] = sceneId;
                return sceneId;
            }

            return trackingId;
        };

        if (haveFrame) {
            diagnostics.update(latestFrame);

            for (const auto& tracked : latestFrame.rigidBodies) {
                uint32_t sceneId = findSceneId(tracked.id);

                if (RigidBody* body = scene.findBody(sceneId)) {
                    if (tracked.trackingValid) {
                        body->position = tracked.position;
                        body->rotation = tracked.rotation;
                    }
                }
            }

            if (previousFrameNumber) {
                uint32_t previous = *previousFrameNumber;
                uint32_t current = latestFrame.frameNumber;

                if (current < previous || current - previous > 15)
                    trails.clear();
            }

            if (!previousFrameNumber ||
                latestFrame.frameNumber != *previousFrameNumber) {

                for (const auto& tracked : latestFrame.rigidBodies) {
                    if (!tracked.trackingValid)
                        continue;

                    uint32_t sceneId = findSceneId(tracked.id);

                    if (scene.findBody(sceneId)) {
                        trails.add(
                            sceneId,
                            tracked.position,
                            static_cast<size_t>(trailLength)
                        );
                    }
                }
            }

            previousFrameNumber = latestFrame.frameNumber;
        }


        std::unordered_set<uint32_t> csvIds;
        std::unordered_set<uint32_t> liveIds;

        for (const auto& [trackingId, sceneId] : csvToSceneId)
            csvIds.insert(sceneId);

        for (const auto& [trackingId, sceneId] : liveToSceneId)
            liveIds.insert(sceneId);

        for (const auto& entry : scene.getBodies()) {
            RigidBody* body = scene.findBody(entry.id);
            if (!body)
                continue;

            body->visible =
                (sourceMode == 0 && csvIds.contains(body->id)) ||
                (sourceMode == 1 && !liveIds.contains(body->id)) ||
                (sourceMode == 2 && liveConnected &&
                lastLiveFrameTicks != 0 &&
                now - lastLiveFrameTicks <= 1000 &&
                liveIds.contains(body->id));
        }

        if (selectedBodyId != 0) {
            RigidBody* selectedBody = scene.findBody(selectedBodyId);

            if (!selectedBody || !selectedBody->visible)
                selectedBodyId = 0;
        }



        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();


        static bool orbiting = false;
        static bool panning = false;

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !io.WantCaptureMouse)
            orbiting = true;

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)
            && !io.WantCaptureMouse)
            panning = true;

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            orbiting = false;

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle))
            panning = false;

        if (orbiting)
            viewport.orbit(io.MouseDelta.x, io.MouseDelta.y);

        if (panning)
            viewport.pan(io.MouseDelta.x, io.MouseDelta.y);

        if (!io.WantCaptureMouse && io.MouseWheel != 0.0f)
            viewport.zoom(io.MouseWheel);


        // Render 3D background
        int fbWidth = 0;
        int fbHeight = 0;
        SDL_GetWindowSizeInPixels(window, &fbWidth, &fbHeight);

        if (fbWidth > 0 && fbHeight > 0) {
            viewport.render(
                scene,
                selectedBodyId,
                fbWidth,
                fbHeight,
                static_cast<AxisDisplayMode>(axisMode),
                trails,
                showTrails
            );

            ImGuiViewport* mainViewport = ImGui::GetMainViewport();
            ImVec2 pos = mainViewport->Pos;
            ImVec2 size = mainViewport->Size;

            ImGui::GetBackgroundDrawList(mainViewport)->AddImage(
                static_cast<ImTextureID>(viewport.texture()),
                pos,
                ImVec2(pos.x + size.x, pos.y + size.y),
                ImVec2(0, 1),
                ImVec2(1, 0)
            );
        }

        // Scene Manager
        ImGui::SetNextWindowPos(
            ImVec2(10, 10), ImGuiCond_FirstUseEver
        );
        ImGui::SetNextWindowSize(
            ImVec2(260, 400), ImGuiCond_FirstUseEver
        );
        ImGui::SetNextWindowBgAlpha(0.85f);

        ImGui::Begin("Scene Manager");

        ImGui::BeginDisabled(sourceMode != 1);

        if (ImGui::Button("Add Body"))
            selectedBodyId = scene.addBody("New Body");

        ImGui::SameLine();

        ImGui::BeginDisabled(selectedBodyId == 0);

        if (ImGui::Button("Remove Selected")) {
            if (scene.removeBody(selectedBodyId))
                selectedBodyId = 0;
        }
        ImGui::EndDisabled();

        ImGui::EndDisabled();
        ImGui::Separator();

        for (const auto& body : scene.getBodies()) {
            if (!body.visible)
                continue;

            int role = 0;

            if (auto it = trackerRoles.find(body.id);
                it != trackerRoles.end()) {
                role = it->second;
            }

            std::string label =
                body.name + " [" + roleNames[role] + "]##"
                + std::to_string(body.id);

            if (ImGui::Selectable(
                label.c_str(),
                selectedBodyId == body.id
            )) {
                selectedBodyId = body.id;
            }
        }

        ImGui::Separator();

        if (selectedBodyId != renameEditingId) {
            renameEditingId = selectedBodyId;

            if (RigidBody* body = scene.findBody(selectedBodyId)) {
                std::snprintf(
                    renameBuffer,
                    sizeof(renameBuffer),
                    "%s",
                    body->name.c_str()
                );
            } else {
                renameBuffer[0] = '\0';
            }
        }

        if (RigidBody* body = scene.findBody(selectedBodyId)) {
            ImGui::InputText(
                "Name",
                renameBuffer,
                sizeof(renameBuffer)
            );

            if (ImGui::Button("Rename") && renameBuffer[0] != '\0') {
                body->name = renameBuffer;
            }
        }


        if (scene.findBody(selectedBodyId)) {
            ImGui::Separator();
            ImGui::Text("Scenario Assignment");

            int role = 0;

            if (auto it = trackerRoles.find(selectedBodyId);
                it != trackerRoles.end()) {
                role = it->second;
            }

            if (ImGui::Combo(
                "Role",
                &role,
                roleNames,
                IM_ARRAYSIZE(roleNames)
            )) {
                if (role == 0) {
                    trackerRoles.erase(selectedBodyId);
                } else {
                    for (auto& [id, assignedRole] : trackerRoles) {
                        if (assignedRole == role)
                            assignedRole = 0;
                    }

                    trackerRoles[selectedBodyId] = role;
                }
            }
        }


        ImGui::Separator();
        ImGui::Text("Camera");

        if (ImGui::Button("Reset Camera"))
            viewport.resetCamera();

        ImGui::Separator();
        
        
        ImGui::Combo(
            "Tracking Source",
            &sourceMode,
            "CSV Playback\0Mock Tracking\0Live OptiTrack\0"
        );

        if (sourceMode == 0) {
            ImGui::Checkbox("Playing", &playback.playing);

            ImGui::SliderFloat(
                "Speed", &playback.speed,
                0.1f, 3.0f, "%.1fx"
            );

            float timeline = static_cast<float>(playback.time);

            if (ImGui::SliderFloat(
                "Timeline", &timeline,
                0.0f, static_cast<float>(playback.duration()),
                "%.2f s"
            )) {
                playback.time = timeline;
            }

            if (ImGui::Button("Restart"))
                playback.time = 0.0;
        }


        if (sourceMode == 2) {
            ImGui::Separator();
            ImGui::Text("NatNet Connection");

            ImGui::InputText(
                "Server IP", serverIP, sizeof(serverIP)
            );

            ImGui::InputText(
                "Local IP", localIP, sizeof(localIP)
            );

            ImGui::BeginDisabled(
                serverIP[0] == '\0' || localIP[0] == '\0'
            );

            if (ImGui::Button("Connect")) {
                natnet.disconnect();

                liveConnected = natnet.connect(serverIP, localIP);
                liveConnectionFailed = !liveConnected;

                latestFrame = TrackingFrame{};
                diagnostics.reset();
                trails.clear();
                previousFrameNumber.reset();
                lastLiveFrameTicks = 0;
            }

            ImGui::EndDisabled();

            ImGui::SameLine();

            if (ImGui::Button("Disconnect")) {
                natnet.disconnect();
                liveConnected = false;
                liveConnectionFailed = false;

                latestFrame = TrackingFrame{};
                diagnostics.reset();
                trails.clear();
                previousFrameNumber.reset();
                lastLiveFrameTicks = 0;
            }

            if (liveConnectionFailed) {
                ImGui::TextDisabled("Connection failed");
            } else if (!liveConnected) {
                ImGui::TextDisabled("Disconnected");
            } else if (lastLiveFrameTicks == 0) {
                ImGui::Text("Connected - waiting for frames");
            } else if (SDL_GetTicks() - lastLiveFrameTicks > 1000) {
                ImGui::TextDisabled("No recent live frames");
            } else {
                ImGui::Text("Receiving live frames");
            }
        }


        ImGui::Text(
            "Source: %s",
            sourceMode == 0 ? "CSV" :
            sourceMode == 1 ? "MOCK" : "LIVE"
        );



        ImGui::Text("Frame: %u", latestFrame.frameNumber);
        ImGui::Text("Time: %.3f s", latestFrame.timestampSeconds);
        ImGui::Text("Tracked bodies: %zu",
            latestFrame.rigidBodies.size());


        // Body Inspector - quaternion and position

        ImGui::Separator();
        ImGui::Text("Body Inspector");

        const TrackedRigidBody* selected = nullptr;

        for (const auto& body : latestFrame.rigidBodies) {
            if (findSceneId(body.id) == selectedBodyId) {
                selected = &body;
                break;
            }
        }

        if (sourceMode == 2 &&
            (!liveConnected ||
            lastLiveFrameTicks == 0 ||
            SDL_GetTicks() - lastLiveFrameTicks > 1000)) {
            selected = nullptr;
        }

        if (selected) {
            ImGui::Text("ID: %u", selected->id);
            ImGui::Text("Tracking: %s",
                selected->trackingValid ? "VALID" : "INVALID");

            ImGui::Text("Mean Error: %.6f", selected->meanError);

            if (selected->trackingValid) {
                const auto& p = selected->position;
                const auto& r = selected->rotation;

                ImGui::Text("Position (m)");
                ImGui::Text("X: %.4f  Y: %.4f  Z: %.4f",
                    p.x, p.y, p.z);

                ImGui::Text("Quaternion (w, x, y, z)");
                ImGui::Text("%.4f  %.4f  %.4f  %.4f",
                    r.w, r.x, r.y, r.z);


                glm::quat q(r.w, r.x, r.y, r.z);

                if (glm::dot(q, q) > 1e-8f) {
                    glm::vec3 euler = glm::degrees(
                        glm::eulerAngles(glm::normalize(q))
                    );

                    ImGui::Text("Euler XYZ (deg)");
                    ImGui::Text("X: %.2f  Y: %.2f  Z: %.2f",
                        euler.x, euler.y, euler.z);
                }
            }

            ImGui::Separator();
            ImGui::Text("Movement Diagnostics");

            const BodyDiagnostics* stats = diagnostics.get(selected->id);

            if (stats) {
                if (stats->valid) {
                    ImGui::Text(
                        "Speed: %.2f mm/s",
                        stats->speedMps * 1000.0f
                    );
                } else {
                    ImGui::TextDisabled("Speed: unavailable");
                }

                if (!stats->speedHistory.empty()) {
                    ImGui::PlotLines(
                        "Speed history (m/s)",
                        stats->speedHistory.data(),
                        static_cast<int>(stats->speedHistory.size()),
                        0,
                        nullptr,
                        0.0f,
                        FLT_MAX,
                        ImVec2(0, 80)
                    );
                }
            }
        } else {
            ImGui::TextDisabled("No tracking data for selected body");
        }


        // Diagnostics
        ImGui::Separator();
        ImGui::Text("Diagnostics");

        ImGui::Text("Display FPS: %.1f", io.Framerate);

        ImGui::Text(
            "Tracking rate (estimated): %.1f Hz",
            diagnostics.getFrameRateHz()
        );


        // Axis Display Mode
        ImGui::Separator();
        ImGui::Text("Visualization");

        ImGui::Combo(
            "Body Axes",
            &axisMode,
            "Always\0Selected only\0Hidden\0"
        );

        // Movement Trails
        ImGui::Separator();
        ImGui::Text("Movement Trails");

        ImGui::Checkbox("Show Trails", &showTrails);

        ImGui::SliderInt(
            "Trail Length",
            &trailLength,
            10,
            500
        );

        if (ImGui::Button("Clear Trails"))
            trails.clear();

        ImGui::End();

        // Render UI
        ImGui::Render();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.10f, 0.11f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    viewport.shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
