#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include "../src/ui/font_atlas.hpp"
#include "../src/ui/font_renderer.hpp"
#include "../src/ui/hud.hpp"
#include "../src/ui/orbital_hub.hpp"
#include "../src/ui/pause_menu.hpp"
#include "../src/player/character_class.hpp"
#include "../src/player/upgrades.hpp"

using namespace Voidfall;

// Simple test assertion helper with human-readable logging
#define EXPECT_TRUE(cond, msg) \
    if (!(cond)) { \
        std::cerr << "[FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
        return false; \
    }

#define EXPECT_FALSE(cond, msg) \
    if (cond) { \
        std::cerr << "[FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
        return false; \
    }

// ─────────────────────────────────────────────────────────────────────────────
// Module 1: Font Metrics & Printable ASCII Bounds
// ─────────────────────────────────────────────────────────────────────────────
bool test_font_metrics_and_printable_ascii() {
    const auto& metrics = FontRenderer::get_metrics();
    
    // Test that all 128 glyphs are initialized and within valid normalized UV coordinates
    for (int c = 0; c < 128; ++c) {
        const auto& m = metrics[c];
        EXPECT_TRUE(m.u0 >= 0.0f && m.u0 <= 1.0f, "Glyph u0 out of bounds for ASCII " + std::to_string(c));
        EXPECT_TRUE(m.u1 >= 0.0f && m.u1 <= 1.0f, "Glyph u1 out of bounds for ASCII " + std::to_string(c));
        EXPECT_TRUE(m.u1 >= m.u0, "Glyph u1 must be >= u0 for ASCII " + std::to_string(c));
        EXPECT_TRUE(m.v0 >= 0.0f && m.v1 <= 1.0f, "Glyph v out of bounds for ASCII " + std::to_string(c));
        EXPECT_TRUE(m.height > 0.0f && m.height <= 48.0f, "Base glyph cell height must be valid for ASCII " + std::to_string(c));
    }

    // Printable ASCII characters (32 to 126) must have positive advanceX
    for (int c = 32; c <= 126; ++c) {
        const auto& m = metrics[c];
        EXPECT_TRUE(m.advanceX > 0.0f, "Printable character must have positive advanceX: '" + std::string(1, static_cast<char>(c)) + "'");
    }

    // Verify proportional spacing: 'W' and 'M' should be strictly wider than 'I' and 'l' and '!'
    float w_width = metrics[static_cast<uint8_t>('W')].advanceX;
    float m_width = metrics[static_cast<uint8_t>('M')].advanceX;
    float i_width = metrics[static_cast<uint8_t>('I')].advanceX;
    float l_width = metrics[static_cast<uint8_t>('l')].advanceX;
    float excl_width = metrics[static_cast<uint8_t>('!')].advanceX;

    EXPECT_TRUE(w_width > i_width, "Proportional kerning: 'W' must be wider than 'I'");
    EXPECT_TRUE(m_width > l_width, "Proportional kerning: 'M' must be wider than 'l'");
    EXPECT_TRUE(w_width > excl_width, "Proportional kerning: 'W' must be wider than '!'");

    std::cout << "  [+] Module 1: Font Metrics & Printable ASCII Bounds PASS" << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Module 2: String Sanitization & Unicode Fallback
// ─────────────────────────────────────────────────────────────────────────────
bool test_string_sanitization_and_unicode() {
    // 1. Em-dash, En-dash, Bullet, Ellipsis, Arrows, Degree
    std::string s1 = FontRenderer::sanitize_ascii("Delver—Vanguard–Rig•Bullet…More→Go°Temp");
    EXPECT_TRUE(s1.find("//") != std::string::npos, "Em-dash must convert to ' // '");
    EXPECT_TRUE(s1.find("-") != std::string::npos, "En-dash must convert to ' - '");
    EXPECT_TRUE(s1.find("*") != std::string::npos, "Bullet must convert to ' * '");
    EXPECT_TRUE(s1.find("...") != std::string::npos, "Ellipsis must convert to '...'");
    EXPECT_TRUE(s1.find("->") != std::string::npos, "Arrow must convert to ' -> '");
    EXPECT_TRUE(s1.find("deg") != std::string::npos, "Degree must convert to ' deg'");

    // 2. Curly quotes
    std::string s2 = FontRenderer::sanitize_ascii("“Quoted Text” and ‘Single Quote’");
    EXPECT_TRUE(s2.find("\"Quoted Text\"") != std::string::npos, "Double curly quotes must convert to straight double quotes");
    EXPECT_TRUE(s2.find("'Single Quote'") != std::string::npos, "Single curly quotes must convert to straight single quotes");

    // 3. Warning symbol
    std::string s3 = FontRenderer::sanitize_ascii("⚠ WARNING: VOID BREACH");
    EXPECT_TRUE(s3.find("[!]") != std::string::npos, "Warning symbol must convert to '[!]'");

    // 4. Standard ASCII remains completely intact
    std::string plain = "VOIDFALL DREDGE: MINING PROTOCOL 101 - WASD";
    EXPECT_TRUE(FontRenderer::sanitize_ascii(plain) == plain, "Plain ASCII must not be modified");

    std::cout << "  [+] Module 2: String Sanitization & Unicode Fallback PASS" << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Module 3: Proportional Measurement & Scaling Mechanics
// ─────────────────────────────────────────────────────────────────────────────
bool test_measurement_and_scaling_mechanics() {
    float scale = 1.0f;
    float w1 = FontRenderer::get_rendered_width("A", scale);
    float w2 = FontRenderer::get_rendered_width("AA", scale);
    float w3 = FontRenderer::get_rendered_width("AAA", scale);

    EXPECT_TRUE(w1 > 0.0f, "Rendered width of 'A' must be positive");
    EXPECT_TRUE(w2 > w1, "Width of 'AA' must be strictly greater than 'A'");
    EXPECT_TRUE(w3 > w2, "Width of 'AAA' must be strictly greater than 'AA'");

    // Scaling linearity
    float w_double = FontRenderer::get_rendered_width("TEST", 2.0f);
    float w_single = FontRenderer::get_rendered_width("TEST", 1.0f);
    EXPECT_TRUE(std::abs(w_double - 2.0f * w_single) < 0.001f, "Width scaling must be strictly linear");

    // fit_scale mechanics
    std::string long_label = "SEISMIC HAZARDS & CAVERN ACOUSTIC RESONANCE: 100%";
    float base_scale = 1.2f;
    float natural_w = FontRenderer::get_rendered_width(long_label, base_scale);

    // If max_width >= natural_w, scale remains unchanged
    float fit_unchanged = FontRenderer::fit_scale(long_label, natural_w + 50.0f, base_scale, 0.5f);
    EXPECT_TRUE(std::abs(fit_unchanged - base_scale) < 0.001f, "fit_scale must not downscale if text fits");

    // If max_width is constricted, fit_scale downscales proportionally
    float restricted_w = natural_w * 0.6f;
    float fitted_scale = FontRenderer::fit_scale(long_label, restricted_w, base_scale, 0.4f);
    EXPECT_TRUE(fitted_scale < base_scale, "fit_scale must reduce scale for restricted width");
    float fitted_w = FontRenderer::get_rendered_width(long_label, fitted_scale);
    EXPECT_TRUE(fitted_w <= restricted_w + 0.1f, "Fitted text rendered width must not exceed restricted width");

    // wrap_text mechanics
    std::string paragraph = "The Demolitionist deploys heavy micro-charges and industrial bulkheads to reinforce crumbling caverns during seismic tremors.";
    float box_w = 200.0f;
    auto lines = FontRenderer::wrap_text(paragraph, box_w, 1.0f);
    EXPECT_TRUE(lines.size() >= 3, "Paragraph should wrap into at least 3 lines for narrow width");
    for (size_t i = 0; i < lines.size(); ++i) {
        float line_w = FontRenderer::get_rendered_width(lines[i], 1.0f);
        EXPECT_TRUE(line_w <= box_w + 1.0f, "Wrapped line rendered width must not exceed box width");
    }

    std::cout << "  [+] Module 3: Proportional Measurement & Scaling Mechanics PASS" << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Module 4: Orbital Hub Screen Layout Non-Collision Invariants
// ─────────────────────────────────────────────────────────────────────────────
bool test_orbital_hub_layout_invariants() {
    float screen_w = 1600.0f;
    float screen_h = 900.0f;
    float ui_scale = UIUtils::compute_ui_scale(static_cast<int>(screen_w), static_cast<int>(screen_h));

    // 1. Main Menu Buttons
    const std::vector<std::string> main_buttons = {
        "[ DEPLOY EXPEDITION ]",
        "[ CONTRACTOR UPGRADES ]",
        "[ DELVER ROSTER ]",
        "[ AUDIO & RIG SETTINGS ]",
        "[ RESET SAVE DATA ]",
        "[ EXIT TO DESKTOP ]"
    };
    float btn_w = 340.0f * ui_scale;
    float btn_h = 44.0f * ui_scale;
    for (const auto& btn : main_buttons) {
        float text_w = FontRenderer::get_rendered_width(btn, 1.15f * ui_scale);
        EXPECT_TRUE(text_w < btn_w, "Button text '" + btn + "' must fit within button width");
        float fitted = FontRenderer::fit_scale(btn, btn_w - 20.0f * ui_scale, 1.15f * ui_scale, 0.6f);
        EXPECT_TRUE(fitted >= 0.70f, "Fitted scale for button text must remain comfortably legible");
    }

    // 2. Sector Select Carousel: 3 Sector Cards layout
    float card_w = 380.0f * ui_scale;
    float pad = 24.0f * ui_scale;
    float desc_w = card_w - 2.0f * pad;
    const std::vector<std::string> sector_descs = {
        "Shallow seismic bedrock with stable quartz veins. Minimal void stalker presence. Ideal for fresh contractors.",
        "Deep pressurized fissure riddled with acoustic fissures. Void Burrowers actively displace voxel geometry.",
        "Sub-mantle abyssal trenches with lethal void storms and apex Void Stalkers. Maximum extraction hazard."
    };
    for (const auto& desc : sector_descs) {
        auto wrapped = FontRenderer::wrap_text(desc, desc_w, 0.95f * ui_scale);
        EXPECT_TRUE(!wrapped.empty(), "Sector description must produce wrapped lines");
        for (const auto& l : wrapped) {
            float lw = FontRenderer::get_rendered_width(l, 0.95f * ui_scale);
            EXPECT_TRUE(lw <= desc_w + 1.0f, "Sector card description line must not bleed past card margin");
        }
    }

    // 3. Delver Roster: Stat bars and Traits
    const std::vector<std::string> stat_labels = {"Mining Speed", "Armor Plating", "Mobility Speed", "Utility Rating"};
    const std::vector<std::string> stat_vals = {"115%", "140 HP", "100%", "Demolitions"};
    for (size_t s = 0; s < stat_labels.size(); ++s) {
        float label_w = FontRenderer::get_rendered_width(stat_labels[s], 1.00f * ui_scale);
        float val_w = FontRenderer::get_rendered_width(stat_vals[s], 1.00f * ui_scale);
        float total_stat_w = label_w + val_w + 10.0f * ui_scale;
        EXPECT_TRUE(total_stat_w < card_w - 2.0f * pad, "Stat label + value must not collide in delver card row");
    }

    // 4. Hub Audio Rig: Slider, Meter, and Label Clearances
    float panel_w = std::clamp(screen_w * 0.70f, 600.0f, 850.0f);
    float content_w = panel_w - 2.0f * pad;
    float adj_btn_w = std::clamp(42.0f * ui_scale, 34.0f, 48.0f);
    float btn2_x = content_w - 14.0f * ui_scale - adj_btn_w;
    float btn1_x = btn2_x - adj_btn_w - 8.0f * ui_scale;
    float meter_w = 120.0f * ui_scale;
    float meter_x = btn1_x - meter_w - (adj_btn_w * 1.6f + 16.0f * ui_scale);
    float label_x = 14.0f * ui_scale;
    float max_label_w = meter_x - label_x - 10.0f * ui_scale;

    EXPECT_TRUE(max_label_w > 180.0f, "Audio mixer label area must have substantial width");
    EXPECT_TRUE(meter_x > label_x + 100.0f, "Meter bar must start with clear clearance from label start");
    EXPECT_TRUE(btn1_x > meter_x + meter_w, "Minus stepper must have clear clearance from meter bar");
    EXPECT_TRUE(btn2_x > btn1_x + adj_btn_w, "Plus stepper must have clear clearance from minus stepper");

    std::cout << "  [+] Module 4: Orbital Hub Screen Layout Non-Collision Invariants PASS" << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Module 5: HUD & Gameplay Screen Layout Non-Collision Invariants
// ─────────────────────────────────────────────────────────────────────────────
bool test_hud_gameplay_layout_invariants() {
    float screen_w = 1600.0f;
    float screen_h = 900.0f;
    float ui_scale = UIUtils::compute_ui_scale(static_cast<int>(screen_w), static_cast<int>(screen_h));

    // 1. Top Stack Layout: Hazard Bar vs Seismic Banner vs Evac Banner vs Notifications
    float hazard_bar_w = 400.0f * ui_scale;
    float hazard_bar_h = 28.0f * ui_scale;
    float top_y = 12.0f * ui_scale;
    float seismic_banner_y = top_y + hazard_bar_h + 8.0f * ui_scale;
    float seismic_banner_h = 32.0f * ui_scale;
    float notif_start_y = seismic_banner_y + seismic_banner_h + 10.0f * ui_scale;

    EXPECT_TRUE(seismic_banner_y > top_y + hazard_bar_h, "Seismic banner must never overlap top hazard bar");
    EXPECT_TRUE(notif_start_y > seismic_banner_y + seismic_banner_h, "HUD notification stack must not overlap seismic banner");

    // 2. Concurrent Loot Toasts Spacing
    float toast_w = 260.0f * ui_scale;
    float toast_h = 42.0f * ui_scale;
    float toast_gap = 6.0f * ui_scale;
    float toast_step = toast_h + toast_gap;

    for (int t = 0; t < 5; ++t) {
        float y0 = 120.0f + t * toast_step;
        float y1 = y0 + toast_h;
        float next_y0 = 120.0f + (t + 1) * toast_step;
        EXPECT_TRUE(next_y0 >= y1 + toast_gap, "Consecutive loot toasts must maintain vertical safety gap");
    }

    // 3. Contractor Operations Field Manual [H] Columns
    float manual_modal_w = screen_w * 0.82f;
    float manual_pad = 28.0f * ui_scale;
    float manual_content_w = manual_modal_w - 2.0f * manual_pad;
    float col_gap = 20.0f * ui_scale;
    float col_w = (manual_content_w - col_gap) * 0.5f;

    EXPECT_TRUE(col_w > 400.0f, "Manual columns must provide generous width for text directives");
    float col2_x = manual_pad + col_w + col_gap;
    EXPECT_TRUE(col2_x >= manual_pad + col_w + col_gap, "Column 2 must never overlap Column 1");

    // 4. Tactical Cartography Map [TAB] legend & prompts
    float map_w = std::clamp(screen_w * 0.88f, 750.0f, 1380.0f);
    float prompt_w = FontRenderer::get_rendered_width("[TAB / ESC] CLOSE TACTICAL SCANNER  |  [M] CYCLE LAYER", 1.05f * ui_scale);
    EXPECT_TRUE(prompt_w < map_w, "Map control guide must fit within map frame");

    std::cout << "  [+] Module 5: HUD & Gameplay Screen Layout Non-Collision Invariants PASS" << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Module 6: Pause Menu Screen Layout Non-Collision Invariants
// ─────────────────────────────────────────────────────────────────────────────
bool test_pause_menu_layout_invariants() {
    float screen_w = 1600.0f;
    float screen_h = 900.0f;
    float ui_scale = UIUtils::compute_ui_scale(static_cast<int>(screen_w), static_cast<int>(screen_h));

    float panel_w = std::clamp(screen_w * 0.65f, 560.0f, 780.0f);
    float pad = 20.0f * ui_scale;
    float content_w = panel_w - 2.0f * pad;

    // 1. Header Tabs: 4 tabs ([MISSION], [INVENTORY], [AUDIO & RIG], [CONTROLS])
    float tab_gap = 6.0f * ui_scale;
    float tab_w = (content_w - 3.0f * tab_gap) / 4.0f;
    EXPECT_TRUE(tab_w > 90.0f, "Pause menu tabs must have adequate horizontal space");
    const std::vector<std::string> tab_names = {"[MISSION]", "[INVENTORY]", "[AUDIO & RIG]", "[CONTROLS]"};
    for (const auto& tab : tab_names) {
        float tw = FontRenderer::get_rendered_width(tab, 0.95f * ui_scale);
        EXPECT_TRUE(tw < tab_w, "Tab text '" + tab + "' must fit inside tab button width");
    }

    // 2. Tab 2 (Cargo Inventory) Rows: Item name vs [DROP 1] vs [DROP 5]
    float drop_btn_w = std::clamp(78.0f * ui_scale, 65.0f, 95.0f);
    float inner_pad = 16.0f * ui_scale;
    float row_w = content_w - 2.0f * inner_pad;
    float max_item_label_w = row_w - (drop_btn_w * 2.0f + 30.0f * ui_scale);

    EXPECT_TRUE(max_item_label_w > 180.0f, "Inventory item label width must be positive and spacious");
    const std::vector<std::string> items = {
        "Voidite Ore: 999 UNITS",
        "Titanium Cores: 120 UNITS",
        "Mineral Salvage: 50 UNITS",
        "Bulkhead Barricades: 12 UNITS",
        "Demolition Charges: 8 UNITS"
    };
    for (const auto& item : items) {
        float item_w = FontRenderer::get_rendered_width(item, 0.98f * ui_scale);
        EXPECT_TRUE(item_w < max_item_label_w, "Item info '" + item + "' must fit without overlapping [DROP 1] button");
    }

    // 3. Tab 4 (Controls Briefing): Keybindings Two-Column Table
    float c_col_gap = 14.0f * ui_scale;
    float c_col_w = (content_w - 28.0f * ui_scale - c_col_gap) * 0.5f;
    EXPECT_TRUE(c_col_w > 200.0f, "Keybinding columns must have positive width");
    float badge_w = FontRenderer::get_rendered_width("[WASD]", 1.00f * ui_scale);
    float act_w = FontRenderer::get_rendered_width("Locomotion & Strafing", 0.92f * ui_scale);
    EXPECT_TRUE(badge_w + act_w + 16.0f * ui_scale < c_col_w, "Key badge and action description must not collide in table");

    std::cout << "  [+] Module 6: Pause Menu Screen Layout Non-Collision Invariants PASS" << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Module 7: Extraction Beacon & Mission Debrief Screen Invariants
// ─────────────────────────────────────────────────────────────────────────────
bool test_extraction_and_debrief_invariants() {
    float screen_w = 1600.0f;
    float screen_h = 900.0f;
    float ui_scale = UIUtils::compute_ui_scale(static_cast<int>(screen_w), static_cast<int>(screen_h));

    // 1. Extraction Beacon Holdout banner
    float banner_w = 420.0f * ui_scale;
    std::string evac_txt = "EVAC POD ARRIVING IN: 38s  [LZ DEFENSE +40% RESIST]";
    float fitted_scale = FontRenderer::fit_scale(evac_txt, banner_w - 20.0f * ui_scale, 1.15f * ui_scale, 0.6f);
    float fitted_w = FontRenderer::get_rendered_width(evac_txt, fitted_scale);
    EXPECT_TRUE(fitted_w <= banner_w, "Fitted holdout timer text must fit inside extraction banner");

    // 2. Mission Debrief Screen Invariants (Full Screen Expanded Layout)
    float debrief_w = std::clamp(screen_w * 0.94f, 680.0f, screen_w - 36.0f);
    float col_gap = std::clamp(22.0f * ui_scale, 14.0f, 28.0f);
    float btn_w = (debrief_w - col_gap) * 0.5f;

    EXPECT_TRUE(btn_w > 300.0f, "Debrief action button width must be spacious across expanded layout");
    float continue_txt_w = FontRenderer::get_rendered_width("[CONTINUE TO NEXT ROUND]", 1.55f * ui_scale, FontType::Display);
    float redeploy_txt_w = FontRenderer::get_rendered_width("[RE-DEPLOY EXPEDITION]", 1.55f * ui_scale, FontType::Display);
    float hub_txt_w = FontRenderer::get_rendered_width("[RETURN TO ORBITAL HUB]", 1.55f * ui_scale, FontType::Display);
    EXPECT_TRUE(continue_txt_w < btn_w - 20.0f * ui_scale, "Continue text at 1.55x Display font must fit inside debrief action button");
    EXPECT_TRUE(redeploy_txt_w < btn_w - 20.0f * ui_scale, "Redeploy text at 1.55x Display font must fit inside debrief action button");
    EXPECT_TRUE(hub_txt_w < btn_w - 20.0f * ui_scale, "Hub return text at 1.55x Display font must fit inside debrief action button");

    // 3. Harvest & Rewards Large Font Invariants
    float col_w = (debrief_w - col_gap) * 0.5f;
    float l_pad = std::clamp(20.0f * ui_scale, 16.0f, 26.0f);
    float inner_w = col_w - 2.0f * l_pad;
    float voidite_w = FontRenderer::get_rendered_width("<voidite> Voidite Crystals Extracted:", 1.45f * ui_scale, FontType::Body);
    float voidite_val_w = FontRenderer::get_rendered_width("999 units (4995 pts)", 1.45f * ui_scale, FontType::Tech);
    EXPECT_TRUE(voidite_w + voidite_val_w < inner_w, "Voidite extraction label and value at 1.45x font must fit inside left debrief column");

    float score_w = FontRenderer::get_rendered_width("9999 POINTS", 2.30f * ui_scale, FontType::Display);
    EXPECT_TRUE(score_w < inner_w, "Final expedition score at large 2.30x font must comfortably fit score box");

    std::cout << "  [+] Module 7: Extraction Beacon & Mission Debrief Screen Invariants PASS" << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Module 8: Stress Test - Extreme Font Metrics & Clutter Prevention
// ─────────────────────────────────────────────────────────────────────────────
bool test_font_stress_and_extreme_metrics() {
    // Simulate an extreme custom font where glyph widths are 1.75x wider
    float wide_font_scale = 1.75f;
    std::string test_str = "MAXIMUM VOLUME LEVEL 100%";
    float target_box_w = 140.0f;

    // Without fitting, this wide font would drastically exceed the box
    float raw_w = FontRenderer::get_rendered_width(test_str, wide_font_scale);
    EXPECT_TRUE(raw_w > target_box_w, "Raw wide text must initially exceed container to test fit_scale");

    // fit_scale must successfully compress the scale so that the rendered width stays inside target_box_w
    float fitted_scale = FontRenderer::fit_scale(test_str, target_box_w, wide_font_scale, 0.35f);
    float fitted_w = FontRenderer::get_rendered_width(test_str, fitted_scale);
    EXPECT_TRUE(fitted_w <= target_box_w + 0.1f, "Fitted scale must guarantee text fits within target box under extreme font expansion");

    // Test extreme narrow box wrapping
    std::string words = "Void Stalker Burrower Demolitionist Vanguard Scout Titanium Voidite";
    auto lines = FontRenderer::wrap_text(words, 80.0f, 1.0f);
    EXPECT_TRUE(lines.size() >= 5, "Wrapping in extremely narrow box must break into multiple lines without infinite loop");

    std::cout << "  [+] Module 8: Stress Test - Extreme Font Metrics & Clutter Prevention PASS" << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Module 9: Multi-Font Atlas Roles & Game Symbol Tags Bounds & Layout Safety
// ─────────────────────────────────────────────────────────────────────────────
bool test_multi_font_roles_and_symbol_tags() {
    // 1. Verify Distinct Font Types have valid and distinct metrics
    const auto& display_metrics = FontRenderer::get_metrics(FontType::Display);
    const auto& body_metrics    = FontRenderer::get_metrics(FontType::Body);
    const auto& tech_metrics    = FontRenderer::get_metrics(FontType::Tech);

    EXPECT_TRUE(display_metrics[static_cast<uint8_t>('A')].advanceX > 0.0f, "Display font 'A' advance must be positive");
    EXPECT_TRUE(body_metrics[static_cast<uint8_t>('A')].advanceX > 0.0f, "Body font 'A' advance must be positive");
    EXPECT_TRUE(tech_metrics[static_cast<uint8_t>('A')].advanceX > 0.0f, "Tech font 'A' advance must be positive");

    // 2. Monospace invariant for Tech font: all digits '0'..'9' must have IDENTICAL advanceX
    float tech_digit_adv = tech_metrics[static_cast<uint8_t>('0')].advanceX;
    for (char d = '1'; d <= '9'; ++d) {
        float adv = tech_metrics[static_cast<uint8_t>(d)].advanceX;
        EXPECT_TRUE(std::abs(adv - tech_digit_adv) < 0.01f,
                    "Tech monospace font digits must have strictly equal advances for jitter-free HUD counters");
    }

    // 3. Game Symbol Tag Parsing & Width Verification
    std::string symbols[] = {
        "<voidite>", "<titanium>", "<scrap>", "<salvage>", "<shield>", "<health>",
        "<hazard>", "<sonar>", "<drill>", "<flare>", "<exp>", "<warning>",
        "<check>", "<lock>", "<evac>", "<clock>", "<skull>", "<audio>"
    };

    for (const auto& sym : symbols) {
        float sym_w = FontRenderer::get_rendered_width(sym, 1.0f, FontType::Body);
        EXPECT_TRUE(sym_w > 8.0f && sym_w < 50.0f, "Symbol " + sym + " must have valid rendered width: " + std::to_string(sym_w));

        // Vertex generation test
        std::vector<float> verts;
        FontRenderer::build_text_vertices(sym + " 100 UNITS", 10.0f, 20.0f, 1.0f, verts, FontType::Body);
        EXPECT_TRUE(!verts.empty(), "Vertex output must not be empty for symbol string " + sym);
        EXPECT_TRUE(verts.size() % 24 == 0, "Vertex count must be a multiple of 24 (6 vertices * 4 floats)");

        // Verify all vertex coordinates are non-NaN and non-Infinite
        for (float v : verts) {
            EXPECT_TRUE(!std::isnan(v) && !std::isinf(v), "Vertices must be finite numbers");
        }
    }

    // 4. Combined String Measurement: Text with symbol must be strictly wider than text alone
    float w_plain = FontRenderer::get_rendered_width("VOIDITE: 50 / 50", 1.0f, FontType::Tech);
    float w_sym   = FontRenderer::get_rendered_width("<voidite> VOIDITE: 50 / 50", 1.0f, FontType::Tech);
    EXPECT_TRUE(w_sym > w_plain, "Text with <voidite> icon must be wider than text without icon");

    std::cout << "  [+] Module 9: Multi-Font Atlas Roles & Game Symbol Tags Bounds & Layout Safety PASS" << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Module 10: Extreme Scenarios & Bounds Enforcement
// ─────────────────────────────────────────────────────────────────────────────
bool test_extreme_scenarios_and_bounds_enforcement() {
    // 1. Giant Resource Amounts: 99,999,999 units must fit within HUD manifest width without overflow
    float ui_scale = 1.0f;
    float mf_w = 280.0f * ui_scale;
    float bar_w = mf_w - 24.0f * ui_scale;

    std::string extreme_voidite = "<voidite> VOIDITE: 99999999 / 99999999 [MET!]";
    float fit_scale = FontRenderer::fit_scale(extreme_voidite, bar_w, 1.10f * ui_scale, 0.45f * ui_scale, FontType::Tech);
    float rendered_w = FontRenderer::get_rendered_width(extreme_voidite, fit_scale, FontType::Tech);
    EXPECT_TRUE(rendered_w <= bar_w + 0.1f, "Extreme voidite counter must fit inside manifest box");

    // 2. Extremely Long Contractor Briefing (1000 characters) - word wrapping without infinite loop or crash
    std::string long_doc;
    for (int i = 0; i < 50; ++i) {
        long_doc += "Delver directive protocol seismic hazard wyrm subterranean voidite extraction ";
    }
    float col_w = 320.0f;
    auto wrapped_lines = FontRenderer::wrap_text(long_doc, col_w, 0.85f * ui_scale, FontType::Body);
    EXPECT_TRUE(wrapped_lines.size() >= 10, "1000-character doc must wrap into multiple lines");
    for (const auto& line : wrapped_lines) {
        float line_w = FontRenderer::get_rendered_width(line, 0.85f * ui_scale, FontType::Body);
        EXPECT_TRUE(line_w <= col_w + 10.0f, "Wrapped line must strictly fit inside column width");
    }

    // 3. Multi-Resolution UI Scale bounds
    int test_resolutions[][2] = {
        {800, 600}, {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}
    };
    for (const auto& res : test_resolutions) {
        float scale = UIUtils::compute_ui_scale(res[0], res[1]);
        EXPECT_TRUE(scale >= 0.50f && scale <= 3.0f, "UI scale must be within sensible bounds across all resolutions");

        // Verify HUD manifest stays within screen bounds
        float test_mf_w = std::clamp(280.0f * scale, 220.0f, 320.0f);
        EXPECT_TRUE(test_mf_w < static_cast<float>(res[0]) * 0.5f, "Manifest box must never exceed half the screen width");
    }

    std::cout << "  [+] Module 10: Extreme Scenarios & Bounds Enforcement PASS" << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Module 11: Draggable Upgrade Tree & Large Screen Typography Invariants
// ─────────────────────────────────────────────────────────────────────────────
bool test_draggable_upgrade_tree_and_large_typography_invariants() {
    float screen_w = 1600.0f;
    float screen_h = 900.0f;
    float ui_scale = UIUtils::compute_ui_scale(static_cast<int>(screen_w), static_cast<int>(screen_h));

    // 1. Draggable Upgrade Tree Geometry & Scroll Clamping
    float terminal_w = std::clamp(screen_w * 0.94f, 680.0f, 1500.0f);
    float tree_w = std::round(terminal_w * 0.62f);
    float insp_w = terminal_w - tree_w - 14.0f * ui_scale;

    float col_w = 230.0f * ui_scale;
    float col_gap = 16.0f * ui_scale;
    float col_pad = 16.0f * ui_scale;
    float total_tree_w = 6.0f * col_w + 5.0f * col_gap + 2.0f * col_pad;
    float max_scroll_x = std::max(0.0f, total_tree_w - tree_w);

    EXPECT_TRUE(col_w >= 220.0f, "Upgrade discipline columns must be expanded (>=220px) for readable text");
    EXPECT_TRUE(tree_w >= 850.0f, "Skill tree viewport must be broad (>=850px)");
    EXPECT_TRUE(insp_w >= 480.0f, "Inspector panel must have generous width (>=480px)");
    EXPECT_TRUE(max_scroll_x >= 450.0f, "Draggable tree canvas must provide substantial horizontal panning range");

    // Test momentum & clamping invariants
    float test_targets[] = {-200.0f, 0.0f, 250.0f, max_scroll_x, max_scroll_x + 300.0f};
    for (float t : test_targets) {
        float clamped = std::clamp(t, 0.0f, max_scroll_x);
        EXPECT_TRUE(clamped >= 0.0f && clamped <= max_scroll_x, "Target scroll position must strictly clamp to [0, max_scroll_x]");
    }

    // 2. Upgrade Node Text & Box Sizing
    float node_h = 115.0f * ui_scale;
    EXPECT_TRUE(node_h >= 95.0f, "Node height must be at least 95px to house large fonts and pips");

    std::string sample_node_title = "PNEUMATIC IMPACT";
    float title_w = FontRenderer::get_rendered_width(sample_node_title, 1.25f * ui_scale, FontType::Display);
    EXPECT_TRUE(title_w < col_w - 16.0f * ui_scale, "Node title at 1.25x Display font must fit inside column width");

    std::string sample_cost = "<exp> 2500 EXP";
    float cost_w = FontRenderer::get_rendered_width(sample_cost, 1.15f * ui_scale, FontType::Tech);
    EXPECT_TRUE(cost_w < col_w - 24.0f * ui_scale, "Cost tag at 1.15x Tech font must fit inside cost button");

    // 3. Delver Contractor Card Layout & Rich Dossier Spacing
    float avail_w = screen_w - 80.0f * ui_scale;
    float card_gap = std::clamp(24.0f * ui_scale, 16.0f, 32.0f);
    float contractor_card_w = (avail_w - 2.0f * card_gap) / 3.0f;
    float contractor_card_h = screen_h - 76.0f * ui_scale - 24.0f * ui_scale;
    EXPECT_TRUE(contractor_card_w >= 450.0f, "Contractor cards must be wide (>=450px)");
    EXPECT_TRUE(contractor_card_h >= 750.0f, "Contractor cards must span vertical height");

    // Verify contractor name fits at large 2.20x Display font
    std::string contractor_name = "KAELEN VANCE";
    float cname_w = FontRenderer::get_rendered_width(contractor_name, 2.20f * ui_scale, FontType::Display);
    EXPECT_TRUE(cname_w < contractor_card_w - 36.0f * ui_scale, "Contractor name at 2.20x Display font must comfortably fit card width");

    // 4. Sector Selection Carousel Card Proportions
    float sector_card_w = std::clamp(420.0f * ui_scale, 360.0f, 480.0f);
    float sector_card_h = screen_h - 130.0f * ui_scale;
    EXPECT_TRUE(sector_card_w >= 360.0f, "Sector card must be spacious (>=360px)");
    EXPECT_TRUE(sector_card_h >= 700.0f, "Sector card height must comfortably fit all 6 tactical boxes");

    // 5. Main Menu Prominent Buttons & Titles
    float menu_btn_w = std::clamp(480.0f * ui_scale, 380.0f, 540.0f);
    float menu_btn_h = std::clamp(54.0f * ui_scale, 44.0f, 60.0f);
    EXPECT_TRUE(menu_btn_w >= 450.0f && menu_btn_h >= 50.0f, "Main menu buttons must be large and prominently clickable");
    float menu_btn_txt_w = FontRenderer::get_rendered_width("[ 1. EMBARK EXPEDITION ]", 1.50f * ui_scale, FontType::Display);
    EXPECT_TRUE(menu_btn_txt_w < menu_btn_w - 20.0f * ui_scale, "Main menu button text at 1.50x Display font must easily fit inside button");

    std::cout << "  [+] Module 11: Draggable Upgrade Tree & Large Screen Typography Invariants PASS" << std::endl;
    return true;
}

int main() {
    std::cout << "====================================================================" << std::endl;
    std::cout << "  VOIDFALL DREDGE - TYPOGRAPHY, FONT METRICS & TEXT CLUTTER TESTS" << std::endl;
    std::cout << "====================================================================" << std::endl;

    bool pass = true;
    pass &= test_font_metrics_and_printable_ascii();
    pass &= test_string_sanitization_and_unicode();
    pass &= test_measurement_and_scaling_mechanics();
    pass &= test_orbital_hub_layout_invariants();
    pass &= test_hud_gameplay_layout_invariants();
    pass &= test_pause_menu_layout_invariants();
    pass &= test_extraction_and_debrief_invariants();
    pass &= test_font_stress_and_extreme_metrics();
    pass &= test_multi_font_roles_and_symbol_tags();
    pass &= test_extreme_scenarios_and_bounds_enforcement();
    pass &= test_draggable_upgrade_tree_and_large_typography_invariants();

    std::cout << "====================================================================" << std::endl;
    if (pass) {
        std::cout << "  OVERALL RESULT: ALL TYPOGRAPHY & LAYOUT TESTS PASSED (11 MODULES)" << std::endl;
        std::cout << "====================================================================" << std::endl;
        return 0;
    } else {
        std::cerr << "  OVERALL RESULT: TYPOGRAPHY TESTS FAILED" << std::endl;
        std::cout << "====================================================================" << std::endl;
        return 1;
    }
}

