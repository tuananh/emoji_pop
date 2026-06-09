#include "fonts.h"
#include "font_paths.h"

#include <imgui.h>

ImFont* g_font_ui = nullptr;

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    g_font_ui = io.Fonts->AddFontFromFileTTF(kNotoSansRegular, kUIFontSize);
    if (!g_font_ui)
        g_font_ui = io.Fonts->AddFontDefault();
    io.FontDefault = g_font_ui;
}
