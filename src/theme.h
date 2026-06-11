#pragma once

inline constexpr int kThemeDark = 0;
inline constexpr int kThemeLight = 1;

void ApplyTheme(int theme);
void GetThemeClearColor(float out[4]);
