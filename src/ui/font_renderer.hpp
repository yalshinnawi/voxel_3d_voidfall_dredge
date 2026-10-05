#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include "font_atlas.hpp"

namespace Voidfall {

namespace Typography {
    // High-contrast color hierarchy
    inline constexpr glm::vec4 COLOR_PRIMARY     = glm::vec4(0.941f, 0.957f, 0.973f, 1.0f); // #F0F4F8 Off-White
    inline constexpr glm::vec4 COLOR_CYAN        = glm::vec4(0.000f, 0.898f, 1.000f, 1.0f); // #00E5FF Electric Cyan
    inline constexpr glm::vec4 COLOR_CYAN_GLOW   = glm::vec4(0.000f, 0.949f, 1.000f, 1.0f); // #00F2FF Cyan Glow
    inline constexpr glm::vec4 COLOR_AMBER       = glm::vec4(1.000f, 0.702f, 0.000f, 1.0f); // #FFB300 Safety Amber
    inline constexpr glm::vec4 COLOR_GREEN       = glm::vec4(0.180f, 0.800f, 0.443f, 1.0f); // #2ECC71 High-Vis Green
    inline constexpr glm::vec4 COLOR_CRIMSON     = glm::vec4(1.000f, 0.220f, 0.220f, 1.0f); // #FF3838 Alert Red
    inline constexpr glm::vec4 COLOR_MUTED       = glm::vec4(0.480f, 0.530f, 0.600f, 0.85f); // Crisp Charcoal/Slate
    inline constexpr glm::vec4 COLOR_PANEL_BG    = glm::vec4(0.039f, 0.055f, 0.078f, 0.85f); // rgba(10, 14, 20, 0.85)
    inline constexpr glm::vec4 COLOR_SHADOW      = glm::vec4(0.000f, 0.000f, 0.000f, 0.95f); // rgba(0, 0, 0, 0.95)
    inline constexpr glm::vec4 COLOR_BUTTON_BG   = glm::vec4(0.063f, 0.086f, 0.118f, 0.90f); // rgba(16, 22, 30, 0.90)
    inline constexpr glm::vec4 COLOR_BUTTON_HOV  = glm::vec4(0.094f, 0.133f, 0.188f, 0.95f); // rgba(24, 34, 48, 0.95)
}

class FontRenderer {
public:
    static const std::array<GlyphMetric, 128>& get_metrics() {
        static std::array<GlyphMetric, 128> s_metrics;
        static bool s_initialized = false;
        if (!s_initialized) {
            int w, h;
            std::vector<uint8_t> dummy;
            generate_proportional_sans_font_atlas(w, h, dummy, s_metrics);
            s_initialized = true;
        }
        return s_metrics;
    }

    static std::string sanitize_ascii(const std::string& text) {
        std::string out;
        out.reserve(text.size() + 16);
        for (size_t i = 0; i < text.size(); ) {
            unsigned char c = static_cast<unsigned char>(text[i]);
            // Check for 3-byte UTF-8 sequences starting with 0xE2
            if (c == 0xE2 && i + 2 < text.size()) {
                unsigned char c1 = static_cast<unsigned char>(text[i + 1]);
                unsigned char c2 = static_cast<unsigned char>(text[i + 2]);
                if (c1 == 0x80) {
                    if (c2 == 0x94) { // Em-dash: '—'
                        out += " // ";
                        i += 3;
                        continue;
                    } else if (c2 == 0x93) { // En-dash: '–'
                        out += " - ";
                        i += 3;
                        continue;
                    } else if (c2 == 0xA2) { // Bullet: '•'
                        out += " * ";
                        i += 3;
                        continue;
                    } else if (c2 == 0xA6) { // Ellipsis: '…'
                        out += "...";
                        i += 3;
                        continue;
                    } else if (c2 == 0x98 || c2 == 0x99) { // Single curly quotes
                        out += '\'';
                        i += 3;
                        continue;
                    } else if (c2 == 0x9C || c2 == 0x9D) { // Double curly quotes
                        out += '"';
                        i += 3;
                        continue;
                    }
                } else if (c1 == 0x86 && c2 == 0x92) { // Right arrow: '→'
                    out += " -> ";
                    i += 3;
                    continue;
                } else if (c1 == 0x9A && c2 == 0xA0) { // Warning: '⚠'
                    out += "[!]";
                    i += 3;
                    continue;
                }
            }
            // Check for 2-byte UTF-8 degree sign 0xC2 0xB0 '°'
            if (c == 0xC2 && i + 1 < text.size()) {
                unsigned char c1 = static_cast<unsigned char>(text[i + 1]);
                if (c1 == 0xB0) {
                    out += " deg";
                    i += 2;
                    continue;
                }
            }
            // Standard ASCII
            if (c < 128) {
                out += static_cast<char>(c);
                ++i;
            } else if ((c & 0xE0) == 0xC0) {
                // 2-byte UTF-8 sequence fallback
                i += std::min<size_t>(2, text.size() - i);
                out += ' ';
            } else if ((c & 0xF0) == 0xE0) {
                // 3-byte UTF-8 sequence fallback
                i += std::min<size_t>(3, text.size() - i);
                out += " // ";
            } else if ((c & 0xF8) == 0xF0) {
                // 4-byte UTF-8 sequence fallback
                i += std::min<size_t>(4, text.size() - i);
                out += ' ';
            } else {
                // Stray byte
                ++i;
            }
        }
        return out;
    }

    static float get_text_width(const std::string& raw_text, float scale) {
        std::string text = sanitize_ascii(raw_text);
        const auto& metrics = get_metrics();
        float width = 0.0f;
        for (char c : text) {
            uint8_t uc = static_cast<uint8_t>(c);
            if (uc >= 128) uc = '?';
            width += metrics[uc].advanceX * scale;
        }
        return width;
    }

    static void build_text_vertices(
        const std::string& raw_text,
        float start_x, float start_y, float scale,
        std::vector<float>& out_vertices
    ) {
        std::string text = sanitize_ascii(raw_text);
        const auto& metrics = get_metrics();
        float cur_x = start_x;
        float cur_y = start_y;
        float line_h = 32.0f * scale;

        for (char c : text) {
            if (c == '\n') {
                cur_x = start_x;
                cur_y += line_h + 4.0f * scale;
                continue;
            }
            uint8_t uc = static_cast<uint8_t>(c);
            if (uc >= 128) uc = '?';

            const auto& m = metrics[uc];
            if (m.width > 0.0f) {
                float x0 = cur_x + m.bearingX * scale;
                float y0 = cur_y;
                float x1 = x0 + m.width * scale;
                float y1 = y0 + m.height * scale;

                float quad[24] = {
                    x0, y0, m.u0, m.v0,
                    x0, y1, m.u0, m.v1,
                    x1, y1, m.u1, m.v1,

                    x0, y0, m.u0, m.v0,
                    x1, y1, m.u1, m.v1,
                    x1, y0, m.u1, m.v0
                };
                out_vertices.insert(out_vertices.end(), quad, quad + 24);
            }
            cur_x += m.advanceX * scale;
        }
    }
    // Base font atlas glyph height is 32. All shaders and UI draw calls pass scale * 0.45f to build_text_vertices.
    static constexpr float DRAW_SCALE_FACTOR = 0.45f;

    static float get_rendered_width(const std::string& text, float draw_scale) {
        return get_text_width(text, draw_scale * DRAW_SCALE_FACTOR);
    }

    static float get_rendered_height(float draw_scale) {
        return 32.0f * (draw_scale * DRAW_SCALE_FACTOR);
    }

    static float get_line_height(float draw_scale, float extra_spacing = 6.0f) {
        return (32.0f + extra_spacing) * (draw_scale * DRAW_SCALE_FACTOR);
    }

    // Computes a scale <= base_scale so that text fits within max_width, not dropping below min_scale
    static float fit_scale(const std::string& text, float max_width, float base_scale, float min_scale = 0.55f) {
        if (text.empty() || max_width <= 0.0f) return base_scale;
        float cur_w = get_rendered_width(text, base_scale);
        if (cur_w <= max_width) return base_scale;
        float fitted = base_scale * (max_width / cur_w);
        return std::max(min_scale, fitted);
    }

    // Wraps text by pixel width (proportional font accurate)
    static std::vector<std::string> wrap_text(const std::string& raw_text, float max_width, float draw_scale) {
        std::string text = sanitize_ascii(raw_text);
        std::vector<std::string> lines;
        if (text.empty() || max_width <= 0.0f) return lines;

        std::string current_line;
        size_t i = 0;
        while (i < text.length()) {
            if (text[i] == '\n') {
                lines.push_back(current_line);
                current_line.clear();
                ++i;
                continue;
            }

            size_t word_end = text.find_first_of(" \n", i);
            if (word_end == std::string::npos) word_end = text.length();

            std::string word = text.substr(i, word_end - i);
            std::string test_line = current_line.empty() ? word : (current_line + " " + word);

            if (get_rendered_width(test_line, draw_scale) <= max_width) {
                current_line = test_line;
                i = word_end;
                if (i < text.length() && text[i] == ' ') ++i;
            } else {
                if (current_line.empty()) {
                    for (char c : word) {
                        std::string test_c = current_line + c;
                        if (!current_line.empty() && get_rendered_width(test_c, draw_scale) > max_width) {
                            lines.push_back(current_line);
                            current_line.clear();
                        }
                        current_line += c;
                    }
                    i = word_end;
                    if (i < text.length() && text[i] == ' ') ++i;
                } else {
                    lines.push_back(current_line);
                    current_line.clear();
                }
            }
        }
        if (!current_line.empty()) {
            lines.push_back(current_line);
        }
        return lines;
    }
};

namespace UIUtils {
    inline float compute_ui_scale(int screen_w, int screen_h) {
        float sw = static_cast<float>(screen_w) / 1600.0f;
        float sh = static_cast<float>(screen_h) / 900.0f;
        return std::clamp(std::min(sw, sh), 0.65f, 1.5f);
    }
}

} // namespace Voidfall
