#include "ui.hpp"

struct UiPalette {
    uint8_t bg[3];
    uint8_t panel[3];
    uint8_t panel2[3];
    uint8_t top[3];
    uint8_t accent[3];
    uint8_t text[3];
    uint8_t muted[3];
    uint8_t select_text[3];
};

static UiPalette g_palette = {
    {8, 10, 12}, {16, 20, 25}, {26, 28, 31}, {18, 24, 30},
    {235, 145, 35}, {224, 226, 226}, {160, 170, 176}, {15, 20, 24}
};

void ui_set_theme(AppTheme theme) {
    switch (theme) {
    case AppTheme::Amber:
        g_palette = {{13, 10, 7}, {28, 22, 14}, {40, 32, 20}, {38, 28, 18},
                     {244, 155, 32}, {236, 228, 210}, {190, 166, 132}, {20, 14, 8}};
        break;
    case AppTheme::Cyan:
        g_palette = {{5, 11, 14}, {12, 24, 30}, {18, 34, 42}, {13, 34, 42},
                     {72, 204, 210}, {220, 238, 240}, {145, 180, 186}, {6, 22, 26}};
        break;
    case AppTheme::Light:
        g_palette = {{218, 220, 214}, {236, 238, 232}, {205, 210, 204}, {190, 198, 196},
                     {210, 126, 28}, {32, 35, 38}, {88, 94, 96}, {245, 246, 240}};
        break;
    case AppTheme::Dark:
    default:
        g_palette = {{8, 10, 12}, {16, 20, 25}, {26, 28, 31}, {18, 24, 30},
                     {235, 145, 35}, {224, 226, 226}, {160, 170, 176}, {15, 20, 24}};
        break;
    }
}

static uint32_t color(SDL_Surface* s, uint8_t r, uint8_t g, uint8_t b) {
    // SDL_MapRGB is not free on the Miyoo CPU. The UI uses a small, repeated
    // palette, so a tiny direct-mapped cache removes most conversion calls.
    struct CacheEntry {
        const SDL_PixelFormat* format;
        uint32_t rgb;
        uint32_t pixel;
    };
    static CacheEntry cache[64] = {};

    const uint32_t rgb = (static_cast<uint32_t>(r) << 16) |
                         (static_cast<uint32_t>(g) << 8) |
                         static_cast<uint32_t>(b);
    const uintptr_t fmt_key = reinterpret_cast<uintptr_t>(s->format) >> 4;
    const unsigned idx = static_cast<unsigned>((rgb * 2654435761u) ^ fmt_key) & 63u;
    CacheEntry& e = cache[idx];
    if (e.format == s->format && e.rgb == rgb) {
        return e.pixel;
    }
    e.format = s->format;
    e.rgb = rgb;
    e.pixel = SDL_MapRGB(s->format, r, g, b);
    return e.pixel;
}

static void fill_mapped_unlocked(SDL_Surface* s, int x, int y, int w, int h, uint32_t pixel) {
    if (!s || w <= 0 || h <= 0) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x >= s->w || y >= s->h || w <= 0 || h <= 0) {
        return;
    }
    if (x + w > s->w) w = s->w - x;
    if (y + h > s->h) h = s->h - y;

    const int bpp = s->format->BytesPerPixel;
    uint8_t* row = static_cast<uint8_t*>(s->pixels) + y * s->pitch + x * bpp;

    switch (bpp) {
    case 2: {
        const uint16_t v = static_cast<uint16_t>(pixel);
        for (int yy = 0; yy < h; ++yy) {
            uint16_t* dst = reinterpret_cast<uint16_t*>(row + yy * s->pitch);
            for (int xx = 0; xx < w; ++xx) dst[xx] = v;
        }
        break;
    }
    case 4: {
        const uint32_t v = pixel;
        for (int yy = 0; yy < h; ++yy) {
            uint32_t* dst = reinterpret_cast<uint32_t*>(row + yy * s->pitch);
            for (int xx = 0; xx < w; ++xx) dst[xx] = v;
        }
        break;
    }
    case 3: {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        const uint8_t c0 = static_cast<uint8_t>((pixel >> 16) & 0xFF);
        const uint8_t c1 = static_cast<uint8_t>((pixel >> 8) & 0xFF);
        const uint8_t c2 = static_cast<uint8_t>(pixel & 0xFF);
#else
        const uint8_t c0 = static_cast<uint8_t>(pixel & 0xFF);
        const uint8_t c1 = static_cast<uint8_t>((pixel >> 8) & 0xFF);
        const uint8_t c2 = static_cast<uint8_t>((pixel >> 16) & 0xFF);
#endif
        for (int yy = 0; yy < h; ++yy) {
            uint8_t* dst = row + yy * s->pitch;
            for (int xx = 0; xx < w; ++xx) {
                dst[xx * 3 + 0] = c0;
                dst[xx * 3 + 1] = c1;
                dst[xx * 3 + 2] = c2;
            }
        }
        break;
    }
    case 1:
        for (int yy = 0; yy < h; ++yy) {
            std::memset(row + yy * s->pitch, static_cast<int>(pixel), static_cast<size_t>(w));
        }
        break;
    default: {
        SDL_Rect rc{static_cast<Sint16>(x), static_cast<Sint16>(y),
                    static_cast<Uint16>(w), static_cast<Uint16>(h)};
        SDL_FillRect(s, &rc, pixel);
        break;
    }
    }
}

static void fill(SDL_Surface* s, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    if (!s) {
        return;
    }
    const uint32_t pixel = color(s, r, g, b);
    if (SDL_MUSTLOCK(s)) {
        if (SDL_LockSurface(s) < 0) return;
        fill_mapped_unlocked(s, x, y, w, h, pixel);
        SDL_UnlockSurface(s);
    } else {
        fill_mapped_unlocked(s, x, y, w, h, pixel);
    }
}

static const uint8_t kFont5x7[96][7] = {
    /* 0x20   */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x21 ! */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x22 " */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x23 # */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x24 $ */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x25 % */ {0x11, 0x01, 0x02, 0x04, 0x08, 0x10, 0x11},
    /* 0x26 & */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x27 0x27 */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x28 ( */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x29 ) */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x2A * */ {0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00},
    /* 0x2B + */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x2C , */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x2D - */ {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},
    /* 0x2E . */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C},
    /* 0x2F / */ {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10},
    /* 0x30 0 */ {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    /* 0x31 1 */ {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F},
    /* 0x32 2 */ {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    /* 0x33 3 */ {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
    /* 0x34 4 */ {0x11, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x01},
    /* 0x35 5 */ {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
    /* 0x36 6 */ {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    /* 0x37 7 */ {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    /* 0x38 8 */ {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    /* 0x39 9 */ {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E},
    /* 0x3A : */ {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00},
    /* 0x3B ; */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x3C < */ {0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01},
    /* 0x3D = */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x3E > */ {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10},
    /* 0x3F ? */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x40 @ */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x41 A */ {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    /* 0x42 B */ {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    /* 0x43 C */ {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    /* 0x44 D */ {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    /* 0x45 E */ {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    /* 0x46 F */ {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    /* 0x47 G */ {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E},
    /* 0x48 H */ {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    /* 0x49 I */ {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F},
    /* 0x4A J */ {0x1F, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C},
    /* 0x4B K */ {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    /* 0x4C L */ {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    /* 0x4D M */ {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11},
    /* 0x4E N */ {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    /* 0x4F O */ {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    /* 0x50 P */ {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    /* 0x51 Q */ {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    /* 0x52 R */ {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    /* 0x53 S */ {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
    /* 0x54 T */ {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    /* 0x55 U */ {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    /* 0x56 V */ {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    /* 0x57 W */ {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11},
    /* 0x58 X */ {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    /* 0x59 Y */ {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
    /* 0x5A Z */ {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
    /* 0x5B [ */ {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E},
    /* 0x5C 0x5C */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x5D ] */ {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E},
    /* 0x5E ^ */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x5F _ */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F},
    /* 0x60 ` */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x61 a */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x62 b */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x63 c */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x64 d */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x65 e */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x66 f */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x67 g */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x68 h */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x69 i */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x6A j */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x6B k */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x6C l */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x6D m */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x6E n */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x6F o */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x70 p */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x71 q */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x72 r */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x73 s */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x74 t */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x75 u */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x76 v */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x77 w */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x78 x */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x79 y */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x7A z */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x7B { */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x7C | */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x7D } */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x7E ~ */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* 0x7F 0x7F */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

static const uint8_t kMissingGlyph[7] = {0x1F, 0x11, 0x15, 0x11, 0x15, 0x11, 0x1F};

static const uint8_t* glyph_bits(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (uc >= 'a' && uc <= 'z') {
        uc = static_cast<unsigned char>(uc - ('a' - 'A'));
    }
    if (uc >= 32 && uc < 128) {
        const uint8_t* rows = kFont5x7[uc - 32];
        if (uc == ' ') {
            return rows;
        }
        for (int i = 0; i < 7; ++i) {
            if (rows[i] != 0) {
                return rows;
            }
        }
    }
    return kMissingGlyph;
}

void ui_clear(SDL_Surface* s, uint8_t r, uint8_t g, uint8_t b) {
    SDL_FillRect(s, nullptr, color(s, r, g, b));
}

void ui_draw_text(SDL_Surface* s, int x, int y, const std::string& text,
                  uint8_t r, uint8_t g, uint8_t b, int scale) {
    if (!s || text.empty()) {
        return;
    }
    if (scale < 1) {
        scale = 1;
    }

    const uint32_t pixel = color(s, r, g, b);
    const bool must_lock = SDL_MUSTLOCK(s) != 0;
    if (must_lock && SDL_LockSurface(s) < 0) {
        return;
    }

    int px = x;
    const int advance = 6 * scale;
    for (char ch : text) {
        const uint8_t* rows = glyph_bits(ch);
        for (int yy = 0; yy < 7; ++yy) {
            const uint8_t row = rows[yy];
            int xx = 0;
            while (xx < 5) {
                while (xx < 5 && ((row & (1u << (4 - xx))) == 0)) {
                    ++xx;
                }
                const int start = xx;
                while (xx < 5 && ((row & (1u << (4 - xx))) != 0)) {
                    ++xx;
                }
                if (xx > start) {
                    fill_mapped_unlocked(s, px + start * scale, y + yy * scale,
                                         (xx - start) * scale, scale, pixel);
                }
            }
        }
        px += advance;
        if (px > s->w - scale) {
            break;
        }
    }

    if (must_lock) {
        SDL_UnlockSurface(s);
    }
}

static std::string fit_text(std::string s, int chars) {
    if (static_cast<int>(s.size()) <= chars) {
        return s;
    }
    return s.substr(0, std::max(0, chars - 2)) + "..";
}

static const char* sort_name(SortMode mode) {
    switch (mode) {
    case SortMode::Type: return "TYPE";
    case SortMode::Time: return "TIME";
    case SortMode::Name:
    default: return "NAME";
    }
}

static std::string yes_no(bool v) {
    return v ? "ON" : "OFF";
}

static const char* order_name(PlaybackOrder order) {
    switch (order) {
    case PlaybackOrder::ListLoop: return "LOOP";
    case PlaybackOrder::RepeatOne: return "ONE";
    case PlaybackOrder::Shuffle: return "RAND";
    case PlaybackOrder::Sequential:
    default: return "SEQ";
    }
}

static std::string convert_summary(const ConversionOptions& c, int field) {
    char buf[32];
    if (field == 0) return std::string("FMT ") + convert_target_name(c.target);
    if (field == 1) {
        std::snprintf(buf, sizeof(buf), "RATE %d", c.sample_rate);
        return buf;
    }
    if (field == 2) {
        std::snprintf(buf, sizeof(buf), "DEPTH %d", c.bit_depth);
        return buf;
    }
    std::snprintf(buf, sizeof(buf), "BR %dk", c.bitrate_kbps);
    return buf;
}

static std::string size_text(int64_t bytes) {
    char buf[24];
    if (bytes >= 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%dMB", static_cast<int>(bytes / (1024 * 1024)));
    } else if (bytes >= 1024) {
        std::snprintf(buf, sizeof(buf), "%dKB", static_cast<int>(bytes / 1024));
    } else {
        std::snprintf(buf, sizeof(buf), "%dB", static_cast<int>(bytes));
    }
    return buf;
}

static void draw_play_icon(SDL_Surface* s, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < 10; ++i) {
        fill(s, x + i, y + 2 + i / 2, 1, 10 - i, r, g, b);
    }
}

static void draw_pause_icon(SDL_Surface* s, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    fill(s, x, y + 1, 4, 12, r, g, b);
    fill(s, x + 8, y + 1, 4, 12, r, g, b);
}

static void draw_speaker_icon(SDL_Surface* s, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    fill(s, x, y + 5, 4, 5, r, g, b);
    fill(s, x + 4, y + 3, 4, 9, r, g, b);
    fill(s, x + 10, y + 4, 2, 7, r, g, b);
    fill(s, x + 13, y + 2, 2, 11, r, g, b);
}

static void draw_order_icon(SDL_Surface* s, int x, int y, PlaybackOrder order) {
    uint8_t r = g_palette.accent[0], g = g_palette.accent[1], b = g_palette.accent[2];
    if (order == PlaybackOrder::Shuffle) {
        fill(s, x, y + 4, 10, 2, r, g, b);
        fill(s, x + 8, y + 2, 2, 2, r, g, b);
        fill(s, x + 10, y, 4, 2, r, g, b);
        fill(s, x + 8, y + 8, 2, 2, r, g, b);
        fill(s, x + 10, y + 10, 4, 2, r, g, b);
    } else {
        fill(s, x, y + 2, 13, 2, r, g, b);
        fill(s, x + 11, y, 2, 6, r, g, b);
        fill(s, x, y + 10, 13, 2, r, g, b);
        fill(s, x, y + 8, 2, 6, r, g, b);
        if (order == PlaybackOrder::RepeatOne) {
            ui_draw_text(s, x + 16, y + 2, "1", r, g, b);
        }
    }
}

static void draw_playlist_cover(SDL_Surface* s, int x, int y, int w, int h, bool selected, int seed) {
    fill(s, x, y, w, h, selected ? 32 : 14, selected ? 38 : 18, selected ? 44 : 22);
    fill(s, x + 2, y + 2, w - 4, h - 4, 10 + (seed * 17) % 34, 14 + (seed * 13) % 32, 18 + (seed * 19) % 40);
    int cx = x + w / 2 - 14;
    int cy = y + h / 2 - 18;
    fill(s, cx, cy + 5, 28, 4, 120, 124, 128);
    fill(s, cx, cy + 17, 22, 4, 120, 124, 128);
    fill(s, cx + 30, cy + 4, 8, 34, 120, 124, 128);
    fill(s, cx + 38, cy + 4, 8, 8, 120, 124, 128);
    fill(s, cx + 20, cy + 31, 18, 18, 120, 124, 128);
    fill(s, cx + 26, cy + 37, 6, 6, 18, 22, 26);
    fill(s, x + w - 25, y + h - 25, 18, 18, 235, 235, 235);
    draw_play_icon(s, x + w - 19, y + h - 21, 90, 92, 96);
}

static void draw_folder_icon(SDL_Surface* s, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    fill(s, x, y + 5, 28, 18, r, g, b);
    fill(s, x + 2, y + 2, 10, 5, r, g, b);
    fill(s, x, y + 5, 28, 2, r + 20, g + 20, b + 20);
}

static void draw_home_menu(SDL_Surface* s, const HomeState& home) {
    const int x = 188;
    fill(s, x, 22, SCREEN_W - x, 136, g_palette.panel[0], g_palette.panel[1], g_palette.panel[2]);
    fill(s, x, 22, SCREEN_W - x, 18, g_palette.top[0] + 35, g_palette.top[1] + 35, g_palette.top[2] + 35);
    ui_draw_text(s, x + 8, 28, "HOME MENU", g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    const char* rows[] = {"NEW LIST", "SCAN ROOT", "BROWSE", "SETTINGS", "DELETE", "CLOSE"};
    for (int i = 0; i < 6; ++i) {
        int y = 48 + i * 17;
        bool sel = i == home.menu_selected;
        if (sel) fill(s, x + 5, y - 4, SCREEN_W - x - 10, 14, g_palette.accent[0], g_palette.accent[1], g_palette.accent[2]);
        ui_draw_text(s, x + 10, y, rows[i],
                     sel ? g_palette.select_text[0] : g_palette.text[0],
                     sel ? g_palette.select_text[1] : g_palette.text[1],
                     sel ? g_palette.select_text[2] : g_palette.text[2]);
    }
}

void ui_draw_home(SDL_Surface* s, const PlaylistManager& playlists, const HomeState& home) {
    ui_clear(s, g_palette.bg[0], g_palette.bg[1], g_palette.bg[2]);
    fill(s, 0, 0, SCREEN_W, 28, g_palette.top[0], g_palette.top[1], g_palette.top[2]);
    fill(s, 8, 7, 13, 13, g_palette.accent[0], g_palette.accent[1] / 2, g_palette.accent[2] / 2);
    fill(s, 11, 3, 7, 22, g_palette.accent[0], g_palette.accent[1], g_palette.accent[2]);
    ui_draw_text(s, 30, 9, "MIYOO PLAYER", g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    ui_draw_text(s, 236, 9, "SELECT", g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);

    ui_draw_text(s, 8, 42, "PLAYLISTS", g_palette.accent[0], g_palette.accent[1], g_palette.accent[2], 2);
    if (playlists.count() == 0) {
        fill(s, 16, 80, 288, 86, g_palette.panel[0], g_palette.panel[1], g_palette.panel[2]);
        draw_folder_icon(s, 38, 112, 130, 136, 142);
        ui_draw_text(s, 80, 105, "NO PLAYLISTS", g_palette.text[0], g_palette.text[1], g_palette.text[2], 2);
        ui_draw_text(s, 80, 130, "SELECT SCAN OR CREATE", g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);
    } else {
        int sel = playlists.selected();
        int first = sel > 0 ? sel - 1 : 0;
        if (first + 1 >= playlists.count() && first > 0) first = playlists.count() - 2;
        for (int slot = 0; slot < 2; ++slot) {
            int idx = first + slot;
            if (idx >= playlists.count()) break;
            const Playlist* p = playlists.get(idx);
            int x = 14 + slot * 154;
            bool selected = idx == sel;
            draw_playlist_cover(s, x, 72, 138, 78, selected, idx + 1);
            if (selected) fill(s, x, 151, 138, 3, g_palette.accent[0], g_palette.accent[1], g_palette.accent[2]);
            ui_draw_text(s, x, 162, fit_text(p->name, 22), selected ? g_palette.accent[0] : g_palette.text[0], selected ? g_palette.accent[1] : g_palette.text[1], selected ? g_palette.accent[2] : g_palette.text[2]);
            char meta[32];
            std::snprintf(meta, sizeof(meta), "%02d TRACKS", static_cast<int>(p->items.size()));
            ui_draw_text(s, x, 178, meta, g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);
        }
    }

    fill(s, 0, 212, SCREEN_W, 28, g_palette.panel2[0], g_palette.panel2[1], g_palette.panel2[2]);
    if (!home.status.empty()) {
        ui_draw_text(s, 8, 221, fit_text(home.status, 50), g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    } else {
        ui_draw_text(s, 10, 221, "A OPEN", g_palette.text[0], g_palette.text[1], g_palette.text[2]);
        ui_draw_text(s, 80, 221, "B BROWSE", g_palette.text[0], g_palette.text[1], g_palette.text[2]);
        ui_draw_text(s, 174, 221, "X NEW", g_palette.text[0], g_palette.text[1], g_palette.text[2]);
        ui_draw_text(s, 244, 221, "SEL MENU", g_palette.accent[0], g_palette.accent[1], g_palette.accent[2]);
    }
    if (home.menu_open) {
        draw_home_menu(s, home);
    }
}

static std::string mmss(float sec) {
    if (sec < 0.0f) sec = 0.0f;
    int v = static_cast<int>(sec + 0.5f);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", v / 60, v % 60);
    return buf;
}

static void draw_scrollbar(SDL_Surface* s, int total, int selected) {
    fill(s, SCREEN_W - 5, 30, 3, 184, 44, 48, 54);
    if (total <= 1) {
        fill(s, SCREEN_W - 5, 30, 3, 184, 160, 170, 80);
        return;
    }
    int knob_h = std::max(12, 184 / std::max(1, total));
    int y = 30 + ((184 - knob_h) * selected) / (total - 1);
    fill(s, SCREEN_W - 5, y, 3, knob_h, 196, 210, 82);
}

static void draw_browser_menu(SDL_Surface* s, const FileBrowser& browser, const BrowserMenuState& menu) {
    const int x = 170;
    fill(s, x, 18, SCREEN_W - x, 206, g_palette.panel[0], g_palette.panel[1], g_palette.panel[2]);
    fill(s, x, 18, SCREEN_W - x, 18, g_palette.top[0], g_palette.top[1], g_palette.top[2]);
    ui_draw_text(s, x + 8, 24, "MENU", g_palette.text[0], g_palette.text[1], g_palette.text[2]);

    std::string rows[] = {
        "HOME",
        std::string("SORT ") + sort_name(browser.sort_mode()),
        std::string("HIDDEN ") + yes_no(browser.show_hidden()),
        std::string("ORDER ") + order_name(menu.order),
        "OPEN MARKS",
        "OPEN DIR",
        "ADD FILE",
        "SCAN LIST",
        "CONVERT",
        "NEW LIST",
        "REFRESH",
        "CLOSE"
    };
    for (int i = 0; i < 12; ++i) {
        int y = 40 + i * 14;
        bool sel = i == menu.selected;
        if (sel) {
            fill(s, x + 4, y - 3, SCREEN_W - x - 8, 12, g_palette.accent[0], g_palette.accent[1], g_palette.accent[2]);
        }
        ui_draw_text(s, x + 10, y, rows[i],
                     sel ? g_palette.select_text[0] : g_palette.text[0],
                     sel ? g_palette.select_text[1] : g_palette.text[1],
                     sel ? g_palette.select_text[2] : g_palette.text[2]);
    }
    ui_draw_text(s, x + 8, 211, "B CLOSE", g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);
}

static std::string basename_display(const std::string& path) {
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static void draw_playlist_menu(SDL_Surface* s, const PlaylistViewState& view) {
    const int x = 178;
    fill(s, x, 28, SCREEN_W - x, 104, g_palette.panel[0], g_palette.panel[1], g_palette.panel[2]);
    fill(s, x, 28, SCREEN_W - x, 18, g_palette.top[0], g_palette.top[1], g_palette.top[2]);
    ui_draw_text(s, x + 8, 34, "LIST MENU", g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    const char* rows[] = {"PLAY", "REMOVE ITEM", "DELETE LIST", "CLOSE"};
    for (int i = 0; i < 4; ++i) {
        int y = 56 + i * 17;
        bool sel = i == view.menu_selected;
        if (sel) {
            fill(s, x + 5, y - 4, SCREEN_W - x - 10, 14, g_palette.accent[0], g_palette.accent[1], g_palette.accent[2]);
        }
        ui_draw_text(s, x + 10, y, rows[i],
                     sel ? g_palette.select_text[0] : g_palette.text[0],
                     sel ? g_palette.select_text[1] : g_palette.text[1],
                     sel ? g_palette.select_text[2] : g_palette.text[2]);
    }
}

void ui_draw_playlist(SDL_Surface* s, const PlaylistManager& playlists, const PlaylistViewState& view) {
    ui_clear(s, g_palette.bg[0], g_palette.bg[1], g_palette.bg[2]);
    const Playlist* p = playlists.get(view.playlist_index);
    fill(s, 0, 0, SCREEN_W, 28, g_palette.top[0], g_palette.top[1], g_palette.top[2]);
    ui_draw_text(s, 6, 9, p ? fit_text(p->name, 35) : "PLAYLIST", g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    ui_draw_text(s, 238, 9, "SEL MENU", g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);

    if (!p || p->items.empty()) {
        fill(s, 16, 78, 288, 76, g_palette.panel[0], g_palette.panel[1], g_palette.panel[2]);
        ui_draw_text(s, 48, 102, "EMPTY PLAYLIST", g_palette.text[0], g_palette.text[1], g_palette.text[2], 2);
        ui_draw_text(s, 48, 128, "B BACK  SEL MENU", g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);
    } else {
        char meta[32];
        std::snprintf(meta, sizeof(meta), "%02d TRACKS", static_cast<int>(p->items.size()));
        ui_draw_text(s, 8, 34, meta, g_palette.accent[0], g_palette.accent[1], g_palette.accent[2]);
        int y = 52;
        const int row_h = 16;
        for (int i = view.scroll; i < static_cast<int>(p->items.size()) && y < 210; ++i, y += row_h) {
            bool sel = i == view.selected;
            if (sel) {
                fill(s, 4, y - 3, SCREEN_W - 12, row_h, g_palette.accent[0], g_palette.accent[1], g_palette.accent[2]);
            }
            char num[16];
            std::snprintf(num, sizeof(num), "%02d ", i + 1);
            ui_draw_text(s, 8, y, fit_text(std::string(num) + basename_display(p->items[i]), 50),
                         sel ? g_palette.select_text[0] : g_palette.text[0],
                         sel ? g_palette.select_text[1] : g_palette.text[1],
                         sel ? g_palette.select_text[2] : g_palette.text[2]);
        }
        draw_scrollbar(s, static_cast<int>(p->items.size()), view.selected);
    }

    fill(s, 0, 212, SCREEN_W, 28, g_palette.panel2[0], g_palette.panel2[1], g_palette.panel2[2]);
    std::string status = view.status.empty() ? "A PLAY  B BACK  X REMOVE" : view.status;
    ui_draw_text(s, 8, 221, fit_text(status, 50), g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    if (view.menu_open) {
        draw_playlist_menu(s, view);
    }
}

void ui_draw_converter(SDL_Surface* s, const ConverterState& converter) {
    ui_clear(s, g_palette.bg[0], g_palette.bg[1], g_palette.bg[2]);
    fill(s, 0, 0, SCREEN_W, 28, g_palette.top[0], g_palette.top[1], g_palette.top[2]);
    ui_draw_text(s, 8, 9, "CONVERTER", g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    ui_draw_text(s, 236, 9, "B BACK", g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);

    fill(s, 10, 38, 300, 42, g_palette.panel[0], g_palette.panel[1], g_palette.panel[2]);
    ui_draw_text(s, 18, 48, "SOURCE", g_palette.accent[0], g_palette.accent[1], g_palette.accent[2]);
    ui_draw_text(s, 18, 64, fit_text(basename_display(converter.source), 46), g_palette.text[0], g_palette.text[1], g_palette.text[2]);

    const std::string rows[] = {
        convert_summary(converter.options, 0),
        convert_summary(converter.options, 1),
        convert_summary(converter.options, 2),
        convert_summary(converter.options, 3),
        "START"
    };
    for (int i = 0; i < 5; ++i) {
        int y = 96 + i * 24;
        bool sel = i == converter.selected;
        fill(s, 18, y - 6, 284, 19,
             sel ? g_palette.accent[0] : g_palette.panel[0],
             sel ? g_palette.accent[1] : g_palette.panel[1],
             sel ? g_palette.accent[2] : g_palette.panel[2]);
        ui_draw_text(s, 28, y, rows[i],
                     sel ? g_palette.select_text[0] : g_palette.text[0],
                     sel ? g_palette.select_text[1] : g_palette.text[1],
                     sel ? g_palette.select_text[2] : g_palette.text[2]);
        if (i < 4) {
            ui_draw_text(s, 238, y, "< >",
                         sel ? g_palette.select_text[0] : g_palette.muted[0],
                         sel ? g_palette.select_text[1] : g_palette.muted[1],
                         sel ? g_palette.select_text[2] : g_palette.muted[2]);
        }
    }

    fill(s, 0, 212, SCREEN_W, 28, g_palette.panel2[0], g_palette.panel2[1], g_palette.panel2[2]);
    std::string status = converter.status.empty() ? "A CHANGE  START RUN" : converter.status;
    ui_draw_text(s, 8, 221, fit_text(status, 50), g_palette.text[0], g_palette.text[1], g_palette.text[2]);
}

static std::string setting_summary(const AppSettings& s, int field) {
    char buf[32];
    if (field == 0) {
        std::snprintf(buf, sizeof(buf), "RATE %d", s.audio_output_rate);
        return buf;
    }
    if (field == 1) {
        std::snprintf(buf, sizeof(buf), "BUFFER %dKB", s.audio_buffer_kb);
        return buf;
    }
    if (field == 2) {
        std::snprintf(buf, sizeof(buf), "SDL BUF %d", s.sdl_audio_samples);
        return buf;
    }
    if (field == 3) {
        std::snprintf(buf, sizeof(buf), "UI %dMS", s.audio_ui_ms);
        return buf;
    }
    if (field == 4) {
        std::snprintf(buf, sizeof(buf), "SEEK %dS", s.seek_step_sec);
        return buf;
    }
    if (field == 5) {
        std::snprintf(buf, sizeof(buf), "OSD %dMS", s.osd_ms);
        return buf;
    }
    if (field == 6) return std::string("HIDDEN ") + yes_no(s.show_hidden);
    if (field == 7) return std::string("ORDER ") + settings_order_name(s.default_order);
    if (field == 8) return std::string("THEME ") + settings_theme_name(s.theme);
    return "SAVE";
}

void ui_draw_settings(SDL_Surface* s, const SettingsState& settings) {
    ui_clear(s, g_palette.bg[0], g_palette.bg[1], g_palette.bg[2]);
    fill(s, 0, 0, SCREEN_W, 28, g_palette.top[0], g_palette.top[1], g_palette.top[2]);
    ui_draw_text(s, 8, 9, "SETTINGS", g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    ui_draw_text(s, 236, 9, "B BACK", g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);

    fill(s, 12, 38, 296, 160, g_palette.panel[0], g_palette.panel[1], g_palette.panel[2]);
    ui_draw_text(s, 20, 48, "AUDIO AND PLAYER", g_palette.accent[0], g_palette.accent[1], g_palette.accent[2]);
    for (int i = 0; i < 10; ++i) {
        int y = 68 + i * 13;
        bool sel = i == settings.selected;
        if (sel) {
            fill(s, 18, y - 3, 284, 12, g_palette.accent[0], g_palette.accent[1], g_palette.accent[2]);
        }
        ui_draw_text(s, 24, y, setting_summary(settings.values, i),
                     sel ? g_palette.select_text[0] : g_palette.text[0],
                     sel ? g_palette.select_text[1] : g_palette.text[1],
                     sel ? g_palette.select_text[2] : g_palette.text[2]);
        if (i < 9) {
            ui_draw_text(s, 258, y, "<>",
                         sel ? g_palette.select_text[0] : g_palette.muted[0],
                         sel ? g_palette.select_text[1] : g_palette.muted[1],
                         sel ? g_palette.select_text[2] : g_palette.muted[2]);
        }
    }

    fill(s, 0, 212, SCREEN_W, 28, g_palette.panel2[0], g_palette.panel2[1], g_palette.panel2[2]);
    std::string status = settings.status.empty() ? "A CHANGE  START SAVE" : settings.status;
    ui_draw_text(s, 8, 221, fit_text(status, 50), g_palette.text[0], g_palette.text[1], g_palette.text[2]);
}

void ui_draw_browser(SDL_Surface* s, const FileBrowser& browser, const BrowserMenuState& menu) {
    ui_clear(s, g_palette.bg[0], g_palette.bg[1], g_palette.bg[2]);
    fill(s, 0, 0, SCREEN_W, 24, g_palette.top[0], g_palette.top[1], g_palette.top[2]);
    ui_draw_text(s, 6, 8, "MIYOO PLAYER", g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    char count[32];
    std::snprintf(count, sizeof(count), "%02d VID %02d DIR", browser.media_count(), browser.dir_count());
    ui_draw_text(s, 216, 8, count, g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);

    fill(s, 0, 24, SCREEN_W, 18, g_palette.panel[0], g_palette.panel[1], g_palette.panel[2]);
    ui_draw_text(s, 6, 30, fit_text(browser.cwd(), 50), g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);

    const auto& es = browser.entries();
    int y = 50;
    const int row_h = 16;
    if (es.empty()) {
        ui_draw_text(s, 22, 105, "NO MEDIA FILES", g_palette.muted[0], g_palette.muted[1], g_palette.muted[2], 2);
    }
    for (int i = browser.scroll(); i < static_cast<int>(es.size()) && y < 210; ++i, y += row_h) {
        bool sel = i == browser.selected();
        if (sel) {
            fill(s, 4, y - 3, SCREEN_W - 14, row_h, g_palette.accent[0], g_palette.accent[1], g_palette.accent[2]);
        }
        bool marked = std::find(menu.marked_paths.begin(), menu.marked_paths.end(), es[i].path) != menu.marked_paths.end();
        std::string prefix = marked ? "[*] " : (es[i].is_dir ? "[D] " : "[F] ");
        ui_draw_text(s, 8, y, fit_text(prefix + es[i].name, 50),
                     sel ? g_palette.select_text[0] : g_palette.text[0],
                     sel ? g_palette.select_text[1] : g_palette.text[1],
                     sel ? g_palette.select_text[2] : g_palette.text[2]);
    }
    draw_scrollbar(s, static_cast<int>(es.size()), browser.selected());

    fill(s, 0, 212, SCREEN_W, 28, g_palette.panel2[0], g_palette.panel2[1], g_palette.panel2[2]);
    const FileEntry* selected = browser.selected_entry();
    if (selected) {
        std::string meta = menu.status.empty() ? (selected->is_dir ? "FOLDER" : size_text(selected->size)) : menu.status;
        if (menu.status.empty()) {
            meta += std::string(" SORT ") + sort_name(browser.sort_mode());
        }
        if (!menu.marked_paths.empty()) {
            char mark[16];
            std::snprintf(mark, sizeof(mark), " M%02d", static_cast<int>(menu.marked_paths.size()));
            meta += mark;
        }
        ui_draw_text(s, 6, 220, fit_text(meta, 40), g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);
    }
    ui_draw_text(s, 132, 220, "X MARK  B HOME  SEL", g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    if (menu.open) {
        draw_browser_menu(s, browser, menu);
    }
}

void ui_draw_playback_osd(SDL_Surface* s, const PlaybackInfo& info) {
    if (!info.osd_visible) {
        return;
    }
    const bool has_meta = !info.metadata_line.empty();
    fill(s, 0, 0, SCREEN_W, 28, g_palette.top[0] / 2, g_palette.top[1] / 2, g_palette.top[2] / 2);
    ui_draw_text(s, 6, 5, fit_text(info.title, has_meta ? 50 : 42), g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    if (has_meta) {
        ui_draw_text(s, 6, 17, fit_text(info.metadata_line, 50),
                     g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);
    }

    const int panel_y = SCREEN_H - 36;
    const int control_y = panel_y + 7;
    const int bar_x = 8;
    const int bar_y = SCREEN_H - 10;
    const int bar_w = SCREEN_W - 16;
    fill(s, 0, panel_y, SCREEN_W, 36, g_palette.top[0] / 2, g_palette.top[1] / 2, g_palette.top[2] / 2);

    int filled = 0;
    if (info.duration > 0.1f) {
        filled = clamp_int(static_cast<int>((info.pos / info.duration) * bar_w), 0, bar_w);
    }
    fill(s, bar_x, bar_y, bar_w, 5, g_palette.panel2[0] + 20, g_palette.panel2[1] + 20, g_palette.panel2[2] + 20);
    fill(s, bar_x, bar_y, filled, 5, g_palette.accent[0], g_palette.accent[1], g_palette.accent[2]);

    if (info.paused) {
        draw_pause_icon(s, 9, control_y - 2, g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    } else {
        draw_play_icon(s, 10, control_y - 2, g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    }

    std::string text = mmss(info.pos) + "/" + mmss(info.duration);
    char vol[16];
    std::snprintf(vol, sizeof(vol), "%03d", (info.volume * 100) / SDL_MIX_MAXVOLUME);
    ui_draw_text(s, 30, control_y, text, g_palette.text[0], g_palette.text[1], g_palette.text[2]);

    draw_speaker_icon(s, 176, control_y - 2, g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);
    ui_draw_text(s, 196, control_y, vol, g_palette.text[0], g_palette.text[1], g_palette.text[2]);

    draw_order_icon(s, 254, control_y - 1, info.order);
    ui_draw_text(s, 282, control_y, order_name(info.order), g_palette.text[0], g_palette.text[1], g_palette.text[2]);
}

void ui_draw_playback_menu(SDL_Surface* s, const PlaybackInfo& info) {
    fill(s, 34, 42, 252, 134, g_palette.panel[0], g_palette.panel[1], g_palette.panel[2]);
    fill(s, 34, 42, 252, 20, g_palette.top[0], g_palette.top[1], g_palette.top[2]);
    ui_draw_text(s, 44, 49, "NOW PLAYING", g_palette.text[0], g_palette.text[1], g_palette.text[2]);
    ui_draw_text(s, 44, 69, fit_text(info.title, 38), g_palette.text[0], g_palette.text[1], g_palette.text[2]);

    int y = 89;
    if (!info.metadata_line.empty()) {
        ui_draw_text(s, 44, y, fit_text(info.metadata_line, 38),
                     g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);
        y += 18;
    }

    char line[48];
    std::snprintf(line, sizeof(line), "TIME %s/%s", mmss(info.pos).c_str(), mmss(info.duration).c_str());
    ui_draw_text(s, 44, y, line, g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);
    y += 18;
    std::snprintf(line, sizeof(line), "VOL %03d  QUEUE %02dKB", (info.volume * 100) / SDL_MIX_MAXVOLUME,
                  info.audio_queue_kb);
    ui_draw_text(s, 44, y, line, g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);
    y += 18;
    if (info.video_w > 0 && info.video_h > 0) {
        std::snprintf(line, sizeof(line), "VIDEO %dx%d", info.video_w, info.video_h);
    } else {
        std::snprintf(line, sizeof(line), "AUDIO ONLY");
    }
    ui_draw_text(s, 44, y, line, g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);
    y += 18;
    std::snprintf(line, sizeof(line), "ORDER %s  X CHANGE", order_name(info.order));
    ui_draw_text(s, 44, y, line, g_palette.muted[0], g_palette.muted[1], g_palette.muted[2]);
}
