#include "tone_icons.h"
#include "font_paths.h"
#include "fonts.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_OTSVG_H
#include <plutosvg-ft.h>
#include <hb.h>
#include <hb-ft.h>

#include <GL/gl.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

static const int kRasterPx = kEmojiNativePx;

static std::unordered_map<std::string, ToneIcon> g_emoji_cache;
static ToneIcon g_empty_icon = {};

static const uint8_t* Utf8Decode(const uint8_t* s, uint32_t* cp) {
    if (!s || !*s) { *cp = 0; return s; }
    if (s[0] < 0x80) { *cp = s[0]; return s + 1; }
    if ((s[0] & 0xE0) == 0xC0) {
        *cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        return s + 2;
    }
    if ((s[0] & 0xF0) == 0xE0) {
        *cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return s + 3;
    }
    *cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    return s + 4;
}

bool NeedsShapedDisplay(const char* glyph) {
    for (const uint8_t* p = (const uint8_t*)glyph; *p; ) {
        uint32_t cp;
        p = Utf8Decode(p, &cp);
        if (cp == 0x200D)
            return true;
    }
    return false;
}

void StripVs16(const char* in, char* out, size_t cap) {
    char* d = out;
    const char* end = out + cap - 1;
    for (const uint8_t* p = (const uint8_t*)in; *p && d < end; ) {
        uint32_t cp;
        const uint8_t* next = Utf8Decode(p, &cp);
        if (cp != 0xFE0F) {
            while (p < next && d < end)
                *d++ = (char)*p++;
        } else {
            p = next;
        }
    }
    *d = '\0';
}

static const char* kToneMods[] = {
    "",
    "\xF0\x9F\x8F\xBB", // U+1F3FB
    "\xF0\x9F\x8F\xBC", // U+1F3FC
    "\xF0\x9F\x8F\xBD", // U+1F3FD
    "\xF0\x9F\x8F\xBE", // U+1F3FE
    "\xF0\x9F\x8F\xBF", // U+1F3FF
};

static bool IsFitzpatrick(uint32_t cp) {
    return cp >= 0x1F3FB && cp <= 0x1F3FF;
}

static bool IsEmojiModifierBase(uint32_t cp) {
    static const struct {
        uint32_t start;
        uint32_t end;
    } kRanges[] = {
        {0x261D, 0x261D}, {0x26F9, 0x26F9}, {0x270A, 0x270D},
        {0x1F385, 0x1F385}, {0x1F3C3, 0x1F3C4}, {0x1F3CA, 0x1F3CA},
        {0x1F442, 0x1F443}, {0x1F446, 0x1F450}, {0x1F466, 0x1F469},
        {0x1F46E, 0x1F46E}, {0x1F470, 0x1F478}, {0x1F47C, 0x1F47C},
        {0x1F481, 0x1F483}, {0x1F485, 0x1F487}, {0x1F48F, 0x1F48F}, {0x1F491, 0x1F491},
        {0x1F4AA, 0x1F4AA}, {0x1F574, 0x1F575}, {0x1F57A, 0x1F57A}, {0x1F590, 0x1F590},
        {0x1F595, 0x1F596}, {0x1F645, 0x1F647}, {0x1F64B, 0x1F64B}, {0x1F64D, 0x1F64E},
        {0x1F6A3, 0x1F6A3}, {0x1F6B4, 0x1F6B6}, {0x1F6C0, 0x1F6C0},
        {0x1F918, 0x1F91C}, {0x1F91E, 0x1F91E}, {0x1F926, 0x1F926}, {0x1F930, 0x1F939}, {0x1F93D, 0x1F93E},
        {0x1F977, 0x1F977}, {0x1F9B5, 0x1F9B6}, {0x1F9D1, 0x1F9DD},
        {0x1FAC3, 0x1FAC5}, {0x1FAF0, 0x1FAF8},
    };
    for (const auto& range : kRanges) {
        if (cp >= range.start && cp <= range.end)
            return true;
    }
    return false;
}

static bool AppendBytes(char* out, size_t cap, size_t* len, const char* bytes, size_t count) {
    if (*len + count + 1 > cap)
        return false;
    std::memcpy(out + *len, bytes, count);
    *len += count;
    out[*len] = '\0';
    return true;
}

void ApplySkinTone(const char* glyph, int tone, char* out, size_t cap) {
    if (!out || cap == 0)
        return;
    if (!glyph) {
        out[0] = '\0';
        return;
    }
    if (tone <= 0 || tone > 5 || !kToneMods[tone][0]) {
        std::snprintf(out, cap, "%s", glyph);
        return;
    }

    const char* mod = kToneMods[tone];
    size_t len = 0;
    out[0] = '\0';

    for (const uint8_t* p = (const uint8_t*)glyph; *p; ) {
        const uint8_t* cluster_start = p;
        uint32_t cp = 0;
        p = Utf8Decode(p, &cp);
        if (!cp)
            break;

        bool has_fitz = false;
        while (*p) {
            uint32_t next_cp = 0;
            const uint8_t* next = Utf8Decode(p, &next_cp);
            if (next_cp == 0xFE0F || IsFitzpatrick(next_cp)) {
                if (IsFitzpatrick(next_cp))
                    has_fitz = true;
                p = next;
                continue;
            }
            break;
        }

        if (!AppendBytes(out, cap, &len, (const char*)cluster_start, (size_t)(p - cluster_start)))
            goto fallback;

        uint32_t next_cp = 0;
        if (*p)
            Utf8Decode(p, &next_cp);
        if (!has_fitz && IsEmojiModifierBase(cp) && (!next_cp || next_cp == 0x200D)) {
            if (!AppendBytes(out, cap, &len, mod, std::strlen(mod)))
                goto fallback;
        }
    }

    if (len == 0)
        goto fallback;
    return;

fallback:
    std::snprintf(out, cap, "%s", glyph);
}

static void BlitGlyph(FT_GlyphSlot slot, int x, int y, std::vector<uint8_t>& out, int w, int h) {
    const FT_Bitmap& bmp = slot->bitmap;
    if (!bmp.buffer || bmp.pixel_mode != FT_PIXEL_MODE_BGRA)
        return;
    for (unsigned int row = 0; row < bmp.rows; ++row) {
        for (unsigned int col = 0; col < bmp.width; ++col) {
            const int dx = x + (int)col;
            const int dy = y + (int)row;
            if (dx < 0 || dy < 0 || dx >= w || dy >= h)
                continue;
            const uint8_t* p = bmp.buffer + row * bmp.pitch + col * 4;
            const uint8_t sr = p[2], sg = p[1], sb = p[0], sa = p[3];
            if (!sa) continue;
            uint8_t* d = &out[(dy * w + dx) * 4];
            const float src_a = sa / 255.f;
            const float dst_a = d[3] / 255.f;
            const float out_a = src_a + dst_a * (1.f - src_a);
            if (out_a <= 0.f) continue;
            auto blend = [&](uint8_t sc, uint8_t dc) {
                return uint8_t((sc * src_a + dc * dst_a * (1.f - src_a)) / out_a);
            };
            d[0] = blend(sr, d[0]);
            d[1] = blend(sg, d[1]);
            d[2] = blend(sb, d[2]);
            d[3] = uint8_t(out_a * 255.f);
        }
    }
}

struct EmojiFont {
    FT_Face face = nullptr;
    hb_font_t* hb = nullptr;
    int pixel_size = 0;
    bool size_set = false;
};

static FT_Library g_ft = nullptr;
static EmojiFont g_svg_font;
static EmojiFont g_fallback_font;

static bool EnsureFreetype() {
    if (g_ft)
        return true;
    if (FT_Init_FreeType(&g_ft))
        return false;
    if (FT_Property_Set(g_ft, "ot-svg", "svg-hooks", &plutosvg_ft_hooks)) {
        std::fprintf(stderr, "emoji: failed to set SVG renderer hooks\n");
        return false;
    }
    return true;
}

static bool EnsureEmojiFont(EmojiFont& font, const char* path) {
    if (!EnsureFreetype())
        return false;
    if (!font.face && FT_New_Face(g_ft, path, 0, &font.face))
        return false;
    if (!font.hb)
        font.hb = hb_ft_font_create(font.face, nullptr);
    return font.hb != nullptr;
}

static bool SetEmojiFontSize(EmojiFont& font, int px) {
    if (!font.face)
        return false;
    if (font.face->num_fixed_sizes > 0) {
        if (font.size_set)
            return true;
        if (FT_Select_Size(font.face, 0))
            return false;
        hb_ft_font_changed(font.hb);
        font.size_set = true;
        return true;
    }
    if (font.size_set && font.pixel_size == px)
        return true;
    FT_Size_RequestRec req{};
    req.type = FT_SIZE_REQUEST_TYPE_NOMINAL;
    req.height = px * 64;
    if (FT_Request_Size(font.face, &req))
        return false;
    hb_ft_font_changed(font.hb);
    font.pixel_size = px;
    font.size_set = true;
    return true;
}

static bool TryKeycapGlyphId(FT_Face face, const char* utf8, unsigned* out_gid) {
    if (!utf8 || !out_gid)
        return false;

    const uint8_t* p = (const uint8_t*)utf8;
    uint32_t base_cp = 0;
    p = Utf8Decode(p, &base_cp);
    const bool valid_base =
        (base_cp >= '0' && base_cp <= '9') || base_cp == '#' || base_cp == '*';
    if (!valid_base)
        return false;

    uint32_t cp = 0;
    if (*p) {
        p = Utf8Decode(p, &cp);
        if (cp == 0xFE0F && *p)
            p = Utf8Decode(p, &cp);
    }
    if (cp != 0x20E3 || *p)
        return false;

    char name[16];
    std::snprintf(name, sizeof(name), "%x-20e3", base_cp);

    if (!face)
        return false;
    const unsigned gid = FT_Get_Name_Index(face, name);
    if (!gid)
        return false;

    char actual[64];
    if (FT_Get_Glyph_Name(face, gid, actual, sizeof(actual)))
        return false;
    if (std::strcmp(actual, name) != 0)
        return false;

    *out_gid = gid;
    return true;
}

static bool RasterizeGlyphRun(FT_Face face, const unsigned* gids, unsigned count,
                              const hb_glyph_position_t* pos, std::vector<uint8_t>& out,
                              int& out_w, int& out_h) {
    if (!count)
        return false;

    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    int pen_x = 0, pen_y = 0;
    bool first = true;
    for (unsigned i = 0; i < count; ++i) {
        if (FT_Load_Glyph(face, gids[i], FT_LOAD_COLOR))
            continue;
        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL))
            continue;
        const int x = pen_x + ((pos ? pos[i].x_offset : 0) >> 6) + (int)face->glyph->bitmap_left;
        const int y = pen_y - ((pos ? pos[i].y_offset : 0) >> 6) - (int)face->glyph->bitmap_top;
        const int rw = (int)face->glyph->bitmap.width;
        const int rh = (int)face->glyph->bitmap.rows;
        if (!rw || !rh)
            continue;
        if (first) {
            min_x = x;
            min_y = y;
            max_x = x + rw;
            max_y = y + rh;
            first = false;
        } else {
            min_x = std::min(min_x, x);
            min_y = std::min(min_y, y);
            max_x = std::max(max_x, x + rw);
            max_y = std::max(max_y, y + rh);
        }
        pen_x += (pos ? pos[i].x_advance : 0) >> 6;
        pen_y += (pos ? pos[i].y_advance : 0) >> 6;
    }
    if (first)
        return false;

    const int pad = 2;
    out_w = std::max(1, max_x - min_x + pad * 2);
    out_h = std::max(1, max_y - min_y + pad * 2);
    out.assign(out_w * out_h * 4, 0);

    pen_x = pen_y = 0;
    bool any_pixel = false;
    for (unsigned i = 0; i < count; ++i) {
        if (FT_Load_Glyph(face, gids[i], FT_LOAD_COLOR))
            continue;
        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL))
            continue;
        const int x = pen_x + ((pos ? pos[i].x_offset : 0) >> 6) + (int)face->glyph->bitmap_left - min_x + pad;
        const int y = pen_y - ((pos ? pos[i].y_offset : 0) >> 6) - (int)face->glyph->bitmap_top - min_y + pad;
        if (face->glyph->bitmap.buffer)
            any_pixel = true;
        BlitGlyph(face->glyph, x, y, out, out_w, out_h);
        pen_x += (pos ? pos[i].x_advance : 0) >> 6;
        pen_y += (pos ? pos[i].y_advance : 0) >> 6;
    }
    return any_pixel;
}

static bool RasterizeWithFont(EmojiFont& font, const char* utf8, std::vector<uint8_t>& out,
                              int& out_w, int& out_h, int px, bool try_keycap) {
    if (!SetEmojiFontSize(font, px))
        return false;

    FT_Face face = font.face;

    unsigned keycap_gid = 0;
    if (try_keycap && TryKeycapGlyphId(face, utf8, &keycap_gid))
        return RasterizeGlyphRun(face, &keycap_gid, 1, nullptr, out, out_w, out_h);

    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, utf8, -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(font.hb, buf, nullptr, 0);

    unsigned count = 0;
    hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buf, &count);
    hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buf, &count);
    if (!count) {
        hb_buffer_destroy(buf);
        return false;
    }

    std::vector<unsigned> gids(count);
    for (unsigned i = 0; i < count; ++i)
        gids[i] = info[i].codepoint;

    const bool ok = RasterizeGlyphRun(face, gids.data(), count, pos, out, out_w, out_h);
    hb_buffer_destroy(buf);
    return ok;
}

static bool Rasterize(const char* utf8, std::vector<uint8_t>& out, int& out_w, int& out_h, int target_px) {
    const int px = target_px > 0 ? target_px : kRasterPx;

    if (EnsureEmojiFont(g_svg_font, kTwemojiFont) &&
        RasterizeWithFont(g_svg_font, utf8, out, out_w, out_h, px, true))
        return true;

    if (EnsureEmojiFont(g_fallback_font, kTwemojiFallbackFont) &&
        RasterizeWithFont(g_fallback_font, utf8, out, out_w, out_h, px, false))
        return true;

    return false;
}

static uint8_t SampleBilinear(const std::vector<uint8_t>& px, int w, int h, float fx, float fy, int ch) {
    fx = std::max(0.f, std::min(fx, (float)(w - 1)));
    fy = std::max(0.f, std::min(fy, (float)(h - 1)));
    const int x0 = (int)fx;
    const int y0 = (int)fy;
    const int x1 = std::min(x0 + 1, w - 1);
    const int y1 = std::min(y0 + 1, h - 1);
    const float tx = fx - (float)x0;
    const float ty = fy - (float)y0;
    auto at = [&](int x, int y) { return (float)px[(y * w + x) * 4 + ch]; };
    const float v0 = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * tx;
    const float v1 = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * tx;
    return (uint8_t)(v0 + (v1 - v0) * ty + 0.5f);
}

static void FitToSquare(std::vector<uint8_t>& px, int& w, int& h, int size) {
    if (w <= 0 || h <= 0) return;
    const float scale = std::min((float)size / w, (float)size / h);
    const int nw = std::max(1, (int)(w * scale + 0.5f));
    const int nh = std::max(1, (int)(h * scale + 0.5f));
    std::vector<uint8_t> scaled(nw * nh * 4);
    for (int y = 0; y < nh; ++y) {
        const float sy = (y + 0.5f) * h / nh - 0.5f;
        for (int x = 0; x < nw; ++x) {
            const float sx = (x + 0.5f) * w / nw - 0.5f;
            uint8_t* d = &scaled[(y * nw + x) * 4];
            for (int c = 0; c < 4; ++c)
                d[c] = SampleBilinear(px, w, h, sx, sy, c);
        }
    }
    std::vector<uint8_t> out(size * size * 4, 0);
    const int ox = (size - nw) / 2;
    const int oy = (size - nh) / 2;
    for (int y = 0; y < nh; ++y) {
        for (int x = 0; x < nw; ++x) {
            const uint8_t* s = &scaled[(y * nw + x) * 4];
            uint8_t* d = &out[((oy + y) * size + (ox + x)) * 4];
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
        }
    }
    px = std::move(out);
    w = h = size;
}

static unsigned Upload(const std::vector<uint8_t>& px, int w, int h) {
    unsigned tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    return tex;
}

const ToneIcon& GetCachedEmojiTexture(const char* glyph, int target_px) {
    if (!glyph || !glyph[0]) return g_empty_icon;
    const std::string key = std::string(glyph) + "@" + std::to_string(target_px);
    auto it = g_emoji_cache.find(key);
    if (it != g_emoji_cache.end()) return it->second;

    std::vector<uint8_t> px;
    int w = 0, h = 0;
    const int px_size = target_px > 0 ? target_px : kRasterPx;
    if (!Rasterize(glyph, px, w, h, px_size))
        return g_empty_icon;

    FitToSquare(px, w, h, px_size);

    ToneIcon icon;
    icon.w = w;
    icon.h = h;
    icon.tex = Upload(px, w, h);
    auto [ins, _] = g_emoji_cache.emplace(key, icon);
    return ins->second;
}

void DestroyEmojiTextures() {
    std::vector<unsigned> ids;
    for (auto& [_, icon] : g_emoji_cache) {
        if (icon.tex)
            ids.push_back(icon.tex);
    }
    g_emoji_cache.clear();
    if (!ids.empty())
        glDeleteTextures((int)ids.size(), ids.data());

    auto destroy_font = [](EmojiFont& font) {
        if (font.hb) {
            hb_font_destroy(font.hb);
            font.hb = nullptr;
        }
        if (font.face) {
            FT_Done_Face(font.face);
            font.face = nullptr;
        }
        font.pixel_size = 0;
        font.size_set = false;
    };
    destroy_font(g_svg_font);
    destroy_font(g_fallback_font);
    if (g_ft) {
        FT_Done_FreeType(g_ft);
        g_ft = nullptr;
    }
}
