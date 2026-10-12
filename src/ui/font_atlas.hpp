#pragma once
#include <vector>
#include <cstdint>
#include <array>
#include <string>
#include <string_view>
#include <algorithm>

namespace Voidfall {

enum class FontType : uint8_t {
    Display = 0,   // Bold, futuristic sci-fi display (Orbitron) - Banners, Titles, Buttons
    Body    = 1,   // High-legibility modern tech sans (Oxanium) - Tooltips, Logs, Lore, Descriptions
    Tech    = 2,   // Monospace tactical terminal with slashed zero (Share Tech Mono) - Telemetry, Timers, Gauges
    Count   = 3
};

enum class SymbolType : uint8_t {
    None = 0,
    Voidite,       // ◆ Faceted Voidite crystal
    Titanium,      // ▲ Heavy titanium alloy ingot
    Scrap,         // ⚙ Mechanical gear/cog
    Salvage,       // ❖ Raw mineral salvage cluster
    Shield,        // 🛡 Armored aegis shield
    Health,        // ✚ Vitality health cross
    Hazard,        // ☣ Radiation / biohazard trefoil
    Sonar,         // ◎ Seismic sonar pulse waves
    Drill,         // ⛏ Heavy carbide drill bit
    Flare,         // ☼ Chemical flare starburst
    ExpStar,       // ★ Delver contractor EXP star
    Warning,       // ⚠ Caution alert triangle
    Checkmark,     // ✔ Objective confirmed check
    Lock,          // 🔒 Security lock bolt
    Crosshair,     // ⌖ Precision targeting reticle
    EvacPod,       // ⏏ Dropship extraction pod
    Clock,         // ⏱ Hazard countdown clock
    Skull,         // ☠ Apex void danger skull
    Audio,         // 🔊 Sound wave / acoustic rig
    ArrowRight,    // ➔ Right arrow
    ArrowLeft,     // ◄ Left arrow
    ArrowUp,       // ▲ Up arrow
    ArrowDown,     // ▼ Down arrow
    Bullet,        // • Bullet point
    Count
};

struct GlyphMetric {
    float advanceX{14.0f};
    float bearingX{0.0f};
    float bearingY{0.0f};
    float width{14.0f};
    float height{32.0f};
    float u0{0.0f}, v0{0.0f};
    float u1{0.0f}, v1{0.0f};
};

struct UnifiedFontAtlasData {
    int width{2048};
    int height{512};
    std::vector<uint8_t> pixels;
    std::array<std::array<GlyphMetric, 128>, static_cast<size_t>(FontType::Count)> font_metrics;
    std::array<GlyphMetric, static_cast<size_t>(SymbolType::Count)> symbol_metrics;
};

// Generates the complete 2048x512 multi-font & symbol atlas
void generate_unified_font_atlas(
    int& out_w, int& out_h,
    std::vector<uint8_t>& out_atlas,
    std::array<std::array<GlyphMetric, 128>, static_cast<size_t>(FontType::Count)>& out_font_metrics,
    std::array<GlyphMetric, static_cast<size_t>(SymbolType::Count)>& out_symbol_metrics
);

// Backward compatibility helper: fills default Body font metrics
inline void generate_proportional_sans_font_atlas(
    int& out_w, int& out_h,
    std::vector<uint8_t>& out_atlas,
    std::array<GlyphMetric, 128>& out_metrics
) {
    std::array<std::array<GlyphMetric, 128>, static_cast<size_t>(FontType::Count)> all_fonts;
    std::array<GlyphMetric, static_cast<size_t>(SymbolType::Count)> all_symbols;
    generate_unified_font_atlas(out_w, out_h, out_atlas, all_fonts, all_symbols);
    out_metrics = all_fonts[static_cast<size_t>(FontType::Body)];
}

inline void generate_high_legibility_font_atlas(int& out_w, int& out_h, std::vector<uint8_t>& out_atlas) {
    std::array<GlyphMetric, 128> dummy_metrics;
    generate_proportional_sans_font_atlas(out_w, out_h, out_atlas, dummy_metrics);
}

} // namespace Voidfall
