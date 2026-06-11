#include "tone_icons.h"
#include "font_paths.h"
#include "fonts.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

#include <GL/gl.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

ToneIcon g_tone_icons[6] = {};

static const int kRasterPx = kEmojiNativePx;

static const char* kToneUtf8[] = {
    "\xF0\x9F\x91\x8D",
    "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBB",
    "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBC",
    "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD",
    "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBE",
    "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBF",
};

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
    int count = 0;
    for (const uint8_t* p = (const uint8_t*)glyph; *p; ) {
        uint32_t cp;
        p = Utf8Decode(p, &cp);
        if (cp == 0xFE0F) continue;
        if (cp == 0x200D) return true;
        ++count;
    }
    return count > 1;
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

static void BlitGlyph(FT_GlyphSlot slot, int x, int y, std::vector<uint8_t>& out, int w, int h) {
    const FT_Bitmap& bmp = slot->bitmap;
    if (bmp.pixel_mode != FT_PIXEL_MODE_BGRA)
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

static FT_Library g_ft = nullptr;
static FT_Face g_emoji_face = nullptr;
static hb_font_t* g_hb_font = nullptr;
static bool g_tone_icons_loaded = false;

static bool EnsureEmojiFace() {
    if (g_emoji_face)
        return true;
    if (!g_ft && FT_Init_FreeType(&g_ft))
        return false;
    if (FT_New_Face(g_ft, kTwemojiFont, 0, &g_emoji_face))
        return false;

    if (g_emoji_face->num_fixed_sizes > 0) {
        FT_Select_Size(g_emoji_face, 0);
    } else {
        FT_Size_RequestRec req{};
        req.type = FT_SIZE_REQUEST_TYPE_NOMINAL;
        req.height = kRasterPx * 64;
        FT_Request_Size(g_emoji_face, &req);
    }
    g_hb_font = hb_ft_font_create(g_emoji_face, nullptr);
    return g_hb_font != nullptr;
}

static bool Rasterize(const char* utf8, std::vector<uint8_t>& out, int& out_w, int& out_h) {
    if (!EnsureEmojiFace())
        return false;

    FT_Face face = g_emoji_face;
    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, utf8, -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(g_hb_font, buf, nullptr, 0);

    unsigned count = 0;
    hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buf, &count);
    hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buf, &count);
    if (!count) {
        hb_buffer_destroy(buf);
        return false;
    }

    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    int pen_x = 0, pen_y = 0;
    bool first = true;
    for (unsigned i = 0; i < count; ++i) {
        if (FT_Load_Glyph(face, info[i].codepoint, FT_LOAD_COLOR))
            continue;
        const int x = pen_x + (pos[i].x_offset >> 6) + (int)face->glyph->bitmap_left;
        const int y = pen_y - (pos[i].y_offset >> 6) - (int)face->glyph->bitmap_top;
        const int rw = (int)face->glyph->bitmap.width;
        const int rh = (int)face->glyph->bitmap.rows;
        if (first) {
            min_x = x; min_y = y; max_x = x + rw; max_y = y + rh;
            first = false;
        } else {
            min_x = std::min(min_x, x);
            min_y = std::min(min_y, y);
            max_x = std::max(max_x, x + rw);
            max_y = std::max(max_y, y + rh);
        }
        pen_x += pos[i].x_advance >> 6;
        pen_y += pos[i].y_advance >> 6;
    }

    const int pad = 2;
    out_w = std::max(1, max_x - min_x + pad * 2);
    out_h = std::max(1, max_y - min_y + pad * 2);
    out.assign(out_w * out_h * 4, 0);

    pen_x = pen_y = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (FT_Load_Glyph(face, info[i].codepoint, FT_LOAD_COLOR))
            continue;
        const int x = pen_x + (pos[i].x_offset >> 6) + (int)face->glyph->bitmap_left - min_x + pad;
        const int y = pen_y - (pos[i].y_offset >> 6) - (int)face->glyph->bitmap_top - min_y + pad;
        BlitGlyph(face->glyph, x, y, out, out_w, out_h);
        pen_x += pos[i].x_advance >> 6;
        pen_y += pos[i].y_advance >> 6;
    }

    hb_buffer_destroy(buf);
    return true;
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    return tex;
}

const ToneIcon& GetCachedEmojiTexture(const char* glyph) {
    if (!glyph || !glyph[0]) return g_empty_icon;
    auto it = g_emoji_cache.find(glyph);
    if (it != g_emoji_cache.end()) return it->second;

    std::vector<uint8_t> px;
    int w = 0, h = 0;
    if (!Rasterize(glyph, px, w, h))
        return g_empty_icon;

    ToneIcon icon;
    icon.w = w;
    icon.h = h;
    icon.tex = Upload(px, w, h);
    auto [ins, _] = g_emoji_cache.emplace(glyph, icon);
    return ins->second;
}

void EnsureToneIconsLoaded() {
    if (g_tone_icons_loaded)
        return;
    for (int i = 0; i < 6; ++i) {
        std::vector<uint8_t> px;
        int w = 0, h = 0;
        if (!Rasterize(kToneUtf8[i], px, w, h)) {
            std::fprintf(stderr, "tone_icons: failed to rasterize %d\n", i);
            continue;
        }
        FitToSquare(px, w, h, kToneIconCachePx);
        g_tone_icons[i].w = w;
        g_tone_icons[i].h = h;
        g_tone_icons[i].tex = Upload(px, w, h);
    }
    g_tone_icons_loaded = true;
}

void LoadToneIcons() {
    EnsureToneIconsLoaded();
}

void DestroyToneIcons() {
    std::vector<unsigned> ids;
    for (int i = 0; i < 6; ++i) {
        if (g_tone_icons[i].tex)
            ids.push_back(g_tone_icons[i].tex);
        g_tone_icons[i] = {};
    }
    for (auto& [_, icon] : g_emoji_cache) {
        if (icon.tex)
            ids.push_back(icon.tex);
    }
    g_emoji_cache.clear();
    if (!ids.empty())
        glDeleteTextures((int)ids.size(), ids.data());

    if (g_hb_font) {
        hb_font_destroy(g_hb_font);
        g_hb_font = nullptr;
    }
    if (g_emoji_face) {
        FT_Done_Face(g_emoji_face);
        g_emoji_face = nullptr;
    }
    if (g_ft) {
        FT_Done_FreeType(g_ft);
        g_ft = nullptr;
    }
    g_tone_icons_loaded = false;
}
