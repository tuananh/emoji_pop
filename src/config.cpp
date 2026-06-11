#include "config.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <sys/stat.h>

namespace {

struct Settings {
    int tone = 0;
    int theme = kThemeDark;
    char recents[kMaxRecents][kRecentGlyphSize] = {};
    int recent_count = 0;
};

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

void WriteSettings(const Settings& settings) {
    EnsureConfigDir();

    FILE* f = std::fopen(ConfigPath().c_str(), "w");
    if (!f)
        return;

    std::fprintf(f, "tone=%d\n", settings.tone);
    std::fprintf(f, "theme=%d\n", settings.theme);
    for (int i = 0; i < settings.recent_count; ++i) {
        if (settings.recents[i][0])
            std::fprintf(f, "recent=%s\n", settings.recents[i]);
    }
    std::fclose(f);
}

void ReadSettings(Settings* settings) {
    if (!settings)
        return;

    FILE* f = std::fopen(ConfigPath().c_str(), "r");
    if (!f)
        return;

    char line[128];
    while (std::fgets(line, sizeof(line), f)) {
        TrimNewline(line);
        if (std::strncmp(line, "tone=", 5) == 0) {
            int value = 0;
            if (std::sscanf(line + 5, "%d", &value) == 1 && value >= 0 && value <= 5)
                settings->tone = value;
            continue;
        }
        if (std::strncmp(line, "theme=", 6) == 0) {
            int value = 0;
            if (std::sscanf(line + 6, "%d", &value) == 1 &&
                (value == kThemeDark || value == kThemeLight))
                settings->theme = value;
            continue;
        }
        if (std::strncmp(line, "recent=", 7) == 0) {
            const char* glyph = line + 7;
            if (!glyph[0] || settings->recent_count >= kMaxRecents)
                continue;
            std::strncpy(settings->recents[settings->recent_count], glyph, kRecentGlyphSize - 1);
            settings->recents[settings->recent_count][kRecentGlyphSize - 1] = '\0';
            ++settings->recent_count;
        }
    }
    std::fclose(f);
}

} // namespace

int LoadTonePreference() {
    Settings settings;
    ReadSettings(&settings);
    return settings.tone;
}

void SaveTonePreference(int tone) {
    if (tone < 0 || tone > 5)
        return;

    Settings settings;
    ReadSettings(&settings);
    settings.tone = tone;
    WriteSettings(settings);
}

int LoadThemePreference() {
    Settings settings;
    ReadSettings(&settings);
    return settings.theme;
}

void SaveThemePreference(int theme) {
    if (theme != kThemeDark && theme != kThemeLight)
        return;

    Settings settings;
    ReadSettings(&settings);
    settings.theme = theme;
    WriteSettings(settings);
}

void LoadRecents(char recents[kMaxRecents][kRecentGlyphSize], int* count) {
    Settings settings;
    ReadSettings(&settings);
    if (count)
        *count = settings.recent_count;
    if (recents) {
        for (int i = 0; i < settings.recent_count; ++i)
            std::strcpy(recents[i], settings.recents[i]);
    }
}

void SaveRecents(int tone, const char recents[kMaxRecents][kRecentGlyphSize], int count) {
    if (tone < 0 || tone > 5)
        tone = 0;
    if (count < 0)
        count = 0;
    if (count > kMaxRecents)
        count = kMaxRecents;

    Settings settings;
    ReadSettings(&settings);
    settings.tone = tone;
    settings.recent_count = count;
    for (int i = 0; i < count; ++i)
        std::strcpy(settings.recents[i], recents[i]);
    for (int i = count; i < kMaxRecents; ++i)
        settings.recents[i][0] = '\0';
    WriteSettings(settings);
}
