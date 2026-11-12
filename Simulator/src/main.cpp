
#include <cstdio>
#include <cstdint>
#include <string>

#include <SDL3/SDL.h>
#include <glad/gl.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

#include "../core/scene.hpp"
#include "viewport.hpp"

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
    scene.addBody("Head");
    scene.addBody("Drill");
    scene.addBody("Reference");
    scene.addBody("Target");

    uint32_t selectedBodyId = 0;

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

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Render 3D background
        int fbWidth = 0;
        int fbHeight = 0;
        SDL_GetWindowSizeInPixels(window, &fbWidth, &fbHeight);

        if (fbWidth > 0 && fbHeight > 0) {
            viewport.render(scene, selectedBodyId, fbWidth, fbHeight);

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

        if (ImGui::Button("Add Body"))
            selectedBodyId = scene.addBody("New Body");

        ImGui::SameLine();

        ImGui::BeginDisabled(selectedBodyId == 0);

        if (ImGui::Button("Remove Selected")) {
            if (scene.removeBody(selectedBodyId))
                selectedBodyId = 0;
        }

        ImGui::EndDisabled();
        ImGui::Separator();

        for (const auto& body : scene.getBodies()) {
            std::string label =
                body.name + "##" + std::to_string(body.id);

            if (ImGui::Selectable(
                label.c_str(),
                selectedBodyId == body.id
            )) {
                selectedBodyId = body.id;
            }
        }

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
