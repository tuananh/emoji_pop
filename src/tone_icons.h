#pragma once

#include <cstddef>

struct ToneIcon {
    unsigned int tex = 0;
    int w = 0;
    int h = 0;
};

extern ToneIcon g_tone_icons[6];

void EnsureToneIconsLoaded();
void LoadToneIcons();
void DestroyToneIcons();

bool NeedsShapedDisplay(const char* glyph);
void StripVs16(const char* in, char* out, std::size_t cap);
void ApplySkinTone(const char* glyph, int tone, char* out, std::size_t cap);
const ToneIcon& GetCachedEmojiTexture(const char* glyph);
