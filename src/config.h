#pragma once

inline constexpr int kMaxRecents = 16;
inline constexpr int kRecentGlyphSize = 32;

int LoadTonePreference();
void SaveTonePreference(int tone);

void LoadRecents(char recents[kMaxRecents][kRecentGlyphSize], int* count);
void SaveRecents(int tone, const char recents[kMaxRecents][kRecentGlyphSize], int count);
