#include "emoji_pop.h"
#include "config.h"
#include "fonts.h"
#include "instance.h"
#include "theme.h"
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

static void CenterWindow(GLFWwindow* window) {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    int win_w = 0, win_h = 0;
    glfwGetWindowSize(window, &win_w, &win_h);
    glfwSetWindowPos(window, (mode->width - win_w) / 2, (mode->height - win_h) / 2);
}

static void ShowWindow(GLFWwindow* window, EmojiPop& pop) {
    CenterWindow(window);
    glfwShowWindow(window);
    glfwFocusWindow(window);
    pop.RequestOpen();
    pop.RequestFocusSearch();
}

static void HideWindow(GLFWwindow* window) {
    glfwHideWindow(window);
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
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(kPopupWidth, kPopupHeight, "Emoji Pop", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        ReleaseInstance();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    EmojiPop pop;
    pop.tone = LoadTonePreference();
    pop.theme = LoadThemePreference();
    ApplyTheme(pop.theme);
    LoadRecents(pop.recents, &pop.recent_count);
    pop.on_pick = [window](const char* glyph) {
        std::printf("picked: %s\n", glyph);
        HideWindow(window);
    };

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    float content_scale_x = 1.f, content_scale_y = 1.f;
    glfwGetWindowContentScale(window, &content_scale_x, &content_scale_y);
    io.FontGlobalScale = content_scale_y;

    LoadFonts();

    ShowWindow(window, pop);

    while (!glfwWindowShouldClose(window)) {
        PollInstanceServer([&]() { ShowWindow(window, pop); });

        const bool visible = glfwGetWindowAttrib(window, GLFW_VISIBLE) != GLFW_FALSE;
        if (!visible) {
            glfwWaitEventsTimeout(0.05);
            continue;
        }

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (!pop.Draw())
            HideWindow(window);

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        float clear_color[4];
        GetThemeClearColor(clear_color);
        glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    DestroyEmojiTextures();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    ReleaseInstance();
    return 0;
}
