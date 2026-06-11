#pragma once

#include "theme.h"

inline constexpr int kMaxRecents = 16;
inline constexpr int kRecentGlyphSize = 32;

int LoadTonePreference();
void SaveTonePreference(int tone);

int LoadThemePreference();
void SaveThemePreference(int theme);

void LoadRecents(char recents[kMaxRecents][kRecentGlyphSize], int* count);
void SaveRecents(int tone, const char recents[kMaxRecents][kRecentGlyphSize], int count);
