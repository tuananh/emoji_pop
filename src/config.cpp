#include "config.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <sys/stat.h>

namespace {

std::string ConfigDir() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        return std::string(xdg) + "/emoji_pop";
    if (const char* home = std::getenv("HOME"))
        return std::string(home) + "/.config/emoji_pop";
    return "/tmp/emoji_pop";
}

std::string ConfigPath() {
    return ConfigDir() + "/settings";
}

void EnsureConfigDir() {
    const std::string dir = ConfigDir();
    mkdir(dir.c_str(), 0755);
}

void TrimNewline(char* line) {
    const size_t len = std::strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[len - 1] = '\0';
}

void WriteConfig(int tone, const char recents[kMaxRecents][kRecentGlyphSize], int count) {
    EnsureConfigDir();

    FILE* f = std::fopen(ConfigPath().c_str(), "w");
    if (!f)
        return;

    std::fprintf(f, "tone=%d\n", tone);
    for (int i = 0; i < count; ++i) {
        if (recents[i][0])
            std::fprintf(f, "recent=%s\n", recents[i]);
    }
    std::fclose(f);
}

void ReadConfig(int* tone, char recents[kMaxRecents][kRecentGlyphSize], int* count) {
    if (tone)
        *tone = 0;
    if (count)
        *count = 0;

    FILE* f = std::fopen(ConfigPath().c_str(), "r");
    if (!f)
        return;

    char line[128];
    while (std::fgets(line, sizeof(line), f)) {
        TrimNewline(line);
        if (tone && std::strncmp(line, "tone=", 5) == 0) {
            int value = 0;
            if (std::sscanf(line + 5, "%d", &value) == 1 && value >= 0 && value <= 5)
                *tone = value;
            continue;
        }
        if (count && recents && std::strncmp(line, "recent=", 7) == 0) {
            const char* glyph = line + 7;
            if (!glyph[0] || *count >= kMaxRecents)
                continue;
            std::strncpy(recents[*count], glyph, kRecentGlyphSize - 1);
            recents[*count][kRecentGlyphSize - 1] = '\0';
            ++(*count);
        }
    }
    std::fclose(f);
}

} // namespace

int LoadTonePreference() {
    int tone = 0;
    ReadConfig(&tone, nullptr, nullptr);
    return tone;
}

void SaveTonePreference(int tone) {
    if (tone < 0 || tone > 5)
        return;

    char recents[kMaxRecents][kRecentGlyphSize] = {};
    int count = 0;
    ReadConfig(nullptr, recents, &count);
    WriteConfig(tone, recents, count);
}

void LoadRecents(char recents[kMaxRecents][kRecentGlyphSize], int* count) {
    ReadConfig(nullptr, recents, count);
}

void SaveRecents(int tone, const char recents[kMaxRecents][kRecentGlyphSize], int count) {
    if (tone < 0 || tone > 5)
        tone = 0;
    if (count < 0)
        count = 0;
    if (count > kMaxRecents)
        count = kMaxRecents;
    WriteConfig(tone, recents, count);
}
