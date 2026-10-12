#pragma once
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <algorithm>
#include "font_atlas.hpp"

namespace Voidfall {

namespace Typography {
    // High-contrast primary UI palette
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

    // Specialized Game Resource & Combat Colors
    inline constexpr glm::vec4 COLOR_VOIDITE     = glm::vec4(0.720f, 0.400f, 1.000f, 1.0f); // #B866FF Voidite Purple / Violet
    inline constexpr glm::vec4 COLOR_TITANIUM    = glm::vec4(0.200f, 0.850f, 0.950f, 1.0f); // #33D9F2 Titanium Cyan
    inline constexpr glm::vec4 COLOR_SCRAP       = glm::vec4(1.000f, 0.720f, 0.200f, 1.0f); // #FFB833 Scrap Bronze / Gold
    inline constexpr glm::vec4 COLOR_SALVAGE     = glm::vec4(0.400f, 0.900f, 0.600f, 1.0f); // #66E699 Salvage Emerald
    inline constexpr glm::vec4 COLOR_SHIELD      = glm::vec4(0.000f, 0.800f, 1.000f, 1.0f); // #00CCFF Shield Cyan
    inline constexpr glm::vec4 COLOR_HEALTH      = glm::vec4(0.200f, 0.950f, 0.500f, 1.0f); // #33F280 Health Green
    inline constexpr glm::vec4 COLOR_HAZARD      = glm::vec4(1.000f, 0.450f, 0.000f, 1.0f); // #FF7300 Hazard Orange
    inline constexpr glm::vec4 COLOR_WARNING     = glm::vec4(1.000f, 0.250f, 0.250f, 1.0f); // #FF4040 Warning Red
    inline constexpr glm::vec4 COLOR_EXP         = glm::vec4(1.000f, 0.880f, 0.200f, 1.0f); // #FFE033 EXP Gold
}

class FontRenderer {
public:
    static void init_data() {
        static bool s_initialized = false;
        if (!s_initialized) {
            int w, h;
            std::vector<uint8_t> dummy;
            generate_unified_font_atlas(w, h, dummy, s_all_font_metrics, s_all_symbol_metrics);
            s_initialized = true;
        }
    }

    static const std::array<GlyphMetric, 128>& get_metrics(FontType font = FontType::Body) {
        init_data();
        size_t idx = static_cast<size_t>(font);
        if (idx >= static_cast<size_t>(FontType::Count)) idx = static_cast<size_t>(FontType::Body);
        return s_all_font_metrics[idx];
    }

    static const std::array<GlyphMetric, static_cast<size_t>(SymbolType::Count)>& get_symbol_metrics() {
        init_data();
        return s_all_symbol_metrics;
    }

    static const GlyphMetric& get_symbol_metric(SymbolType symbol) {
        init_data();
        size_t idx = static_cast<size_t>(symbol);
        if (idx >= static_cast<size_t>(SymbolType::Count)) idx = 0;
        return s_all_symbol_metrics[idx];
    }

    // Resolves tag string like "<voidite>" or "<shield>" to SymbolType
    static SymbolType resolve_symbol_tag(std::string_view tag) {
        if (tag.size() < 3 || tag.front() != '<' || tag.back() != '>') return SymbolType::None;
        std::string_view name = tag.substr(1, tag.size() - 2);

        if (name == "voidite")    return SymbolType::Voidite;
        if (name == "titanium")   return SymbolType::Titanium;
        if (name == "scrap")      return SymbolType::Scrap;
        if (name == "salvage")    return SymbolType::Salvage;
        if (name == "shield")     return SymbolType::Shield;
        if (name == "health")     return SymbolType::Health;
        if (name == "hazard")     return SymbolType::Hazard;
        if (name == "sonar")      return SymbolType::Sonar;
        if (name == "drill")      return SymbolType::Drill;
        if (name == "flare")      return SymbolType::Flare;
        if (name == "exp")        return SymbolType::ExpStar;
        if (name == "warning")    return SymbolType::Warning;
        if (name == "check")      return SymbolType::Checkmark;
        if (name == "lock")       return SymbolType::Lock;
        if (name == "crosshair")  return SymbolType::Crosshair;
        if (name == "evac")       return SymbolType::EvacPod;
        if (name == "clock")      return SymbolType::Clock;
        if (name == "skull")      return SymbolType::Skull;
        if (name == "audio")      return SymbolType::Audio;
        if (name == "arrow_r")    return SymbolType::ArrowRight;
        if (name == "arrow_l")    return SymbolType::ArrowLeft;
        if (name == "arrow_u")    return SymbolType::ArrowUp;
        if (name == "arrow_d")    return SymbolType::ArrowDown;
        if (name == "bullet")     return SymbolType::Bullet;

        return SymbolType::None;
    }

    // Resolves UTF-8 multi-byte symbol sequences into SymbolType
    static SymbolType resolve_utf8_symbol(const char* ptr, size_t remaining, size_t& out_bytes_consumed) {
        out_bytes_consumed = 0;
        if (remaining == 0) return SymbolType::None;
        unsigned char c0 = static_cast<unsigned char>(ptr[0]);

        // 3-byte sequences (0xE2)
        if (c0 == 0xE2 && remaining >= 3) {
            unsigned char c1 = static_cast<unsigned char>(ptr[1]);
            unsigned char c2 = static_cast<unsigned char>(ptr[2]);

            // ◆ (0xE2 0x97 0x86) - Voidite Crystal
            if (c1 == 0x97 && c2 == 0x86) { out_bytes_consumed = 3; return SymbolType::Voidite; }
            // ▲ (0xE2 0x96 0xB2) - Titanium / Arrow Up
            if (c1 == 0x96 && c2 == 0xB2) { out_bytes_consumed = 3; return SymbolType::Titanium; }
            // ⚙ (0xE2 0x9A 0x99) - Scrap Metal Gear
            if (c1 == 0x9A && c2 == 0x99) { out_bytes_consumed = 3; return SymbolType::Scrap; }
            // ❖ (0xE2 0x9D 0x96) - Salvage
            if (c1 == 0x9D && c2 == 0x96) { out_bytes_consumed = 3; return SymbolType::Salvage; }
            // ✚ (0xE2 0x9C 0x9A) or ♥ (0xE2 0x99 0xA5) - Health
            if ((c1 == 0x9C && c2 == 0x9A) || (c1 == 0x99 && c2 == 0xA5)) { out_bytes_consumed = 3; return SymbolType::Health; }
            // ☣ (0xE2 0x98 0xA3) - Hazard
            if (c1 == 0x98 && c2 == 0xA3) { out_bytes_consumed = 3; return SymbolType::Hazard; }
            // ◎ (0xE2 0x97 0x8E) - Sonar Pulse
            if (c1 == 0x97 && c2 == 0x8E) { out_bytes_consumed = 3; return SymbolType::Sonar; }
            // ⛏ (0xE2 0x9B 0x8F) - Mining Drill
            if (c1 == 0x9B && c2 == 0x8F) { out_bytes_consumed = 3; return SymbolType::Drill; }
            // ☼ (0xE2 0x98 0xBC) - Flare
            if (c1 == 0x98 && c2 == 0xBC) { out_bytes_consumed = 3; return SymbolType::Flare; }
            // ★ (0xE2 0x98 0x85) - EXP Star
            if (c1 == 0x98 && c2 == 0x85) { out_bytes_consumed = 3; return SymbolType::ExpStar; }
            // ⚠ (0xE2 0x9A 0xA0) - Warning
            if (c1 == 0x9A && c2 == 0xA0) { out_bytes_consumed = 3; return SymbolType::Warning; }
            // ✔ (0xE2 0x9C 0x94) or ✓ (0xE2 0x9C 0x93) - Checkmark
            if (c1 == 0x9C && (c2 == 0x94 || c2 == 0x93)) { out_bytes_consumed = 3; return SymbolType::Checkmark; }
            // ◄ (0xE2 0x97 0x84) - Arrow Left
            if (c1 == 0x97 && c2 == 0x84) { out_bytes_consumed = 3; return SymbolType::ArrowLeft; }
            // ► (0xE2 0x96 0xBA) - Arrow Right
            if (c1 == 0x96 && c2 == 0xBA) { out_bytes_consumed = 3; return SymbolType::ArrowRight; }
            // ▼ (0xE2 0x96 0xBC) - Arrow Down
            if (c1 == 0x96 && c2 == 0xBC) { out_bytes_consumed = 3; return SymbolType::ArrowDown; }
            // ➔ (0xE2 0x9E 0x94) or → (0xE2 0x86 0x92) - Arrow Right
            if ((c1 == 0x9E && c2 == 0x94) || (c1 == 0x86 && c2 == 0x92)) { out_bytes_consumed = 3; return SymbolType::ArrowRight; }
            // • (0xE2 0x80 0xA2) - Bullet
            if (c1 == 0x80 && c2 == 0xA2) { out_bytes_consumed = 3; return SymbolType::Bullet; }
            // ⌖ (0xE2 0x8C 0x96) - Crosshair
            if (c1 == 0x8C && c2 == 0x96) { out_bytes_consumed = 3; return SymbolType::Crosshair; }
            // ⏏ (0xE2 0x8F 0x8F) - Evac Pod
            if (c1 == 0x8F && c2 == 0x8F) { out_bytes_consumed = 3; return SymbolType::EvacPod; }
            // ⏱ (0xE2 0x8F 0xB1) - Clock
            if (c1 == 0x8F && c2 == 0xB1) { out_bytes_consumed = 3; return SymbolType::Clock; }
            // ☠ (0xE2 0x98 0xA0) - Skull
            if (c1 == 0x98 && c2 == 0xA0) { out_bytes_consumed = 3; return SymbolType::Skull; }
        }

        // 4-byte sequences (0xF0)
        if (c0 == 0xF0 && remaining >= 4) {
            unsigned char c1 = static_cast<unsigned char>(ptr[1]);
            unsigned char c2 = static_cast<unsigned char>(ptr[2]);
            unsigned char c3 = static_cast<unsigned char>(ptr[3]);

            // 🛡 (0xF0 0x9F 0x9B 0xA1) - Shield
            if (c1 == 0x9F && c2 == 0x9B && c3 == 0xA1) { out_bytes_consumed = 4; return SymbolType::Shield; }
            // 🔒 (0xF0 0x9F 0x94 0x92) - Lock
            if (c1 == 0x9F && c2 == 0x94 && c3 == 0x92) { out_bytes_consumed = 4; return SymbolType::Lock; }
        }

        return SymbolType::None;
    }

    // Cleans plain ASCII text and preserves inline symbol tags and rich game symbols
    static std::string sanitize_ascii(const std::string& text) {
        std::string out;
        out.reserve(text.size() + 16);
        for (size_t i = 0; i < text.size(); ) {
            // Check for inline symbol tags: <voidite>, etc.
            if (text[i] == '<') {
                size_t tag_end = text.find('>', i);
                if (tag_end != std::string::npos && tag_end - i < 20) {
                    std::string_view tag(&text[i], tag_end - i + 1);
                    if (resolve_symbol_tag(tag) != SymbolType::None) {
                        out.append(tag);
                        i = tag_end + 1;
                        continue;
                    }
                }
            }

            unsigned char c = static_cast<unsigned char>(text[i]);

            // Check for 3-byte UTF-8 punctuation and common typographic marks
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

            // Check for direct rich game symbols
            size_t consumed = 0;
            SymbolType sym = resolve_utf8_symbol(&text[i], text.size() - i, consumed);
            if (sym != SymbolType::None && consumed > 0) {
                out.append(&text[i], consumed);
                i += consumed;
                continue;
            }

            // Standard ASCII
            if (c < 128) {
                out += static_cast<char>(c);
                ++i;
            } else if ((c & 0xE0) == 0xC0) {
                i += std::min<size_t>(2, text.size() - i);
                out += ' ';
            } else if ((c & 0xF0) == 0xE0) {
                i += std::min<size_t>(3, text.size() - i);
                out += ' ';
            } else if ((c & 0xF8) == 0xF0) {
                i += std::min<size_t>(4, text.size() - i);
                out += ' ';
            } else {
                ++i;
            }
        }
        return out;
    }

    // Calculates precise text width including inline symbols and tags
    static float get_text_width(const std::string& raw_text, float scale, FontType font = FontType::Body) {
        std::string text = sanitize_ascii(raw_text);
        const auto& metrics = get_metrics(font);
        float width = 0.0f;

        for (size_t i = 0; i < text.size(); ) {
            if (text[i] == '\n') {
                break; // Single line width
            }

            // Check for inline symbol tags: <voidite>, etc.
            if (text[i] == '<') {
                size_t tag_end = text.find('>', i);
                if (tag_end != std::string::npos && tag_end - i < 20) {
                    std::string_view tag(&text[i], tag_end - i + 1);
                    SymbolType sym = resolve_symbol_tag(tag);
                    if (sym != SymbolType::None) {
                        width += get_symbol_metric(sym).advanceX * scale;
                        i = tag_end + 1;
                        continue;
                    }
                }
            }

            // Check for direct UTF-8 symbols
            size_t consumed = 0;
            SymbolType sym = resolve_utf8_symbol(&text[i], text.size() - i, consumed);
            if (sym != SymbolType::None && consumed > 0) {
                width += get_symbol_metric(sym).advanceX * scale;
                i += consumed;
                continue;
            }

            uint8_t uc = static_cast<uint8_t>(text[i]);
            if (uc >= 128) uc = '?';
            width += metrics[uc].advanceX * scale;
            ++i;
        }
        return width;
    }

    // Builds GPU vertex buffer for text + symbols
    static void build_text_vertices(
        const std::string& raw_text,
        float start_x, float start_y, float scale,
        std::vector<float>& out_vertices,
        FontType font = FontType::Body
    ) {
        std::string text = sanitize_ascii(raw_text);
        const auto& metrics = get_metrics(font);
        float cur_x = start_x;
        float cur_y = start_y;
        float line_h = 32.0f * scale;

        for (size_t i = 0; i < text.size(); ) {
            if (text[i] == '\n') {
                cur_x = start_x;
                cur_y += line_h + 4.0f * scale;
                ++i;
                continue;
            }

            // 1. Check for inline symbol tag: <voidite>, etc.
            if (text[i] == '<') {
                size_t tag_end = text.find('>', i);
                if (tag_end != std::string::npos && tag_end - i < 20) {
                    std::string_view tag(&text[i], tag_end - i + 1);
                    SymbolType sym = resolve_symbol_tag(tag);
                    if (sym != SymbolType::None) {
                        const auto& sm = get_symbol_metric(sym);
                        if (sm.width > 0.0f) {
                            float x0 = cur_x + sm.bearingX * scale;
                            float y0 = cur_y + sm.bearingY * scale;
                            float x1 = x0 + sm.width * scale;
                            float y1 = y0 + sm.height * scale;

                            float quad[24] = {
                                x0, y0, sm.u0, sm.v0,
                                x0, y1, sm.u0, sm.v1,
                                x1, y1, sm.u1, sm.v1,

                                x0, y0, sm.u0, sm.v0,
                                x1, y1, sm.u1, sm.v1,
                                x1, y0, sm.u1, sm.v0
                            };
                            out_vertices.insert(out_vertices.end(), quad, quad + 24);
                        }
                        cur_x += sm.advanceX * scale;
                        i = tag_end + 1;
                        continue;
                    }
                }
            }

            // 2. Check for direct UTF-8 symbols
            size_t consumed = 0;
            SymbolType sym = resolve_utf8_symbol(&text[i], text.size() - i, consumed);
            if (sym != SymbolType::None && consumed > 0) {
                const auto& sm = get_symbol_metric(sym);
                if (sm.width > 0.0f) {
                    float x0 = cur_x + sm.bearingX * scale;
                    float y0 = cur_y + sm.bearingY * scale;
                    float x1 = x0 + sm.width * scale;
                    float y1 = y0 + sm.height * scale;

                    float quad[24] = {
                        x0, y0, sm.u0, sm.v0,
                        x0, y1, sm.u0, sm.v1,
                        x1, y1, sm.u1, sm.v1,

                        x0, y0, sm.u0, sm.v0,
                        x1, y1, sm.u1, sm.v1,
                        x1, y0, sm.u1, sm.v0
                    };
                    out_vertices.insert(out_vertices.end(), quad, quad + 24);
                }
                cur_x += sm.advanceX * scale;
                i += consumed;
                continue;
            }

            // 3. Regular ASCII character
            uint8_t uc = static_cast<uint8_t>(text[i]);
            if (uc >= 128) uc = '?';

            const auto& m = metrics[uc];
            if (m.width > 0.0f) {
                float x0 = cur_x + m.bearingX * scale;
                float y0 = cur_y + m.bearingY * scale;
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
            ++i;
        }
    }

    // Base font atlas glyph height is 32. UI draw calls pass scale * DRAW_SCALE_FACTOR to build_text_vertices.
    static constexpr float DRAW_SCALE_FACTOR = 0.45f;

    static float get_rendered_width(const std::string& text, float draw_scale, FontType font = FontType::Body) {
        return get_text_width(text, draw_scale * DRAW_SCALE_FACTOR, font);
    }

    static float get_rendered_height(float draw_scale, FontType font = FontType::Body) {
        (void)font;
        return 32.0f * (draw_scale * DRAW_SCALE_FACTOR);
    }

    static float get_line_height(float draw_scale, float extra_spacing = 6.0f, FontType font = FontType::Body) {
        (void)font;
        return (32.0f + extra_spacing) * (draw_scale * DRAW_SCALE_FACTOR);
    }

    // Computes a scale <= base_scale so that text fits within max_width, not dropping below min_scale
    static float fit_scale(const std::string& text, float max_width, float base_scale, float min_scale = 0.55f, FontType font = FontType::Body) {
        if (text.empty() || max_width <= 0.0f) return base_scale;
        float cur_w = get_rendered_width(text, base_scale, font);
        if (cur_w <= max_width) return base_scale;
        float fitted = base_scale * (max_width / cur_w);
        return std::max(min_scale, fitted);
    }

    // Wraps text by pixel width (proportional & symbol accurate)
    static std::vector<std::string> wrap_text(const std::string& raw_text, float max_width, float draw_scale, FontType font = FontType::Body) {
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

            if (get_rendered_width(test_line, draw_scale, font) <= max_width) {
                current_line = test_line;
                i = word_end;
                if (i < text.length() && text[i] == ' ') ++i;
            } else {
                if (current_line.empty()) {
                    for (char c : word) {
                        std::string test_c = current_line + c;
                        if (!current_line.empty() && get_rendered_width(test_c, draw_scale, font) > max_width) {
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

private:
    inline static std::array<std::array<GlyphMetric, 128>, static_cast<size_t>(FontType::Count)> s_all_font_metrics;
    inline static std::array<GlyphMetric, static_cast<size_t>(SymbolType::Count)> s_all_symbol_metrics;
};

namespace UIUtils {
    inline float compute_ui_scale(int screen_w, int screen_h) {
        float sw = static_cast<float>(screen_w) / 1600.0f;
        float sh = static_cast<float>(screen_h) / 900.0f;
        return std::clamp(std::min(sw, sh), 0.65f, 1.5f);
    }
}

} // namespace Voidfall
