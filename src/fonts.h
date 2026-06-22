#pragma once

struct ImFont;

inline constexpr float kUIFontSize = 18.f;
inline constexpr int kEmojiNativePx = 109;

inline constexpr float kEmojiCellSize = 32.f;
inline constexpr float kEmojiChipSize = 38.f;
inline constexpr float kEmojiPreviewSize = 64.f;
inline constexpr float kEmojiPreviewBarH = 80.f;

inline constexpr int kPopupWidth = 400;
inline constexpr int kPopupHeight = 460;

extern ImFont* g_font_ui;

void LoadFonts();
