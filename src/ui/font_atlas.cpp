#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <font8x8.h>
#include "font_atlas.hpp"
#include <fstream>
#include <cmath>
#include <cstring>
#include <iostream>

namespace Voidfall {

namespace {

// Helper to load binary file bytes from multiple relative fallback paths
std::vector<uint8_t> load_font_file(const std::string& filename) {
    const std::vector<std::string> search_dirs = {
        "assets/fonts/",
        "../assets/fonts/",
        "../../assets/fonts/",
        "build/Release/assets/fonts/",
        "build/assets/fonts/",
        "../build/Release/assets/fonts/"
    };

    for (const auto& dir : search_dirs) {
        std::string path = dir + filename;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            std::streamsize size = file.tellg();
            if (size > 0) {
                file.seekg(0, std::ios::beg);
                std::vector<uint8_t> buffer(static_cast<size_t>(size));
                if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
                    return buffer;
                }
            }
        }
    }
    return {};
}

// Procedural fallback generator for when TTF files are not present
void generate_procedural_fallback_band(
    FontType type, int band_y, int atlas_w, int atlas_h,
    std::vector<uint8_t>& atlas,
    std::array<GlyphMetric, 128>& metrics
) {
    const int CELL_SIZE = 32;
    for (int c = 0; c < 128; ++c) {
        int min_col = 8, max_col = -1;
        for (int y = 0; y < 8; ++y) {
            uint8_t row = font8x8_basic[c][y];
            for (int x = 0; x < 8; ++x) {
                if (row & (1 << x)) {
                    if (x < min_col) min_col = x;
                    if (x > max_col) max_col = x;
                }
            }
        }

        if (max_col == -1) {
            GlyphMetric gm;
            gm.advanceX = (c == ' ') ? 11.0f : 8.0f;
            gm.bearingX = 0.0f;
            gm.bearingY = 0.0f;
            gm.width = 0.0f;
            gm.height = static_cast<float>(CELL_SIZE);
            gm.u0 = static_cast<float>(c * CELL_SIZE) / static_cast<float>(atlas_w);
            gm.u1 = gm.u0;
            gm.v0 = static_cast<float>(band_y) / static_cast<float>(atlas_h);
            gm.v1 = static_cast<float>(band_y + CELL_SIZE) / static_cast<float>(atlas_h);
            metrics[c] = gm;
            continue;
        }

        min_col = std::max(0, std::min(7, min_col));
        max_col = std::max(0, std::min(7, max_col));

        int cell_base_x = (c % 64) * CELL_SIZE;
        int cell_base_y = band_y + (c / 64) * CELL_SIZE + 4;
        const int PIXEL_SCALE = 3;

        for (int y = 0; y < 8; ++y) {
            uint8_t row = font8x8_basic[c][y];
            for (int x = 0; x < 8; ++x) {
                if (row & (1 << x)) {
                    int bx = cell_base_x + 4 + x * PIXEL_SCALE;
                    int by = cell_base_y + y * PIXEL_SCALE;

                    int d_max = (type == FontType::Display) ? PIXEL_SCALE + 1 : PIXEL_SCALE;
                    for (int dy = -1; dy <= d_max; ++dy) {
                        for (int dx = -1; dx <= d_max; ++dx) {
                            int px = bx + dx;
                            int py = by + dy;

                            if (px >= 0 && px < atlas_w && py >= 0 && py < atlas_h) {
                                int val = 255;
                                if (dx < 0 || dx >= PIXEL_SCALE || dy < 0 || dy >= PIXEL_SCALE) {
                                    val = (type == FontType::Display) ? 220 : 180;
                                }
                                size_t idx = static_cast<size_t>(py * atlas_w + px);
                                if (atlas[idx] < val) {
                                    atlas[idx] = static_cast<uint8_t>(val);
                                }
                            }
                        }
                    }
                }
            }
        }

        int left_i = std::max(0, 4 + min_col * PIXEL_SCALE - 1);
        int right_i = std::min(CELL_SIZE, 4 + (max_col + 1) * PIXEL_SCALE + 1);

        float px_left = static_cast<float>(cell_base_x + left_i);
        float px_right = static_cast<float>(cell_base_x + right_i);
        float glyph_w = px_right - px_left;

        GlyphMetric gm;
        gm.width = glyph_w;
        gm.height = static_cast<float>(CELL_SIZE);
        gm.bearingX = 0.0f;
        gm.bearingY = 0.0f;
        if (type == FontType::Tech) {
            gm.advanceX = 18.0f; // Slashed mono style
        } else if (type == FontType::Display) {
            gm.advanceX = glyph_w + 5.0f; // Punchy wide display gap
        } else {
            gm.advanceX = glyph_w + 3.0f; // Clean proportional gap
        }
        gm.u0 = px_left / static_cast<float>(atlas_w);
        gm.u1 = px_right / static_cast<float>(atlas_w);
        gm.v0 = static_cast<float>(cell_base_y) / static_cast<float>(atlas_h);
        gm.v1 = static_cast<float>(cell_base_y + CELL_SIZE) / static_cast<float>(atlas_h);

        metrics[c] = gm;
    }
}

// Bakes a TrueType font into a dedicated 128px band in the atlas
bool bake_ttf_band(
    const std::string& font_filename, float pixel_height,
    int band_y, int atlas_w, int atlas_h,
    std::vector<uint8_t>& atlas,
    std::array<GlyphMetric, 128>& metrics
) {
    auto ttf_data = load_font_file(font_filename);
    if (ttf_data.empty()) {
        return false;
    }

    const int BAND_H = 128;
    std::array<stbtt_bakedchar, 96> baked_chars{};

    // Bake characters 32 to 126
    int ret = stbtt_BakeFontBitmap(
        ttf_data.data(), 0, pixel_height,
        atlas.data() + static_cast<size_t>(band_y) * static_cast<size_t>(atlas_w),
        atlas_w, BAND_H,
        32, 96,
        baked_chars.data()
    );

    if (ret <= 0) {
        // Baking did not fit or error
        return false;
    }

    // Initialize metrics for non-printables
    for (int c = 0; c < 32; ++c) {
        GlyphMetric gm;
        gm.advanceX = (c == ' ') ? (pixel_height * 0.32f) : (pixel_height * 0.25f);
        gm.bearingX = 0.0f;
        gm.bearingY = 0.0f;
        gm.width = 0.0f;
        gm.height = pixel_height;
        gm.u0 = 0.0f; gm.u1 = 0.0f;
        gm.v0 = static_cast<float>(band_y) / static_cast<float>(atlas_h);
        gm.v1 = gm.v0;
        metrics[c] = gm;
    }
    metrics[127] = metrics[0];

    // Map baked chars 32..126
    for (int c = 32; c <= 126; ++c) {
        const auto& bc = baked_chars[static_cast<size_t>(c - 32)];
        GlyphMetric gm;
        gm.width = static_cast<float>(bc.x1 - bc.x0);
        gm.height = static_cast<float>(bc.y1 - bc.y0);
        gm.bearingX = bc.xoff;
        // Standardize baseline alignment: baseline sits at 24px in our 32px standard quad
        gm.bearingY = 24.0f + bc.yoff;
        gm.advanceX = bc.xadvance;
        gm.u0 = static_cast<float>(bc.x0) / static_cast<float>(atlas_w);
        gm.v0 = static_cast<float>(bc.y0 + band_y) / static_cast<float>(atlas_h);
        gm.u1 = static_cast<float>(bc.x1) / static_cast<float>(atlas_w);
        gm.v1 = static_cast<float>(bc.y1 + band_y) / static_cast<float>(atlas_h);
        metrics[c] = gm;
    }

    // Space metric
    metrics[' '].advanceX = baked_chars[0].xadvance > 0.0f ? baked_chars[0].xadvance : (pixel_height * 0.32f);
    metrics[' '].width = 0.0f;
    metrics[' '].height = 32.0f;

    return true;
}

// Pixel drawing primitives for custom symbols
void set_pixel(std::vector<uint8_t>& atlas, int atlas_w, int atlas_h, int x, int y, uint8_t val) {
    if (x >= 0 && x < atlas_w && y >= 0 && y < atlas_h) {
        size_t idx = static_cast<size_t>(y * atlas_w + x);
        if (atlas[idx] < val) atlas[idx] = val;
    }
}

void draw_line(std::vector<uint8_t>& atlas, int atlas_w, int atlas_h, int x0, int y0, int x1, int y1, uint8_t val, int thick = 1) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        for (int ty = -thick / 2; ty <= thick / 2; ++ty) {
            for (int tx = -thick / 2; tx <= thick / 2; ++tx) {
                set_pixel(atlas, atlas_w, atlas_h, x0 + tx, y0 + ty, val);
            }
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fill_circle(std::vector<uint8_t>& atlas, int atlas_w, int atlas_h, int cx, int cy, float r, uint8_t val) {
    int min_x = static_cast<int>(cx - r - 1);
    int max_x = static_cast<int>(cx + r + 1);
    int min_y = static_cast<int>(cy - r - 1);
    int max_y = static_cast<int>(cy + r + 1);
    float r2 = r * r;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float dist2 = static_cast<float>((x - cx) * (x - cx) + (y - cy) * (y - cy));
            if (dist2 <= r2) {
                set_pixel(atlas, atlas_w, atlas_h, x, y, val);
            } else if (dist2 <= (r + 1.2f) * (r + 1.2f)) {
                float alpha = (1.0f - (std::sqrt(dist2) - r)) * static_cast<float>(val);
                set_pixel(atlas, atlas_w, atlas_h, x, y, static_cast<uint8_t>(std::clamp(alpha, 0.0f, 255.0f)));
            }
        }
    }
}

void fill_rect(std::vector<uint8_t>& atlas, int atlas_w, int atlas_h, int x0, int y0, int x1, int y1, uint8_t val) {
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            set_pixel(atlas, atlas_w, atlas_h, x, y, val);
        }
    }
}

// Rasterizes all 24 game symbols into Band 3 (Y: 384..511)
void rasterize_game_symbols(
    int band_y, int atlas_w, int atlas_h,
    std::vector<uint8_t>& atlas,
    std::array<GlyphMetric, static_cast<size_t>(SymbolType::Count)>& symbol_metrics
) {
    const int CELL_SIZE = 48;
    const int ICON_SIZE = 32;

    for (size_t s = 1; s < static_cast<size_t>(SymbolType::Count); ++s) {
        SymbolType sym = static_cast<SymbolType>(s);
        int cell_x = static_cast<int>(s - 1) * CELL_SIZE;
        int cell_y = band_y + 12;

        int cx = cell_x + ICON_SIZE / 2;
        int cy = cell_y + ICON_SIZE / 2;

        switch (sym) {
            case SymbolType::Voidite: {
                // ◆ Faceted Voidite crystal: diamond with faceted interior
                draw_line(atlas, atlas_w, atlas_h, cx, cell_y + 2, cell_x + ICON_SIZE - 3, cy, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cell_x + ICON_SIZE - 3, cy, cx, cell_y + ICON_SIZE - 3, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx, cell_y + ICON_SIZE - 3, cell_x + 3, cy, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cell_x + 3, cy, cx, cell_y + 2, 255, 2);
                // Inner facet lines
                draw_line(atlas, atlas_w, atlas_h, cx, cell_y + 8, cell_x + ICON_SIZE - 8, cy, 200, 1);
                draw_line(atlas, atlas_w, atlas_h, cell_x + ICON_SIZE - 8, cy, cx, cell_y + ICON_SIZE - 8, 200, 1);
                draw_line(atlas, atlas_w, atlas_h, cx, cell_y + ICON_SIZE - 8, cell_x + 8, cy, 200, 1);
                draw_line(atlas, atlas_w, atlas_h, cell_x + 8, cy, cx, cell_y + 8, 200, 1);
                draw_line(atlas, atlas_w, atlas_h, cx, cell_y + 2, cx, cell_y + ICON_SIZE - 3, 180, 1);
                draw_line(atlas, atlas_w, atlas_h, cell_x + 3, cy, cell_x + ICON_SIZE - 3, cy, 180, 1);
                fill_circle(atlas, atlas_w, atlas_h, cx, cy, 3.5f, 255);
                break;
            }
            case SymbolType::Titanium: {
                // ▲ Heavy titanium ingot: beveled metallic trapezoid
                for (int y = 0; y < 18; ++y) {
                    int w_half = 6 + (y * 8) / 18;
                    fill_rect(atlas, atlas_w, atlas_h, cx - w_half, cell_y + 7 + y, cx + w_half, cell_y + 7 + y, 220);
                }
                // Metal bevel lines & notch
                draw_line(atlas, atlas_w, atlas_h, cx - 6, cell_y + 7, cx - 14, cell_y + 25, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx + 6, cell_y + 7, cx + 14, cell_y + 25, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 14, cell_y + 25, cx + 14, cell_y + 25, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 6, cell_y + 7, cx + 6, cell_y + 7, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 3, cell_y + 11, cx + 3, cell_y + 11, 80, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 5, cell_y + 18, cx + 5, cell_y + 18, 80, 2);
                break;
            }
            case SymbolType::Scrap: {
                // ⚙ Mechanical gear/cog: 6 teeth with center hole
                fill_circle(atlas, atlas_w, atlas_h, cx, cy, 11.0f, 240);
                for (int a = 0; a < 6; ++a) {
                    float ang = static_cast<float>(a) * 3.14159f / 3.0f;
                    int tx = cx + static_cast<int>(std::cos(ang) * 13.0f);
                    int ty = cy + static_cast<int>(std::sin(ang) * 13.0f);
                    fill_circle(atlas, atlas_w, atlas_h, tx, ty, 3.5f, 255);
                }
                // Center hole
                for (int y = cy - 4; y <= cy + 4; ++y) {
                    for (int x = cx - 4; x <= cx + 4; ++x) {
                        if ((x - cx)*(x - cx) + (y - cy)*(y - cy) <= 18) {
                            if (x >= 0 && x < atlas_w && y >= 0 && y < atlas_h) {
                                atlas[static_cast<size_t>(y * atlas_w + x)] = 0;
                            }
                        }
                    }
                }
                break;
            }
            case SymbolType::Salvage: {
                // ❖ Raw mineral salvage: three clustered crystal shards
                draw_line(atlas, atlas_w, atlas_h, cx - 8, cell_y + 26, cx - 5, cell_y + 8, 240, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 5, cell_y + 8, cx - 1, cell_y + 26, 240, 2);
                draw_line(atlas, atlas_w, atlas_h, cx, cell_y + 26, cx + 3, cell_y + 4, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx + 3, cell_y + 4, cx + 8, cell_y + 26, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx + 6, cell_y + 26, cx + 11, cell_y + 12, 220, 2);
                draw_line(atlas, atlas_w, atlas_h, cx + 11, cell_y + 12, cx + 13, cell_y + 26, 220, 2);
                fill_circle(atlas, atlas_w, atlas_h, cx + 3, cell_y + 14, 3.0f, 200);
                break;
            }
            case SymbolType::Shield: {
                // 🛡 Armored aegis shield: angular crest with chevron
                draw_line(atlas, atlas_w, atlas_h, cx - 12, cell_y + 5, cx + 12, cell_y + 5, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 12, cell_y + 5, cx - 12, cell_y + 17, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx + 12, cell_y + 5, cx + 12, cell_y + 17, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 12, cell_y + 17, cx, cell_y + 28, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx + 12, cell_y + 17, cx, cell_y + 28, 255, 2);
                // Center chevron
                draw_line(atlas, atlas_w, atlas_h, cx - 7, cell_y + 11, cx, cell_y + 18, 220, 2);
                draw_line(atlas, atlas_w, atlas_h, cx + 7, cell_y + 11, cx, cell_y + 18, 220, 2);
                break;
            }
            case SymbolType::Health: {
                // ✚ Vitality health cross: thick medical cross
                fill_rect(atlas, atlas_w, atlas_h, cx - 4, cell_y + 5, cx + 4, cell_y + 27, 255);
                fill_rect(atlas, atlas_w, atlas_h, cx - 11, cy - 4, cx + 11, cy + 4, 255);
                // Beveled outline
                draw_line(atlas, atlas_w, atlas_h, cx - 4, cell_y + 4, cx + 4, cell_y + 4, 180, 1);
                draw_line(atlas, atlas_w, atlas_h, cx - 4, cell_y + 28, cx + 4, cell_y + 28, 180, 1);
                draw_line(atlas, atlas_w, atlas_h, cx - 12, cy - 4, cx - 12, cy + 4, 180, 1);
                draw_line(atlas, atlas_w, atlas_h, cx + 12, cy - 4, cx + 12, cy + 4, 180, 1);
                break;
            }
            case SymbolType::Hazard: {
                // ☣ Radiation / biohazard trefoil
                fill_circle(atlas, atlas_w, atlas_h, cx, cy, 3.5f, 255);
                for (int b = 0; b < 3; ++b) {
                    float ang = static_cast<float>(b) * (2.0f * 3.14159f / 3.0f) - 3.14159f / 2.0f;
                    int bx = cx + static_cast<int>(std::cos(ang) * 9.0f);
                    int by = cy + static_cast<int>(std::sin(ang) * 9.0f);
                    fill_circle(atlas, atlas_w, atlas_h, bx, by, 4.5f, 240);
                }
                break;
            }
            case SymbolType::Sonar: {
                // ◎ Seismic sonar pulse: emitter dot + 2 concentric arcs
                fill_circle(atlas, atlas_w, atlas_h, cx, cy + 6, 2.5f, 255);
                for (float ang = 3.14159f * 0.9f; ang <= 3.14159f * 2.1f; ang += 0.08f) {
                    int ax1 = cx + static_cast<int>(std::cos(ang) * 8.0f);
                    int ay1 = (cy + 6) + static_cast<int>(std::sin(ang) * 8.0f);
                    int ax2 = cx + static_cast<int>(std::cos(ang) * 14.0f);
                    int ay2 = (cy + 6) + static_cast<int>(std::sin(ang) * 14.0f);
                    set_pixel(atlas, atlas_w, atlas_h, ax1, ay1, 230);
                    set_pixel(atlas, atlas_w, atlas_h, ax2, ay2, 255);
                }
                break;
            }
            case SymbolType::Drill: {
                // ⛏ Mining drill bit: cone with spiral cutting teeth
                for (int y = 0; y < 20; ++y) {
                    int w_half = (20 - y) * 9 / 20;
                    fill_rect(atlas, atlas_w, atlas_h, cx - w_half, cell_y + 7 + y, cx + w_half, cell_y + 7 + y, 220);
                }
                draw_line(atlas, atlas_w, atlas_h, cx - 8, cell_y + 9, cx + 8, cell_y + 12, 60, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 6, cell_y + 15, cx + 6, cell_y + 18, 60, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 4, cell_y + 21, cx + 4, cell_y + 23, 60, 2);
                draw_line(atlas, atlas_w, atlas_h, cx, cell_y + 26, cx, cell_y + 29, 255, 2);
                break;
            }
            case SymbolType::Flare: {
                // ☼ Chemical flare starburst: glowing center + 8 light spikes
                fill_circle(atlas, atlas_w, atlas_h, cx, cy, 4.0f, 255);
                draw_line(atlas, atlas_w, atlas_h, cx - 12, cy, cx + 12, cy, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx, cy - 12, cx, cy + 12, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 8, cy - 8, cx + 8, cy + 8, 200, 1);
                draw_line(atlas, atlas_w, atlas_h, cx - 8, cy + 8, cx + 8, cy - 8, 200, 1);
                break;
            }
            case SymbolType::ExpStar: {
                // ★ Delver contractor EXP star: 4-point radiant compass star
                draw_line(atlas, atlas_w, atlas_h, cx, cell_y + 3, cx, cell_y + 29, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 13, cy, cx + 13, cy, 255, 2);
                for (int y = -8; y <= 8; ++y) {
                    int w = 8 - std::abs(y);
                    fill_rect(atlas, atlas_w, atlas_h, cx - w, cy + y, cx + w, cy + y, 240);
                }
                draw_line(atlas, atlas_w, atlas_h, cx - 7, cy - 7, cx + 7, cy + 7, 200, 1);
                draw_line(atlas, atlas_w, atlas_h, cx - 7, cy + 7, cx + 7, cy - 7, 200, 1);
                break;
            }
            case SymbolType::Warning: {
                // ⚠ Caution alert triangle: equilateral triangle with '!'
                draw_line(atlas, atlas_w, atlas_h, cx, cell_y + 4, cx - 13, cell_y + 27, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx, cell_y + 4, cx + 13, cell_y + 27, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 13, cell_y + 27, cx + 13, cell_y + 27, 255, 2);
                fill_rect(atlas, atlas_w, atlas_h, cx - 1, cell_y + 11, cx + 1, cell_y + 19, 255);
                fill_circle(atlas, atlas_w, atlas_h, cx, cell_y + 23, 1.5f, 255);
                break;
            }
            case SymbolType::Checkmark: {
                // ✔ Objective confirmed check
                draw_line(atlas, atlas_w, atlas_h, cx - 11, cy + 1, cx - 3, cy + 9, 255, 3);
                draw_line(atlas, atlas_w, atlas_h, cx - 3, cy + 9, cx + 11, cy - 8, 255, 3);
                break;
            }
            case SymbolType::Lock: {
                // 🔒 Security lock: body + shackle
                fill_rect(atlas, atlas_w, atlas_h, cx - 10, cell_y + 13, cx + 10, cell_y + 27, 240);
                draw_line(atlas, atlas_w, atlas_h, cx - 6, cell_y + 13, cx - 6, cell_y + 7, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx + 6, cell_y + 13, cx + 6, cell_y + 7, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 6, cell_y + 7, cx + 6, cell_y + 7, 255, 2);
                fill_circle(atlas, atlas_w, atlas_h, cx, cell_y + 19, 2.0f, 60);
                fill_rect(atlas, atlas_w, atlas_h, cx - 1, cell_y + 20, cx + 1, cell_y + 24, 60);
                break;
            }
            case SymbolType::Crosshair: {
                // ⌖ Precision targeting reticle
                for (float a = 0.0f; a <= 6.28f; a += 0.1f) {
                    int px = cx + static_cast<int>(std::cos(a) * 9.0f);
                    int py = cy + static_cast<int>(std::sin(a) * 9.0f);
                    set_pixel(atlas, atlas_w, atlas_h, px, py, 220);
                }
                draw_line(atlas, atlas_w, atlas_h, cx, cell_y + 4, cx, cell_y + 8, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx, cell_y + 24, cx, cell_y + 28, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 12, cy, cx - 8, cy, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx + 8, cy, cx + 12, cy, 255, 2);
                fill_circle(atlas, atlas_w, atlas_h, cx, cy, 1.5f, 255);
                break;
            }
            case SymbolType::EvacPod: {
                // ⏏ Dropship extraction pod
                draw_line(atlas, atlas_w, atlas_h, cx - 6, cell_y + 6, cx + 6, cell_y + 6, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 6, cell_y + 6, cx - 11, cell_y + 20, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx + 6, cell_y + 6, cx + 11, cell_y + 20, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 11, cell_y + 20, cx + 11, cell_y + 20, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 10, cell_y + 20, cx - 13, cell_y + 27, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx + 10, cell_y + 20, cx + 13, cell_y + 27, 255, 2);
                fill_rect(atlas, atlas_w, atlas_h, cx - 4, cell_y + 11, cx + 4, cell_y + 13, 180);
                break;
            }
            case SymbolType::Clock: {
                // ⏱ Hazard countdown clock
                for (float a = 0.0f; a <= 6.28f; a += 0.08f) {
                    int px = cx + static_cast<int>(std::cos(a) * 11.0f);
                    int py = (cy + 2) + static_cast<int>(std::sin(a) * 11.0f);
                    set_pixel(atlas, atlas_w, atlas_h, px, py, 240);
                }
                fill_rect(atlas, atlas_w, atlas_h, cx - 2, cell_y + 3, cx + 2, cell_y + 6, 255);
                draw_line(atlas, atlas_w, atlas_h, cx, cy + 2, cx, cell_y + 10, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx, cy + 2, cx + 6, cy + 2, 255, 2);
                fill_circle(atlas, atlas_w, atlas_h, cx, cy + 2, 2.0f, 255);
                break;
            }
            case SymbolType::Skull: {
                // ☠ Apex danger skull
                fill_circle(atlas, atlas_w, atlas_h, cx, cy - 2, 9.0f, 255);
                fill_rect(atlas, atlas_w, atlas_h, cx - 5, cy + 4, cx + 5, cy + 11, 240);
                // Eye sockets
                fill_circle(atlas, atlas_w, atlas_h, cx - 4, cy - 1, 2.5f, 0);
                fill_circle(atlas, atlas_w, atlas_h, cx + 4, cy - 1, 2.5f, 0);
                // Tooth slits
                draw_line(atlas, atlas_w, atlas_h, cx - 2, cy + 7, cx - 2, cy + 10, 0, 1);
                draw_line(atlas, atlas_w, atlas_h, cx + 2, cy + 7, cx + 2, cy + 10, 0, 1);
                break;
            }
            case SymbolType::Audio: {
                // 🔊 Sound wave / acoustic rig
                fill_rect(atlas, atlas_w, atlas_h, cx - 11, cy - 4, cx - 6, cy + 4, 255);
                draw_line(atlas, atlas_w, atlas_h, cx - 6, cy - 4, cx, cy - 9, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx - 6, cy + 4, cx, cy + 9, 255, 2);
                draw_line(atlas, atlas_w, atlas_h, cx, cy - 9, cx, cy + 9, 255, 2);
                for (float a = -1.0f; a <= 1.0f; a += 0.1f) {
                    int ax1 = cx + 4 + static_cast<int>(std::cos(a) * 4.0f);
                    int ay1 = cy + static_cast<int>(std::sin(a) * 6.0f);
                    int ax2 = cx + 5 + static_cast<int>(std::cos(a) * 8.0f);
                    int ay2 = cy + static_cast<int>(std::sin(a) * 11.0f);
                    set_pixel(atlas, atlas_w, atlas_h, ax1, ay1, 220);
                    set_pixel(atlas, atlas_w, atlas_h, ax2, ay2, 255);
                }
                break;
            }
            case SymbolType::ArrowRight: {
                // ➔ Right arrow
                draw_line(atlas, atlas_w, atlas_h, cx - 11, cy, cx + 9, cy, 255, 3);
                draw_line(atlas, atlas_w, atlas_h, cx + 4, cy - 6, cx + 11, cy, 255, 3);
                draw_line(atlas, atlas_w, atlas_h, cx + 4, cy + 6, cx + 11, cy, 255, 3);
                break;
            }
            case SymbolType::ArrowLeft: {
                // ◄ Left arrow
                draw_line(atlas, atlas_w, atlas_h, cx - 9, cy, cx + 11, cy, 255, 3);
                draw_line(atlas, atlas_w, atlas_h, cx - 11, cy, cx - 4, cy - 6, 255, 3);
                draw_line(atlas, atlas_w, atlas_h, cx - 11, cy, cx - 4, cy + 6, 255, 3);
                break;
            }
            case SymbolType::ArrowUp: {
                // ▲ Up arrow
                draw_line(atlas, atlas_w, atlas_h, cx, cy - 9, cx, cy + 11, 255, 3);
                draw_line(atlas, atlas_w, atlas_h, cx, cy - 11, cx - 6, cy - 4, 255, 3);
                draw_line(atlas, atlas_w, atlas_h, cx, cy - 11, cx + 6, cy - 4, 255, 3);
                break;
            }
            case SymbolType::ArrowDown: {
                // ▼ Down arrow
                draw_line(atlas, atlas_w, atlas_h, cx, cy - 11, cx, cy + 9, 255, 3);
                draw_line(atlas, atlas_w, atlas_h, cx, cy + 11, cx - 6, cy + 4, 255, 3);
                draw_line(atlas, atlas_w, atlas_h, cx, cy + 11, cx + 6, cy + 4, 255, 3);
                break;
            }
            case SymbolType::Bullet: {
                // • Bullet point
                fill_circle(atlas, atlas_w, atlas_h, cx, cy, 4.5f, 255);
                break;
            }
            default:
                break;
        }

        GlyphMetric gm;
        gm.width = static_cast<float>(ICON_SIZE);
        gm.height = static_cast<float>(ICON_SIZE);
        gm.bearingX = 0.0f;
        gm.bearingY = 2.0f; // Vertically centered with 32px text line
        gm.advanceX = static_cast<float>(ICON_SIZE + 4); // 4px spacing after icon
        gm.u0 = static_cast<float>(cell_x) / static_cast<float>(atlas_w);
        gm.v0 = static_cast<float>(cell_y) / static_cast<float>(atlas_h);
        gm.u1 = static_cast<float>(cell_x + ICON_SIZE) / static_cast<float>(atlas_w);
        gm.v1 = static_cast<float>(cell_y + ICON_SIZE) / static_cast<float>(atlas_h);
        symbol_metrics[s] = gm;
    }
}

} // anonymous namespace

void generate_unified_font_atlas(
    int& out_w, int& out_h,
    std::vector<uint8_t>& out_atlas,
    std::array<std::array<GlyphMetric, 128>, static_cast<size_t>(FontType::Count)>& out_font_metrics,
    std::array<GlyphMetric, static_cast<size_t>(SymbolType::Count)>& out_symbol_metrics
) {
    out_w = 2048;
    out_h = 512;
    out_atlas.assign(static_cast<size_t>(out_w * out_h), 0);

    // Band 0: Display (Orbitron)
    if (!bake_ttf_band("Orbitron.ttf", 36.0f, 0, out_w, out_h, out_atlas, out_font_metrics[static_cast<size_t>(FontType::Display)])) {
        generate_procedural_fallback_band(FontType::Display, 0, out_w, out_h, out_atlas, out_font_metrics[static_cast<size_t>(FontType::Display)]);
    }

    // Band 1: Body (Oxanium)
    if (!bake_ttf_band("Oxanium.ttf", 30.0f, 128, out_w, out_h, out_atlas, out_font_metrics[static_cast<size_t>(FontType::Body)])) {
        generate_procedural_fallback_band(FontType::Body, 128, out_w, out_h, out_atlas, out_font_metrics[static_cast<size_t>(FontType::Body)]);
    }

    // Band 2: Tech (Share Tech Mono)
    if (!bake_ttf_band("ShareTechMono.ttf", 28.0f, 256, out_w, out_h, out_atlas, out_font_metrics[static_cast<size_t>(FontType::Tech)])) {
        generate_procedural_fallback_band(FontType::Tech, 256, out_w, out_h, out_atlas, out_font_metrics[static_cast<size_t>(FontType::Tech)]);
    }

    // Band 3: Game Icons & Symbols
    rasterize_game_symbols(384, out_w, out_h, out_atlas, out_symbol_metrics);
}

} // namespace Voidfall
