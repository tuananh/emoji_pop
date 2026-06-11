#include "emoji_pop.h"
#include "config.h"
#include "fonts.h"
#include "instance.h"
#include "tone_icons.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <cstdio>
#include <unistd.h>

static void glfw_error(int, const char* desc) {
    std::fprintf(stderr, "GLFW: %s\n", desc);
}

static void ShowWindow(GLFWwindow* window) {
    glfwShowWindow(window);
    glfwFocusWindow(window);
}

static void HideWindow(GLFWwindow* window) {
    glfwHideWindow(window);
}

static void KeyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        HideWindow(window);
}

int main() {
    if (!AcquireInstance())
        return 0;

    // Return immediately when starting the daemon; child keeps the socket and GUI.
    if (fork() > 0)
        return 0;

    glfwSetErrorCallback(glfw_error);
    if (!glfwInit()) {
        ReleaseInstance();
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(640, 480, "Emoji Pop", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        ReleaseInstance();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, KeyCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    float content_scale_x = 1.f, content_scale_y = 1.f;
    glfwGetWindowContentScale(window, &content_scale_x, &content_scale_y);
    io.FontGlobalScale = content_scale_y;

    EmojiPop pop;
    pop.tone = LoadTonePreference();
    LoadRecents(pop.recents, &pop.recent_count);
    pop.RequestFocusSearch();
    pop.on_pick = [window](const char* glyph) {
        std::printf("picked: %s\n", glyph);
        HideWindow(window);
    };

    LoadFonts();
    EnsureToneIconsLoaded();
    GetCachedEmojiTexture("🔍");
    for (int i = 0; i < pop.recent_count; ++i)
        GetCachedEmojiTexture(pop.recents[i]);

    ShowWindow(window);

    bool prev_visible = false;
    while (!glfwWindowShouldClose(window)) {
        PollInstanceServer([window]() { ShowWindow(window); });

        const bool visible = glfwGetWindowAttrib(window, GLFW_VISIBLE) != GLFW_FALSE;
        if (!visible) {
            prev_visible = false;
            glfwWaitEventsTimeout(0.05);
            continue;
        }
        if (!prev_visible)
            pop.RequestFocusSearch();
        prev_visible = true;

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        pop.Draw();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    DestroyToneIcons();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    ReleaseInstance();
    return 0;
}
