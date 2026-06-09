#include "emoji_pop.h"
#include "config.h"
#include "emoji_data.h"
#include "fonts.h"
#include "tone_icons.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace {

static const char* kToneMods[] = {
    "",
    "\xF0\x9F\x8F\xBB", // U+1F3FB
    "\xF0\x9F\x8F\xBC", // U+1F3FC
    "\xF0\x9F\x8F\xBD", // U+1F3FD
    "\xF0\x9F\x8F\xBE", // U+1F3FE
    "\xF0\x9F\x8F\xBF", // U+1F3FF
};

bool EqualsCI(const char* a, const char* b) {
    if (!a || !b) return a == b;
    while (*a && *b) {
        if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b))
            return false;
        ++a;
        ++b;
    }
    return *a == *b;
}

bool StartsWithCI(const char* hay, const char* needle) {
    if (!needle[0]) return true;
    if (!hay) return false;
    while (*needle) {
        if (!*hay || std::tolower((unsigned char)*hay) != std::tolower((unsigned char)*needle))
            return false;
        ++hay;
        ++needle;
    }
    return true;
}

bool StartsWithWordCI(const char* hay, const char* word) {
    if (!StartsWithCI(hay, word)) return false;
    const char next = hay[std::strlen(word)];
    return next == '\0' || !std::isalnum((unsigned char)next);
}

bool ContainsWordCI(const char* hay, const char* word) {
    if (!word[0]) return true;
    if (!hay) return false;
    for (const char* h = hay; *h; ++h) {
        const char* p = h;
        const char* w = word;
        while (*w && *p && std::tolower((unsigned char)*w) == std::tolower((unsigned char)*p)) {
            ++w;
            ++p;
        }
        if (*w) continue;
        const bool start_ok = h == hay || !std::isalnum((unsigned char)h[-1]);
        const bool end_ok = !*p || !std::isalnum((unsigned char)*p);
        if (start_ok && end_ok) return true;
    }
    return false;
}

bool ContainsCI(const char* hay, const char* needle) {
    if (!needle[0]) return true;
    if (!hay || !hay[0]) return false;
    for (const char* h = hay; *h; ++h) {
        const char* n = needle;
        const char* p = h;
        while (*n && *p && std::tolower((unsigned char)*n) == std::tolower((unsigned char)*p)) {
            ++n; ++p;
        }
        if (!*n) return true;
    }
    return false;
}

void NormalizeSearch(const char* in, char* out, size_t cap) {
    if (!cap) return;
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < cap; ++i) {
        char c = in[i];
        if (c == ':' || c == '_')
            c = ' ';
        if (c == ' ') {
            if (j == 0 || out[j - 1] == ' ')
                continue;
        }
        out[j++] = c;
    }
    while (j > 0 && out[j - 1] == ' ')
        --j;
    out[j] = '\0';
}

bool FieldMatchesToken(const char* field, const char* token) {
    return field && field[0] && ContainsCI(field, token);
}

int FieldTokenScore(const char* field, const char* token, int word_score, int substr_score) {
    if (!field || !field[0]) return 0;
    if (ContainsWordCI(field, token)) return word_score;
    if (ContainsCI(field, token)) return substr_score;
    return 0;
}

int TokenizeSearch(const char* query, char tokens[][64], int max_tokens) {
    int count = 0;
    for (const char* p = query; *p && count < max_tokens; ) {
        while (*p && std::isspace((unsigned char)*p))
            ++p;
        if (!*p) break;

        const char* start = p;
        while (*p && !std::isspace((unsigned char)*p))
            ++p;
        const size_t len = (size_t)(p - start);
        if (len == 1 && !std::isalnum((unsigned char)*start))
            continue;

        const size_t copy = std::min(len, sizeof(tokens[0]) - 1);
        std::memcpy(tokens[count], start, copy);
        tokens[count][copy] = '\0';
        ++count;
    }
    return count;
}

bool TokenMatchesFields(const Emoji& e, const char* token, bool word_only) {
    if (ContainsWordCI(e.name, token) || ContainsWordCI(e.keywords, token) || ContainsWordCI(e.aliases, token))
        return true;
    if (word_only)
        return false;
    return FieldMatchesToken(e.name, token) ||
           FieldMatchesToken(e.keywords, token) ||
           FieldMatchesToken(e.aliases, token);
}

int ScoreEmoji(const Emoji& e, const char* query) {
    if (!query[0]) return 1;

    const bool phrase_query = std::strchr(query, ' ') != nullptr;
    if (phrase_query && EqualsCI(e.name, query))
        return 1000000;

    int score = 0;
    if (phrase_query && ContainsCI(e.name, query))
        score += 500000;

    char tokens[8][64];
    const int token_count = TokenizeSearch(query, tokens, 8);
    if (token_count == 0)
        return score;

    for (int i = 0; i < token_count; ++i) {
        if (!TokenMatchesFields(e, tokens[i], token_count > 1))
            return 0;
    }

    if (token_count == 1 && !phrase_query) {
        const char* token = tokens[0];
        if (EqualsCI(e.name, token)) return 900000;

        if (ContainsWordCI(e.name, token)) {
            int name_score = 800000;
            if (StartsWithWordCI(e.name, token)) name_score += 50000;
            name_score += FieldTokenScore(e.keywords, token, 15000, 0);
            name_score += FieldTokenScore(e.aliases, token, 10000, 0);
            return name_score;
        }

        int score = FieldTokenScore(e.keywords, token, 50000, 5000);
        score += FieldTokenScore(e.aliases, token, 20000, 3000);
        score += FieldTokenScore(e.name, token, 0, 8000);
        return score;
    }

    for (int i = 0; i < token_count; ++i) {
        const char* token = tokens[i];
        int best = FieldTokenScore(e.name, token, 40000, 15000);
        if (!best) best = FieldTokenScore(e.keywords, token, 8000, 3000);
        if (!best) best = FieldTokenScore(e.aliases, token, 3000, 1000);
        score += best;
    }

    bool all_words_in_name = true;
    for (int i = 0; i < token_count; ++i) {
        if (!ContainsWordCI(e.name, tokens[i])) {
            all_words_in_name = false;
            break;
        }
    }
    if (all_words_in_name) score += 200000;

    return score;
}

bool EmojiMatchesToken(const Emoji& e, const char* token) {
    if (!token[0]) return true;
    return FieldMatchesToken(e.name, token) ||
           FieldMatchesToken(e.keywords, token) ||
           FieldMatchesToken(e.aliases, token);
}

void BuildGlyph(const Emoji& e, int tone, char* out, size_t cap) {
    std::snprintf(out, cap, "%s%s", e.glyph, (e.skin_tone && tone > 0) ? kToneMods[tone] : "");
}

static int SearchInputCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag != ImGuiInputTextFlags_CallbackAlways)
        return 0;
    if ((ImGui::GetIO().KeyMods & ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_A))
        data->SelectAll();
    return 0;
}

bool EmojiButton(const char* id, const char* glyph, const ImVec2& size) {
    const ToneIcon& icon = GetCachedEmojiTexture(glyph);
    if (icon.tex) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.f, 2.f));
        const ImVec2 img(size.x - 4.f, size.y - 4.f);
        const bool pressed = ImGui::ImageButton(id, (ImTextureID)(intptr_t)icon.tex, img);
        ImGui::PopStyleVar();
        return pressed;
    }
    char display[32];
    StripVs16(glyph, display, sizeof(display));
    return ImGui::Button(display, size);
}

void FormatShortcode(const char* name, char* out, size_t cap) {
    if (cap < 3) {
        out[0] = '\0';
        return;
    }
    char* d = out;
    const char* end = out + cap - 1;
    *d++ = ':';
    for (const char* s = name; *s && d < end - 1; ++s) {
        const unsigned char c = (unsigned char)*s;
        if (std::isspace(c)) {
            if (d > out + 1 && *(d - 1) != '_')
                *d++ = '_';
        } else if (std::isalnum(c)) {
            *d++ = (char)std::tolower(c);
        }
    }
    while (d > out + 1 && *(d - 1) == '_')
        --d;
    if (d < end)
        *d++ = ':';
    *d = '\0';
}

void DrawEmojiImage(const char* glyph, ImVec2 pos, ImVec2 size) {
    const ToneIcon& icon = GetCachedEmojiTexture(glyph);
    if (icon.tex) {
        ImGui::GetWindowDrawList()->AddImage(
            (ImTextureID)(intptr_t)icon.tex, pos, ImVec2(pos.x + size.x, pos.y + size.y));
        return;
    }
    char display[32];
    StripVs16(glyph, display, sizeof(display));
    ImGui::GetWindowDrawList()->AddText(pos, IM_COL32_WHITE, display);
}

void StripSkinToneSuffix(char* glyph) {
    const size_t len = std::strlen(glyph);
    for (int t = 1; t < 6; ++t) {
        const size_t mod_len = std::strlen(kToneMods[t]);
        if (len >= mod_len && std::strcmp(glyph + len - mod_len, kToneMods[t]) == 0) {
            glyph[len - mod_len] = '\0';
            return;
        }
    }
}

const Emoji* FindEmojiByGlyph(const char* glyph) {
    if (!glyph || !glyph[0]) return nullptr;
    char normalized[32];
    StripVs16(glyph, normalized, sizeof(normalized));
    StripSkinToneSuffix(normalized);
    for (int i = 0; i < kEmojiCount; ++i) {
        char base[32];
        StripVs16(kEmojis[i].glyph, base, sizeof(base));
        if (std::strcmp(normalized, base) == 0)
            return &kEmojis[i];
    }
    return nullptr;
}

bool EmojiCellHovered() {
    if (ImGui::IsItemFocused())
        return true;
    const ImVec2 rmin = ImGui::GetItemRectMin();
    const ImVec2 rmax = ImGui::GetItemRectMax();
    return ImGui::IsMouseHoveringRect(rmin, rmax, false);
}

void SetEmojiPreview(const char* glyph, const Emoji* emoji, char* preview_glyph, const Emoji** hover) {
    std::strncpy(preview_glyph, glyph, 31);
    preview_glyph[31] = '\0';
    *hover = emoji ? emoji : FindEmojiByGlyph(glyph);
}

} // namespace

void EmojiPop::Filter() {
    struct ScoredResult {
        uint16_t index;
        int score;
    };

    char normalized[128];
    NormalizeSearch(search, normalized, sizeof(normalized));

    ScoredResult scored[1024];
    int count = 0;
    for (int i = 0; i < kEmojiCount; ++i) {
        const int score = ScoreEmoji(kEmojis[i], normalized);
        if (score <= 0)
            continue;
        scored[count++] = {(uint16_t)i, score};
    }

    std::sort(scored, scored + count, [](const ScoredResult& a, const ScoredResult& b) {
        if (a.score != b.score)
            return a.score > b.score;
        return a.index < b.index;
    });

    result_count = count;
    for (int i = 0; i < count; ++i)
        results[i] = scored[i].index;
}

void EmojiPop::Pick(const Emoji& e) {
    char buf[32];
    BuildGlyph(e, tone, buf, sizeof(buf));
    PickRaw(buf);
}

void EmojiPop::PickRaw(const char* glyph) {
    if (!glyph[0]) return;

    ImGui::SetClipboardText(glyph);

    for (int i = 0; i < recent_count; ++i) {
        if (std::strcmp(recents[i], glyph) == 0) {
            for (int j = i; j > 0; --j)
                std::strcpy(recents[j], recents[j - 1]);
            std::strcpy(recents[0], glyph);
            SaveRecents(tone, recents, recent_count);
            if (on_pick) on_pick(glyph);
            return;
        }
    }
    if (recent_count < kMaxRecents) ++recent_count;
    for (int j = recent_count - 1; j > 0; --j)
        std::strcpy(recents[j], recents[j - 1]);
    std::strcpy(recents[0], glyph);
    SaveRecents(tone, recents, recent_count);
    if (on_pick) on_pick(glyph);
}

void EmojiPop::Draw() {
    if (!initialized) {
        Filter();
        initialized = true;
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("Emoji Pop", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F,
            ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteOverFocused | ImGuiInputFlags_RouteOverActive))
        RequestFocusSearch();

    const ImGuiStyle& style = ImGui::GetStyle();
    const float frame_h = ImGui::GetFrameHeight();
    const float icon_sz_y = 16.f;
    const float arrow_w = frame_h;
    const float tone_w = 16.f + arrow_w + style.FramePadding.x * 2.f;
    const float gap = style.ItemSpacing.x;
    const float preview_h = kEmojiPreviewBarH;

    if (focus_search) {
        ImGui::SetKeyboardFocusHere();
        focus_search = false;
    }
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - tone_w - gap);
    ImGui::InputTextWithHint("##search", "Search emoji...", search, sizeof(search),
        ImGuiInputTextFlags_CallbackAlways, SearchInputCallback);
    if (std::strcmp(search, last_search) != 0) {
        std::strcpy(last_search, search);
        Filter();
    }
    if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_DownArrow) && result_count > 0) {
        ImGui::ClearActiveID();
        ImGui::NavMoveRequestCancel();
        focus_first_emoji = true;
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(tone_w);
    const float row_h = icon_sz_y + 4.f;
    const ImVec2 icon_sz(16.f, icon_sz_y);
    const float icon_pad = style.FramePadding.x;
    if (ImGui::BeginCombo("##tone", " ")) {
        for (int t = 0; t < 6; ++t) {
            if (!g_tone_icons[t].tex) continue;
            ImGui::PushID(t);
            const bool selected = tone == t;
            if (ImGui::Selectable("##tone_row", selected, 0, ImVec2(tone_w, row_h)) && tone != t) {
                tone = t;
                SaveTonePreference(tone);
            }
            const ImVec2 rmin = ImGui::GetItemRectMin();
            ImGui::GetWindowDrawList()->AddImage(
                (ImTextureID)(intptr_t)g_tone_icons[t].tex,
                ImVec2(rmin.x + icon_pad, rmin.y + (row_h - icon_sz.y) * 0.5f),
                ImVec2(rmin.x + icon_pad + icon_sz.x, rmin.y + (row_h - icon_sz.y) * 0.5f + icon_sz.y));
            if (selected)
                ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (g_tone_icons[tone].tex) {
        const ImVec2 rmin = ImGui::GetItemRectMin();
        const ImVec2 rmax = ImGui::GetItemRectMax();
        const float py = rmin.y + (rmax.y - rmin.y - icon_sz.y) * 0.5f;
        ImGui::GetWindowDrawList()->AddImage(
            (ImTextureID)(intptr_t)g_tone_icons[tone].tex,
            ImVec2(rmin.x + icon_pad, py),
            ImVec2(rmin.x + icon_pad + icon_sz.x, py + icon_sz.y));
    }

    ImGui::Spacing();
    ImGui::Separator();
    char preview_glyph[32] = {};
    const Emoji* hover = nullptr;
    struct RecentHit {
        ImVec2 min;
        ImVec2 max;
        char glyph[kRecentGlyphSize];
    };
    RecentHit recent_hits[kMaxRecents] = {};
    if (recent_count > 0) {
        for (int i = 0; i < recent_count; ++i) {
            if (i > 0) ImGui::SameLine();
            ImGui::PushID(i);
            if (EmojiButton("##recent", recents[i], ImVec2(kEmojiRecentSize, kEmojiRecentSize)))
                PickRaw(recents[i]);
            recent_hits[i].min = ImGui::GetItemRectMin();
            recent_hits[i].max = ImGui::GetItemRectMax();
            std::strncpy(recent_hits[i].glyph, recents[i], sizeof(recent_hits[i].glyph) - 1);
            recent_hits[i].glyph[sizeof(recent_hits[i].glyph) - 1] = '\0';
            ImGui::PopID();
        }
        ImGui::Separator();
    }

    ImGui::BeginChild("##grid", ImVec2(0, -(preview_h + style.ItemSpacing.y + 1.f)), false);
    const float spacing = style.ItemSpacing.x;
    const float cell_h = kEmojiCellSize;
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const int cols = std::max(1, (int)((avail_w + spacing) / (cell_h + spacing)));
    const float cell_w = (avail_w - spacing * (cols - 1)) / cols;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));
    char display_glyph[32];
    for (int i = 0; i < result_count; ++i) {
        const Emoji& e = kEmojis[results[i]];
        BuildGlyph(e, tone, display_glyph, sizeof(display_glyph));
        if (i % cols != 0) ImGui::SameLine();
        ImGui::PushID(i);
        if (EmojiButton("##em", display_glyph, ImVec2(cell_w, cell_h))) Pick(e);
        if (focus_first_emoji && i == 0) {
            ImGui::FocusItem();
            ImGui::SetNavCursorVisible(true);
            focus_first_emoji = false;
        }
        if (EmojiCellHovered())
            SetEmojiPreview(display_glyph, &e, preview_glyph, &hover);
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();

    if (!preview_glyph[0]) {
        for (int i = 0; i < recent_count; ++i) {
            if (!ImGui::IsMouseHoveringRect(recent_hits[i].min, recent_hits[i].max, false))
                continue;
            SetEmojiPreview(recent_hits[i].glyph, nullptr, preview_glyph, &hover);
            break;
        }
    }

    ImGui::Separator();
    const float emoji_sz = kEmojiPreviewSize;
    const float pad = style.FramePadding.x;
    const float preview_w = ImGui::GetContentRegionAvail().x;
    const ImVec2 bar_min = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(preview_w, preview_h));
    if (preview_glyph[0]) {
        DrawEmojiImage(preview_glyph, bar_min, ImVec2(emoji_sz, emoji_sz));
        ImGui::SetCursorScreenPos(ImVec2(bar_min.x + emoji_sz + pad, bar_min.y + 8.f));
        if (g_font_ui) ImGui::PushFont(g_font_ui);
        ImGui::BeginGroup();
        if (hover) {
            ImGui::TextUnformatted(hover->name);
            char shortcode[128];
            FormatShortcode(hover->name, shortcode, sizeof(shortcode));
            ImGui::TextDisabled("%s", shortcode);
        }
        ImGui::EndGroup();
        if (g_font_ui) ImGui::PopFont();
    } else {
        if (g_font_ui) ImGui::PushFont(g_font_ui);
        ImGui::SetCursorScreenPos(
            ImVec2(bar_min.x + pad, bar_min.y + (preview_h - ImGui::GetTextLineHeight()) * 0.5f));
        ImGui::TextDisabled("Hover an emoji");
        if (g_font_ui) ImGui::PopFont();
    }

    ImGui::End();
}
