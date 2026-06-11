#include "fonts.h"

#include <imgui.h>

ImFont* g_font_ui = nullptr;

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    g_font_ui = io.Fonts->AddFontDefault();
    io.FontDefault = g_font_ui;
}
