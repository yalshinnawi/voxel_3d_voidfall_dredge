#pragma once
#include <vector>
#include <cstdint>
#include <array>
#include <algorithm>
#include <font8x8.h>

namespace Voidfall {

struct GlyphMetric {
    float advanceX{14.0f};
    float bearingX{0.0f};
    float bearingY{0.0f};
    float width{14.0f};
    float height{32.0f};
    float u0{0.0f}, v0{0.0f};
    float u1{0.0f}, v1{0.0f};
};

// Generates a high-contrast, stroke-thickened 32px proportional sans-serif font atlas
inline void generate_proportional_sans_font_atlas(
    int& out_w, int& out_h,
    std::vector<uint8_t>& out_atlas,
    std::array<GlyphMetric, 128>& out_metrics
) {
    const int GLYPH_SIZE = 32;
    out_w = 128 * GLYPH_SIZE; // 4096 px wide
    out_h = GLYPH_SIZE;        // 32 px high
    out_atlas.assign(out_w * out_h, 0);

    for (int c = 0; c < 128; ++c) {
        // 1. Scan 8x8 font grid to find tight horizontal bounds
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

        // Space or non-printable glyphs
        if (max_col == -1) {
            GlyphMetric gm;
            gm.advanceX = (c == ' ') ? 10.0f : 8.0f;
            gm.bearingX = 0.0f;
            gm.bearingY = 0.0f;
            gm.width = 0.0f;
            gm.height = static_cast<float>(GLYPH_SIZE);
            gm.u0 = static_cast<float>(c * GLYPH_SIZE) / static_cast<float>(out_w);
            gm.u1 = gm.u0;
            gm.v0 = 0.0f;
            gm.v1 = 1.0f;
            out_metrics[c] = gm;
            continue;
        }

        // Clamp bounds inside cell
        min_col = std::max(0, std::min(7, min_col));
        max_col = std::max(0, std::min(7, max_col));

        int cell_base_x = c * GLYPH_SIZE;
        const int OFFSET_X = 4;
        const int OFFSET_Y = 4;
        const int PIXEL_SCALE = 3;

        // 2. Rasterize glyph into 32x32 cell with stroke dilation and smooth antialiasing
        for (int y = 0; y < 8; ++y) {
            uint8_t row = font8x8_basic[c][y];
            for (int x = 0; x < 8; ++x) {
                if (row & (1 << x)) {
                    int bx = cell_base_x + OFFSET_X + x * PIXEL_SCALE;
                    int by = OFFSET_Y + y * PIXEL_SCALE;

                    // Fill 3x3 core with smooth antialiased outline
                    for (int dy = -1; dy <= PIXEL_SCALE; ++dy) {
                        for (int dx = -1; dx <= PIXEL_SCALE; ++dx) {
                            int px = bx + dx;
                            int py = by + dy;

                            if (px >= cell_base_x && px < cell_base_x + GLYPH_SIZE && py >= 0 && py < GLYPH_SIZE) {
                                int val = 255;
                                if (dx < 0 || dx >= PIXEL_SCALE || dy < 0 || dy >= PIXEL_SCALE) {
                                    val = 180; // Smooth antialiased border
                                }
                                int idx = py * out_w + px;
                                if (out_atlas[idx] < val) {
                                    out_atlas[idx] = static_cast<uint8_t>(val);
                                }
                            }
                        }
                    }
                }
            }
        }

        // 3. Compute exact proportional quad metrics
        int left_i = std::max(0, OFFSET_X + min_col * PIXEL_SCALE - 1);
        int right_i = std::min(GLYPH_SIZE, OFFSET_X + (max_col + 1) * PIXEL_SCALE + 1);

        float px_left = static_cast<float>(cell_base_x + left_i);
        float px_right = static_cast<float>(cell_base_x + right_i);
        float glyph_w = px_right - px_left;

        GlyphMetric gm;
        gm.width = glyph_w;
        gm.height = static_cast<float>(GLYPH_SIZE);
        gm.bearingX = 0.0f;
        gm.bearingY = 0.0f;
        gm.advanceX = glyph_w + 3.0f; // 3px proportional kerning gap
        gm.u0 = px_left / static_cast<float>(out_w);
        gm.u1 = px_right / static_cast<float>(out_w);
        gm.v0 = 0.0f;
        gm.v1 = 1.0f;

        out_metrics[c] = gm;
    }
}

// Backward compatibility helper
inline void generate_high_legibility_font_atlas(int& out_w, int& out_h, std::vector<uint8_t>& out_atlas) {
    std::array<GlyphMetric, 128> dummy_metrics;
    generate_proportional_sans_font_atlas(out_w, out_h, out_atlas, dummy_metrics);
}

} // namespace Voidfall
