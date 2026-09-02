#pragma once

#include "config.h"

#include <cstdint>
#include <functional>

struct Emoji;

struct EmojiPop {
    char search[128] = {};
    int category = 0;
    int tone = 0;
    int theme = kThemeDark;
    int recent_count = 0;
    char recents[kMaxRecents][kRecentGlyphSize] = {};
    uint16_t results[2048] = {};
    int result_count = 0;
    bool initialized = false;
    char last_search[128] = {};
    bool focus_search = false;
    bool focus_first_emoji = false;
    bool highlight_first_result = false;
    bool open_requested = false;

    std::function<void(const char*)> on_pick;

    bool Draw();
    void RequestOpen() { open_requested = true; }
    void RequestFocusSearch() { focus_search = true; }
    void Pick(const Emoji& e);
    void PickRaw(const char* glyph);

private:
    void Filter();
};
