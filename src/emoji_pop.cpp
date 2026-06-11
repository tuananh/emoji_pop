#include "emoji_pop.h"
#include "config.h"
#include "emoji_data.h"
#include "fonts.h"
#include "theme.h"
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
    return ContainsCI(e.name, token) || ContainsCI(e.keywords, token) || ContainsCI(e.aliases, token);
}

// 4 = exact, 3 = prefix word, 2 = word, 1 = substring, 0 = no match.
int FieldMatchScore(const char* field, const char* token) {
    if (!field || !field[0]) return 0;
    if (EqualsCI(field, token)) return 4;
    if (StartsWithWordCI(field, token)) return 3;
    if (ContainsWordCI(field, token)) return 2;
    if (ContainsCI(field, token)) return 1;
    return 0;
}

int ScoreEmoji(const Emoji& e, const char* query) {
    if (!query[0]) return 1;

    char tokens[8][64];
    const int token_count = TokenizeSearch(query, tokens, 8);
    if (token_count == 0) return 1;

    for (int i = 0; i < token_count; ++i) {
        if (!TokenMatchesFields(e, tokens[i], token_count > 1))
            return 0;
    }

    int score = 0;
    if (token_count > 1) {
        if (EqualsCI(e.name, query)) score += 1000;
        else if (ContainsCI(e.name, query)) score += 500;
    }

    for (int i = 0; i < token_count; ++i) {
        const char* token = tokens[i];
        score += FieldMatchScore(e.name, token) * 10;
        score += FieldMatchScore(e.keywords, token) * 3;
        score += FieldMatchScore(e.aliases, token);
    }

    return score;
}

void BuildGlyph(const Emoji& e, int tone, char* out, size_t cap) {
    if (!e.skin_tone || tone <= 0)
        ApplySkinTone(e.glyph, 0, out, cap);
    else
        ApplySkinTone(e.glyph, tone, out, cap);
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
    char display[64];
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
    char display[64];
    StripVs16(glyph, display, sizeof(display));
    ImGui::GetWindowDrawList()->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), display);
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
    std::strncpy(preview_glyph, glyph, sizeof(preview_glyph) - 1);
    preview_glyph[sizeof(preview_glyph) - 1] = '\0';
    *hover = emoji ? emoji : FindEmojiByGlyph(glyph);
}

} // namespace

void EmojiPop::Filter() {
    char normalized[128];
    NormalizeSearch(search, normalized, sizeof(normalized));

    if (category == kEmojiCategoryRecent) {
        result_count = 0;
        for (int i = 0; i < recent_count; ++i) {
            const Emoji* e = FindEmojiByGlyph(recents[i]);
            if (normalized[0] && (!e || ScoreEmoji(*e, normalized) <= 0))
                continue;
            results[result_count++] = (uint16_t)i;
        }
        return;
    }

    struct ScoredResult {
        uint16_t index;
        int score;
    };

    ScoredResult scored[2048];
    int count = 0;
    for (int i = 0; i < kEmojiCount; ++i) {
        if (category >= kEmojiCategoryContentBase &&
            kEmojiCategoryMap[i] != category - kEmojiCategoryContentBase)
            continue;
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
    const float theme_btn = frame_h;
    const float tone_w = 16.f + arrow_w + style.FramePadding.x * 2.f;
    const float gap = style.ItemSpacing.x;
    const float icon_pad = style.FramePadding.x;
    const float search_icon_sz = icon_sz_y;
    const float search_icon_inset = search_icon_sz + icon_pad * 2.f;
    const float preview_h = kEmojiPreviewBarH;

    if (focus_search) {
        ImGui::SetKeyboardFocusHere();
        focus_search = false;
    }
    const float content_w = ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
        ImVec2(search_icon_inset, style.FramePadding.y));
    ImGui::SetNextItemWidth(content_w - tone_w - theme_btn - gap * 2.f);
    ImGui::InputTextWithHint("##search", "Search emoji...", search, sizeof(search),
        ImGuiInputTextFlags_CallbackAlways, SearchInputCallback);
    ImGui::PopStyleVar();
    {
        const ImVec2 rmin = ImGui::GetItemRectMin();
        const ImVec2 rmax = ImGui::GetItemRectMax();
        const float py = rmin.y + (rmax.y - rmin.y - search_icon_sz) * 0.5f;
        DrawEmojiImage("🔍",
            ImVec2(rmin.x + icon_pad, py),
            ImVec2(search_icon_sz, search_icon_sz));
    }
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
    const char* theme_glyph = theme == kThemeDark ? "☀️" : "🌙";
    if (EmojiButton("##theme", theme_glyph, ImVec2(theme_btn, frame_h))) {
        theme = theme == kThemeDark ? kThemeLight : kThemeDark;
        ApplyTheme(theme);
        SaveThemePreference(theme);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(theme == kThemeDark ? "Light theme" : "Dark theme");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(tone_w);
    const float row_h = icon_sz_y + 4.f;
    const ImVec2 icon_sz(16.f, icon_sz_y);
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

    const float category_btn = kEmojiChipSize;
    ImGui::BeginChild("##categories", ImVec2(0, category_btn), false,
        ImGuiWindowFlags_HorizontalScrollbar);
    for (int c = 0; c < kEmojiCategoryCount; ++c) {
        if (c > 0) ImGui::SameLine();
        ImGui::PushID(c);
        const bool selected = category == c;
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (EmojiButton("##cat", kEmojiCategoryGlyphs[c], ImVec2(category_btn, category_btn)) &&
            category != c) {
            category = c;
            Filter();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", kEmojiCategoryLabels[c]);
        if (selected)
            ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    char preview_glyph[64] = {};
    const Emoji* hover = nullptr;

    ImGui::BeginChild("##grid", ImVec2(0, -(preview_h + style.ItemSpacing.y + 1.f)), false);
    const float spacing = style.ItemSpacing.x;
    const float cell_h = kEmojiCellSize;
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const int cols = std::max(1, (int)((avail_w + spacing) / (cell_h + spacing)));
    const float cell_w = (avail_w - spacing * (cols - 1)) / cols;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));
    char display_glyph[64];
    for (int i = 0; i < result_count; ++i) {
        if (i % cols != 0) ImGui::SameLine();
        ImGui::PushID(i);
        if (category == kEmojiCategoryRecent) {
            const char* glyph = recents[results[i]];
            if (EmojiButton("##em", glyph, ImVec2(cell_w, cell_h)))
                PickRaw(glyph);
            if (focus_first_emoji && i == 0) {
                ImGui::FocusItem();
                ImGui::SetNavCursorVisible(true);
                focus_first_emoji = false;
            }
            if (EmojiCellHovered())
                SetEmojiPreview(glyph, FindEmojiByGlyph(glyph), preview_glyph, &hover);
        } else {
            const Emoji& e = kEmojis[results[i]];
            BuildGlyph(e, tone, display_glyph, sizeof(display_glyph));
            if (EmojiButton("##em", display_glyph, ImVec2(cell_w, cell_h)))
                Pick(e);
            if (focus_first_emoji && i == 0) {
                ImGui::FocusItem();
                ImGui::SetNavCursorVisible(true);
                focus_first_emoji = false;
            }
            if (EmojiCellHovered())
                SetEmojiPreview(display_glyph, &e, preview_glyph, &hover);
        }
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();

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
