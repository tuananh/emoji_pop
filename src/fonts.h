#pragma once

struct ImFont;

inline constexpr float kUIFontSize = 18.f;
inline constexpr float kEmojiRasterSize = 109.f;
inline constexpr int kEmojiNativePx = 109;
inline constexpr int kToneIconCachePx = 64;

inline constexpr float kEmojiCellSize = 32.f;
inline constexpr float kEmojiChipSize = 38.f;
inline constexpr float kEmojiPreviewSize = 64.f;
inline constexpr float kEmojiPreviewBarH = 80.f;

extern ImFont* g_font_ui;

void LoadFonts();
