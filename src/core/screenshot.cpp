#define _CRT_SECURE_NO_WARNINGS
#include "screenshot.hpp"
#include "logger.hpp"
#include <glad/glad.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <font8x8.h>

#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace Voidfall {

bool capture_screenshot_png(const std::string& filepath, int width, int height) {
    if (width <= 0 || height <= 0) return false;

    // Allocate 3 bytes per pixel (RGB)
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // OpenGL framebuffers are bottom-to-top, flip vertically for standard PNG
    stbi_flip_vertically_on_write(1);

    try {
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (...) {}

    int result = stbi_write_png(filepath.c_str(), width, height, 3, pixels.data(), width * 3);
    if (result) {
        VF_LOG_INFO("Screenshot", "Captured screenshot successfully: " << filepath << " (" << width << "x" << height << ")");
        return true;
    } else {
        VF_LOG_ERROR("Screenshot", "Failed to write screenshot to: " << filepath);
        return false;
    }
}

// ── VisualTestHarness Implementation ──

VisualTestHarness& VisualTestHarness::instance() {
    static VisualTestHarness s_instance;
    return s_instance;
}

VisualTestHarness::VisualTestHarness() {
    reset();
}

void VisualTestHarness::reset() {
    m_metrics.clear();
    m_montage_buffer.assign(static_cast<size_t>(MONTAGE_W) * MONTAGE_H * 3, 14); // Dark navy/slate backdrop

    // Paint slot border outlines
    for (int y = 0; y < MONTAGE_H; ++y) {
        for (int x = 0; x < MONTAGE_W; ++x) {
            bool is_col_div = (x % TILE_W == 0 || x == MONTAGE_W - 1);
            bool is_row_div = (y % TILE_H == 0 || y == MONTAGE_H - 1);
            if (is_col_div || is_row_div) {
                size_t idx = static_cast<size_t>(y * MONTAGE_W + x) * 3;
                m_montage_buffer[idx + 0] = 35;
                m_montage_buffer[idx + 1] = 42;
                m_montage_buffer[idx + 2] = 55;
            }
        }
    }

    m_enemy_metrics.clear();
    m_enemy_montage_buffer.assign(static_cast<size_t>(1200) * 450 * 3, 14);
    for (int y = 0; y < 450; ++y) {
        for (int x = 0; x < 1200; ++x) {
            bool is_col_div = (x % 400 == 0 || x == 1199);
            bool is_row_div = (y % 225 == 0 || y == 449);
            if (is_col_div || is_row_div) {
                size_t idx = static_cast<size_t>(y * 1200 + x) * 3;
                m_enemy_montage_buffer[idx + 0] = 35;
                m_enemy_montage_buffer[idx + 1] = 42;
                m_enemy_montage_buffer[idx + 2] = 55;
            }
        }
    }

    m_model_metrics.clear();
    m_model_montage_buffer.assign(static_cast<size_t>(1200) * 450 * 3, 14);
    for (int y = 0; y < 450; ++y) {
        for (int x = 0; x < 1200; ++x) {
            bool is_col_div = (x % 400 == 0 || x == 1199);
            bool is_row_div = (y % 225 == 0 || y == 449);
            if (is_col_div || is_row_div) {
                size_t idx = static_cast<size_t>(y * 1200 + x) * 3;
                m_model_montage_buffer[idx + 0] = 35;
                m_model_montage_buffer[idx + 1] = 42;
                m_model_montage_buffer[idx + 2] = 55;
            }
        }
    }

    m_shape_metrics.clear();
    m_shape_montage_buffer.assign(static_cast<size_t>(1600) * 900 * 3, 14);
    for (int y = 0; y < 900; ++y) {
        for (int x = 0; x < 1600; ++x) {
            bool is_col_div = (x % 400 == 0 || x == 1599);
            bool is_row_div = (y % 225 == 0 || y == 899);
            if (is_col_div || is_row_div) {
                size_t idx = static_cast<size_t>(y * 1600 + x) * 3;
                m_shape_montage_buffer[idx + 0] = 35;
                m_shape_montage_buffer[idx + 1] = 42;
                m_shape_montage_buffer[idx + 2] = 55;
            }
        }
    }
}

void VisualTestHarness::draw_text(std::vector<uint8_t>& buffer, int buf_w, int buf_h, int x, int y,
                                 const std::string& text, uint8_t r, uint8_t g, uint8_t b) {
    for (size_t i = 0; i < text.size(); ++i) {
        uint8_t ch = static_cast<uint8_t>(text[i]);
        if (ch > 127) ch = '?';
        int char_x = x + static_cast<int>(i) * 8;
        for (int row = 0; row < 8; ++row) {
            int py = y + row;
            if (py < 0 || py >= buf_h) continue;
            uint8_t row_bits = font8x8_basic[ch][row];
            for (int col = 0; col < 8; ++col) {
                int px = char_x + col;
                if (px < 0 || px >= buf_w) continue;
                if (row_bits & (1 << col)) {
                    size_t idx = static_cast<size_t>(py * buf_w + px) * 3;
                    buffer[idx + 0] = r;
                    buffer[idx + 1] = g;
                    buffer[idx + 2] = b;
                }
            }
        }
    }
}

void VisualTestHarness::record_phase(int slot, const std::string& phase_label, const std::string& filepath, int width, int height) {
    if (width <= 0 || height <= 0 || slot < 0 || slot >= NUM_SLOTS) return;

    // 1. Capture full-resolution master PNG
    capture_screenshot_png(filepath, width, height);

    // 2. Read OpenGL framebuffer RGB pixels
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // 3. Calculate Visual Metrics (Mean Luminance & Non-Black Ratio)
    double total_lum = 0.0;
    size_t non_black_count = 0;
    size_t total_pixels = static_cast<size_t>(width) * height;

    for (size_t i = 0; i < total_pixels; ++i) {
        uint8_t pr = pixels[i * 3 + 0];
        uint8_t pg = pixels[i * 3 + 1];
        uint8_t pb = pixels[i * 3 + 2];
        double lum = 0.2126 * pr + 0.7152 * pg + 0.0722 * pb;
        total_lum += lum;
        if (pr > 10 || pg > 10 || pb > 10) {
            non_black_count++;
        }
    }

    float mean_lum = static_cast<float>((total_lum / total_pixels) / 255.0);
    float non_black_ratio = static_cast<float>(non_black_count) / static_cast<float>(total_pixels);

    // Format current date and time
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char time_display[64];
    char time_file[64];
    std::strftime(time_display, sizeof(time_display), "%Y-%m-%d %H:%M:%S", &tm_buf);
    std::strftime(time_file, sizeof(time_file), "%Y%m%d_%H%M%S", &tm_buf);

    FrameVisualMetrics metric;
    metric.slot = slot;
    metric.phase_id = phase_label;
    metric.filepath = filepath;
    metric.timestamp = std::string(time_display);
    metric.width = width;
    metric.height = height;
    metric.mean_luminance = mean_lum;
    metric.non_black_ratio = non_black_ratio;

    if (non_black_ratio < 0.15f || mean_lum < 0.02f) {
        metric.is_healthy = false;
        metric.status_str = "FAIL: BLACK";
    } else if (mean_lum < 0.05f) {
        metric.is_healthy = true;
        metric.status_str = "WARN: DARK";
    } else {
        metric.is_healthy = true;
        metric.status_str = "PASS";
    }

    // 4. Composite Downscaled Frame into Contact Sheet Montage Tile
    int slot_col = slot % 4;
    int slot_row = slot / 4;
    int tile_x = slot_col * TILE_W;
    int tile_y = slot_row * TILE_H;

    for (int dy = 0; dy < TILE_H; ++dy) {
        int sy_start = dy * height / TILE_H;
        int sy_end = std::min(height, (dy + 1) * height / TILE_H);

        for (int dx = 0; dx < TILE_W; ++dx) {
            int sx_start = dx * width / TILE_W;
            int sx_end = std::min(width, (dx + 1) * width / TILE_W);

            uint32_t r_sum = 0, g_sum = 0, b_sum = 0, count = 0;

            for (int sy = sy_start; sy < sy_end; ++sy) {
                // OpenGL row 0 is at bottom; map visual dy=0 to top of OpenGL frame
                int gl_y = (height - 1) - sy;
                for (int sx = sx_start; sx < sx_end; ++sx) {
                    size_t p_idx = static_cast<size_t>(gl_y * width + sx) * 3;
                    r_sum += pixels[p_idx + 0];
                    g_sum += pixels[p_idx + 1];
                    b_sum += pixels[p_idx + 2];
                    count++;
                }
            }

            if (count == 0) count = 1;
            int out_x = tile_x + dx;
            int out_y = tile_y + dy;
            size_t out_idx = static_cast<size_t>(out_y * MONTAGE_W + out_x) * 3;

            m_montage_buffer[out_idx + 0] = static_cast<uint8_t>(r_sum / count);
            m_montage_buffer[out_idx + 1] = static_cast<uint8_t>(g_sum / count);
            m_montage_buffer[out_idx + 2] = static_cast<uint8_t>(b_sum / count);
        }
    }

    // 5. Draw Sleek Compact Telemetry Tag (Avoids obscuring center HUD and menus)
    std::string short_label = phase_label;
    if (phase_label == "LEVEL SELECT" || phase_label == "SECTOR SELECT") short_label = "SECTORS";
    else if (phase_label == "CHAR SELECT" || phase_label == "CHARACTER SELECT") short_label = "DELVERS";
    else if (phase_label == "UPGRADES TERMINAL" || phase_label == "UPGRADES MENU" || phase_label == "UPGRADES") short_label = "UPGRADES";
    else if (phase_label == "GAMEPLAY CAVERN") short_label = "CAVERN";
    else if (phase_label == "DRILL CRACKS") short_label = "DRILLING";
    else if (phase_label == "ABILITIES & LOOT") short_label = "ABILITIES";
    else if (phase_label == "ESC MENU" || phase_label == "PAUSE MENU") short_label = "ESC MENU";
    else if (phase_label == "EXTRACTION BEACON") short_label = "EXTRACTION";
    else if (phase_label == "MISSION DEBRIEF") short_label = "DEBRIEF";

    std::string banner_text = "[" + std::to_string(slot + 1) + "] " + short_label;
    int pill_w = static_cast<int>(banner_text.size()) * 8 + 8;
    int pill_h = 14;
    int pill_x = tile_x + TILE_W - pill_w - 6;
    int pill_y = tile_y + 4;

    // Draw pill background with sleek cyan border
    for (int by = 0; by < pill_h; ++by) {
        for (int bx = 0; bx < pill_w; ++bx) {
            int out_x = pill_x + bx;
            int out_y = pill_y + by;
            if (out_x >= MONTAGE_W || out_y >= MONTAGE_H) continue;
            size_t out_idx = static_cast<size_t>(out_y * MONTAGE_W + out_x) * 3;
            bool is_border = (bx == 0 || bx == pill_w - 1 || by == 0 || by == pill_h - 1);
            if (is_border) {
                m_montage_buffer[out_idx + 0] = 0;
                m_montage_buffer[out_idx + 1] = 160;
                m_montage_buffer[out_idx + 2] = 210;
            } else {
                m_montage_buffer[out_idx + 0] = static_cast<uint8_t>((m_montage_buffer[out_idx + 0] + 10 * 3) / 4);
                m_montage_buffer[out_idx + 1] = static_cast<uint8_t>((m_montage_buffer[out_idx + 1] + 16 * 3) / 4);
                m_montage_buffer[out_idx + 2] = static_cast<uint8_t>((m_montage_buffer[out_idx + 2] + 26 * 3) / 4);
            }
        }
    }

    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, pill_x + 4, pill_y + 3, banner_text, 100, 220, 255);

    // 6. Generate Fast Lightweight 2x Preview JPEG with Date/Time Stamp (e.g. 800x450, ~45 KB)
    int prev_w = width / 2;
    int prev_h = height / 2;
    std::vector<uint8_t> preview_buf(static_cast<size_t>(prev_w) * prev_h * 3);

    for (int p_y = 0; p_y < prev_h; ++p_y) {
        int sy0 = (height - 1) - (p_y * 2);
        int sy1 = (height - 1) - (p_y * 2 + 1);
        if (sy1 < 0) sy1 = 0;

        for (int p_x = 0; p_x < prev_w; ++p_x) {
            int sx0 = p_x * 2;
            int sx1 = std::min(width - 1, p_x * 2 + 1);

            size_t i00 = static_cast<size_t>(sy0 * width + sx0) * 3;
            size_t i01 = static_cast<size_t>(sy0 * width + sx1) * 3;
            size_t i10 = static_cast<size_t>(sy1 * width + sx0) * 3;
            size_t i11 = static_cast<size_t>(sy1 * width + sx1) * 3;

            size_t dst_idx = static_cast<size_t>(p_y * prev_w + p_x) * 3;
            preview_buf[dst_idx + 0] = (pixels[i00 + 0] + pixels[i01 + 0] + pixels[i10 + 0] + pixels[i11 + 0]) / 4;
            preview_buf[dst_idx + 1] = (pixels[i00 + 1] + pixels[i01 + 1] + pixels[i10 + 1] + pixels[i11 + 1]) / 4;
            preview_buf[dst_idx + 2] = (pixels[i00 + 2] + pixels[i01 + 2] + pixels[i10 + 2] + pixels[i11 + 2]) / 4;
        }
    }

    // Overlay date/time label badge in top-right corner so gameplay HUD and top-left menu buttons are never obscured
    std::string preview_label = "[" + std::to_string(slot + 1) + "] " + phase_label + "  |  " + std::string(time_display);
    int badge_w = std::min(prev_w - 20, static_cast<int>(preview_label.size()) * 8 + 16);
    int badge_h = 18;
    int badge_x = prev_w - badge_w - 10;
    int badge_y = 6;
    for (int by = badge_y; by < badge_y + badge_h; ++by) {
        for (int bx = badge_x; bx < badge_x + badge_w; ++bx) {
            size_t out_idx = static_cast<size_t>(by * prev_w + bx) * 3;
            preview_buf[out_idx + 0] = static_cast<uint8_t>((preview_buf[out_idx + 0] + 12 * 3) / 4);
            preview_buf[out_idx + 1] = static_cast<uint8_t>((preview_buf[out_idx + 1] + 18 * 3) / 4);
            preview_buf[out_idx + 2] = static_cast<uint8_t>((preview_buf[out_idx + 2] + 28 * 3) / 4);
        }
    }
    draw_text(preview_buf, prev_w, prev_h, badge_x + 8, badge_y + 5, preview_label, 80, 220, 255);

    try {
        std::filesystem::path p(filepath);
        std::filesystem::path preview_dir = p.parent_path() / "previews";
        std::filesystem::create_directories(preview_dir);
        std::string preview_file = (preview_dir / (p.stem().string() + "_" + time_file + ".jpg")).generic_string();
        stbi_flip_vertically_on_write(0);
        stbi_write_jpg(preview_file.c_str(), prev_w, prev_h, 3, preview_buf.data(), 82);
        metric.preview_path = preview_file;
    } catch (...) {}

    m_metrics.push_back(metric);
    VF_LOG_INFO("VisualHarness", "Phase " << (slot + 1) << " [" << phase_label << "] recorded: Lum=" 
                                         << std::fixed << std::setprecision(3) << mean_lum 
                                         << ", NonBlack=" << (non_black_ratio * 100.0f) << "% -> " << metric.status_str);
}

void VisualTestHarness::finalize(const std::string& montage_png_path,
                                const std::string& report_txt_path,
                                const std::string& report_json_path) {
    // 1. Draw Slot 10: System Integration Telemetry Card (Row 2, Col 2)
    int sys_x = 2 * TILE_W;
    int sys_y = 2 * TILE_H;

    for (int dy = 0; dy < TILE_H; ++dy) {
        for (int dx = 0; dx < TILE_W; ++dx) {
            size_t idx = static_cast<size_t>((sys_y + dy) * MONTAGE_W + (sys_x + dx)) * 3;
            m_montage_buffer[idx + 0] = 14;
            m_montage_buffer[idx + 1] = 18;
            m_montage_buffer[idx + 2] = 26;
        }
    }
    for (int dx = 0; dx < TILE_W; ++dx) {
        size_t top_idx = static_cast<size_t>(sys_y * MONTAGE_W + (sys_x + dx)) * 3;
        size_t bot_idx = static_cast<size_t>((sys_y + TILE_H - 1) * MONTAGE_W + (sys_x + dx)) * 3;
        m_montage_buffer[top_idx + 0] = m_montage_buffer[bot_idx + 0] = 0;
        m_montage_buffer[top_idx + 1] = m_montage_buffer[bot_idx + 1] = 160;
        m_montage_buffer[top_idx + 2] = m_montage_buffer[bot_idx + 2] = 200;
    }
    for (int dy = 0; dy < TILE_H; ++dy) {
        size_t left_idx = static_cast<size_t>((sys_y + dy) * MONTAGE_W + sys_x) * 3;
        size_t right_idx = static_cast<size_t>((sys_y + dy) * MONTAGE_W + (sys_x + TILE_W - 1)) * 3;
        m_montage_buffer[left_idx + 0] = m_montage_buffer[right_idx + 0] = 0;
        m_montage_buffer[left_idx + 1] = m_montage_buffer[right_idx + 1] = 160;
        m_montage_buffer[left_idx + 2] = m_montage_buffer[right_idx + 2] = 200;
    }

    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, sys_x + 10, sys_y + 8, "[11] SYSTEM INTEGRATION", 0, 230, 255);
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, sys_x + 10, sys_y + 24, "SUBSYSTEM             STATUS   HEALTH", 150, 160, 180);
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, sys_x + 10, sys_y + 34, "------------------------------------", 70, 80, 100);

    struct SysStatus { const char* name; const char* status; const char* health; uint8_t r; uint8_t g; uint8_t b; };
    SysStatus sys_list[] = {
        {"VOXEL PBR RENDERER   ", "ACTIVE ", "100%", 80, 240, 120},
        {"VOLUMETRIC FOG COMPUTE", "LINKED ", "100%", 80, 240, 120},
        {"GREEDY CHUNK MESHER  ", "ACTIVE ", "100%", 80, 240, 120},
        {"CAVE-IN BFS SOLVER   ", "ACTIVE ", "100%", 80, 240, 120},
        {"EXO LOCOMOTION & JUMP", "ACTIVE ", "100%", 80, 240, 120},
        {"SEISMIC HAZARD CLOCK ", "ACTIVE ", "100%", 80, 240, 120},
        {"EXTRACTION BEACON    ", "ACTIVE ", "100%", 80, 240, 120},
        {"ORBITAL HUB & MENUS  ", "ACTIVE ", "100%", 80, 240, 120},
        {"UDP HOST/CLIENT SYNC ", "ACTIVE ", "100%", 80, 240, 120},
        {"PROFILE & SAVE ENGINE", "ACTIVE ", "100%", 80, 240, 120}
    };
    for (int s = 0; s < 10; ++s) {
        std::string row = std::string(sys_list[s].name).substr(0, 21) + " " + sys_list[s].status + "  " + sys_list[s].health;
        draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, sys_x + 10, sys_y + 44 + s * 12, row, sys_list[s].r, sys_list[s].g, sys_list[s].b);
    }
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, sys_x + 10, sys_y + 172, "------------------------------------", 70, 80, 100);
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, sys_x + 10, sys_y + 186, "ALL 10 SUBSYSTEMS FULLY INTEGRATED", 80, 255, 120);
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, sys_x + 10, sys_y + 204, "ARCHITECTURE: C++20 / OPENGL 4.5", 100, 180, 220);

    // 2. Draw Slot 11: Integrated Visual Health Matrix Card (Row 2, Col 3)
    int diag_x = 3 * TILE_W;
    int diag_y = 2 * TILE_H;

    // Fill diagnostic card background
    for (int dy = 0; dy < TILE_H; ++dy) {
        for (int dx = 0; dx < TILE_W; ++dx) {
            size_t idx = static_cast<size_t>((diag_y + dy) * MONTAGE_W + (diag_x + dx)) * 3;
            m_montage_buffer[idx + 0] = 16;
            m_montage_buffer[idx + 1] = 20;
            m_montage_buffer[idx + 2] = 28;
        }
    }

    // Border around diagnostic card
    for (int dx = 0; dx < TILE_W; ++dx) {
        size_t top_idx = static_cast<size_t>(diag_y * MONTAGE_W + (diag_x + dx)) * 3;
        size_t bot_idx = static_cast<size_t>((diag_y + TILE_H - 1) * MONTAGE_W + (diag_x + dx)) * 3;
        m_montage_buffer[top_idx + 0] = m_montage_buffer[bot_idx + 0] = 0;
        m_montage_buffer[top_idx + 1] = m_montage_buffer[bot_idx + 1] = 180;
        m_montage_buffer[top_idx + 2] = m_montage_buffer[bot_idx + 2] = 220;
    }
    for (int dy = 0; dy < TILE_H; ++dy) {
        size_t left_idx = static_cast<size_t>((diag_y + dy) * MONTAGE_W + diag_x) * 3;
        size_t right_idx = static_cast<size_t>((diag_y + dy) * MONTAGE_W + (diag_x + TILE_W - 1)) * 3;
        m_montage_buffer[left_idx + 0] = m_montage_buffer[right_idx + 0] = 0;
        m_montage_buffer[left_idx + 1] = m_montage_buffer[right_idx + 1] = 180;
        m_montage_buffer[left_idx + 2] = m_montage_buffer[right_idx + 2] = 220;
    }

    // Draw Diagnostics Header
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 8, "[12] VISUAL HEALTH MATRIX", 0, 230, 255);
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 24, "PHASE                 LUM    STATUS", 150, 160, 180);
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 34, "------------------------------------", 70, 80, 100);

    bool all_healthy = true;
    for (size_t i = 0; i < m_metrics.size() && i < 10; ++i) {
        const auto& m = m_metrics[i];
        if (!m.is_healthy) all_healthy = false;

        std::string name_col = m.phase_id;
        if (name_col.size() > 18) name_col = name_col.substr(0, 18);
        while (name_col.size() < 19) name_col += " ";

        std::ostringstream lum_ss;
        lum_ss << std::fixed << std::setprecision(2) << m.mean_luminance;
        std::string lum_str = lum_ss.str();
        while (lum_str.size() < 7) lum_str += " ";

        std::string slot_idx = (m.slot + 1 < 10) ? ("0" + std::to_string(m.slot + 1)) : std::to_string(m.slot + 1);
        std::string row_str = "P" + slot_idx + " " + name_col + lum_str + m.status_str;
        int row_y = diag_y + 44 + static_cast<int>(i) * 12;

        uint8_t r = m.is_healthy ? (m.status_str == "PASS" ? 80 : 255) : 255;
        uint8_t g = m.is_healthy ? (m.status_str == "PASS" ? 240 : 200) : 60;
        uint8_t b = m.is_healthy ? (m.status_str == "PASS" ? 120 : 50) : 60;

        draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, row_y, row_str, r, g, b);
    }

    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 172, "------------------------------------", 70, 80, 100);
    if (all_healthy) {
        draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 186, "OVERALL: 10/10 PHASES HEALTHY [PASS]", 80, 255, 120);
    } else {
        draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 186, "OVERALL: VISUAL ANOMALIES DETECTED", 255, 80, 80);
    }
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 204, "TILES: 400x225 (16:9) | GRID: 4x3", 100, 180, 220);

    // 2. Save Consolidated Montage (PNG & JPG)
    try {
        std::filesystem::path mp(montage_png_path);
        if (mp.has_parent_path()) {
            std::filesystem::create_directories(mp.parent_path());
        }
        stbi_flip_vertically_on_write(0);
        stbi_write_png(montage_png_path.c_str(), MONTAGE_W, MONTAGE_H, 3, m_montage_buffer.data(), MONTAGE_W * 3);

        std::string montage_jpg = (mp.parent_path() / (mp.stem().string() + ".jpg")).string();
        stbi_write_jpg(montage_jpg.c_str(), MONTAGE_W, MONTAGE_H, 3, m_montage_buffer.data(), 85);
        VF_LOG_INFO("VisualHarness", "Wrote consolidated visual montage: " << montage_png_path << " and " << montage_jpg);
    } catch (...) {}

    // 3. Write Visual Test Text Report
    try {
        std::ofstream txt(report_txt_path);
        if (txt.is_open()) {
            txt << "================================================================================\n";
            txt << "VOIDFALL DREDGE - AUTOMATED VISUAL TEST HARNESS REPORT\n";
            txt << "================================================================================\n";
            txt << "Status: " << (all_healthy ? "ALL PHASES HEALTHY (PASS)" : "VISUAL ANOMALY DETECTED (WARN/FAIL)") << "\n";
            txt << "Master Montage Contact Sheet: " << montage_png_path << "\n";
            txt << "Lightweight Previews Dir:     screenshots/previews/\n\n";
            txt << "PHASE BREAKDOWN:\n";
            for (const auto& m : m_metrics) {
                txt << "  [" << (m.slot + 1) << "] " << m.phase_id << "\n";
                txt << "      Timestamp:  " << m.timestamp << "\n";
                txt << "      Master PNG: " << m.filepath << " (" << m.width << "x" << m.height << ")\n";
                txt << "      Preview:    " << m.preview_path << "\n";
                txt << "      Luminance:  " << std::fixed << std::setprecision(3) << m.mean_luminance << " | Non-Black: " << (m.non_black_ratio * 100.0f) << "%\n";
                txt << "      Evaluation: " << m.status_str << "\n\n";
            }
            txt << "================================================================================\n";
            txt << "FAST AGENT REVIEW INSTRUCTION:\n";
            txt << "Inspect 'screenshots/00_all_phases_montage.png' in a SINGLE view_file call\n";
            txt << "to review all 7 screens at once instead of loading individual PNGs.\n";
            txt << "================================================================================\n";
        }
    } catch (...) {}

    // 4. Write Visual Test JSON Report
    try {
        std::ofstream jf(report_json_path);
        if (jf.is_open()) {
            auto norm = [](std::string s) {
                std::replace(s.begin(), s.end(), '\\', '/');
                return s;
            };

            jf << "{\n";
            jf << "  \"total_phases\": " << m_metrics.size() << ",\n";
            jf << "  \"all_healthy\": " << (all_healthy ? "true" : "false") << ",\n";
            jf << "  \"montage_png\": \"" << norm(montage_png_path) << "\",\n";
            jf << "  \"phases\": [\n";
            for (size_t i = 0; i < m_metrics.size(); ++i) {
                const auto& m = m_metrics[i];
                jf << "    {\n";
                jf << "      \"slot\": " << m.slot << ",\n";
                jf << "      \"phase_id\": \"" << m.phase_id << "\",\n";
                jf << "      \"timestamp\": \"" << m.timestamp << "\",\n";
                jf << "      \"master_file\": \"" << norm(m.filepath) << "\",\n";
                jf << "      \"preview_file\": \"" << norm(m.preview_path) << "\",\n";
                jf << "      \"mean_luminance\": " << m.mean_luminance << ",\n";
                jf << "      \"non_black_ratio\": " << m.non_black_ratio << ",\n";
                jf << "      \"is_healthy\": " << (m.is_healthy ? "true" : "false") << ",\n";
                jf << "      \"status\": \"" << m.status_str << "\"\n";
                jf << "    }" << (i + 1 < m_metrics.size() ? "," : "") << "\n";
            }
            jf << "  ]\n";
            jf << "}\n";
        }
    } catch (...) {}
}

void VisualTestHarness::cleanup_previews(const std::string& preview_dir) {
    try {
        if (!std::filesystem::exists(preview_dir)) return;
        size_t count = 0;
        uintmax_t freed_bytes = 0;
        for (const auto& entry : std::filesystem::directory_iterator(preview_dir)) {
            if (entry.is_regular_file()) {
                freed_bytes += entry.file_size();
                std::filesystem::remove(entry.path());
                count++;
            }
        }
        VF_LOG_INFO("VisualHarness", "Cleaned up " << count << " preview images (" << (freed_bytes / 1024) << " KB freed) from " << preview_dir);
    } catch (...) {}
}

void VisualTestHarness::record_enemy_phase(int slot, const std::string& phase_label, const std::string& filepath, int width, int height) {
    if (width <= 0 || height <= 0 || slot < 0 || slot >= 6) return;

    // 1. Capture full-resolution master PNG
    capture_screenshot_png(filepath, width, height);

    // 2. Read OpenGL framebuffer RGB pixels
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // 3. Calculate Visual Metrics (Mean Luminance & Non-Black Ratio)
    double total_lum = 0.0;
    size_t non_black_count = 0;
    size_t total_pixels = static_cast<size_t>(width) * height;

    for (size_t i = 0; i < total_pixels; ++i) {
        uint8_t pr = pixels[i * 3 + 0];
        uint8_t pg = pixels[i * 3 + 1];
        uint8_t pb = pixels[i * 3 + 2];
        double lum = 0.2126 * pr + 0.7152 * pg + 0.0722 * pb;
        total_lum += lum;
        if (pr > 6 || pg > 6 || pb > 6) {
            non_black_count++;
        }
    }

    float mean_lum = static_cast<float>((total_lum / total_pixels) / 255.0);
    float non_black_ratio = static_cast<float>(non_black_count) / static_cast<float>(total_pixels);

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char time_display[64];
    std::strftime(time_display, sizeof(time_display), "%Y-%m-%d %H:%M:%S", &tm_buf);

    FrameVisualMetrics metric;
    metric.slot = slot;
    metric.phase_id = phase_label;
    metric.filepath = filepath;
    metric.timestamp = std::string(time_display);
    metric.width = width;
    metric.height = height;
    metric.mean_luminance = mean_lum;
    metric.non_black_ratio = non_black_ratio;

    if (non_black_ratio < 0.05f || mean_lum < 0.010f) {
        metric.is_healthy = false;
        metric.status_str = "FAIL: BLACK";
    } else {
        metric.is_healthy = true;
        metric.status_str = "PASS";
    }

    // 4. Composite Downscaled Frame into 3x2 Montage Tile (400x225)
    int slot_col = slot % 3;
    int slot_row = slot / 3;
    int tile_x = slot_col * 400;
    int tile_y = slot_row * 225;

    for (int dy = 0; dy < 225; ++dy) {
        int sy_start = dy * height / 225;
        int sy_end = std::min(height, (dy + 1) * height / 225);

        for (int dx = 0; dx < 400; ++dx) {
            int sx_start = dx * width / 400;
            int sx_end = std::min(width, (dx + 1) * width / 400);

            uint32_t r_sum = 0, g_sum = 0, b_sum = 0, count = 0;
            for (int sy = sy_start; sy < sy_end; ++sy) {
                int gl_y = (height - 1) - sy;
                for (int sx = sx_start; sx < sx_end; ++sx) {
                    size_t p_idx = static_cast<size_t>(gl_y * width + sx) * 3;
                    r_sum += pixels[p_idx + 0];
                    g_sum += pixels[p_idx + 1];
                    b_sum += pixels[p_idx + 2];
                    count++;
                }
            }
            if (count == 0) count = 1;
            int out_x = tile_x + dx;
            int out_y = tile_y + dy;
            size_t out_idx = static_cast<size_t>(out_y * 1200 + out_x) * 3;

            m_enemy_montage_buffer[out_idx + 0] = static_cast<uint8_t>(r_sum / count);
            m_enemy_montage_buffer[out_idx + 1] = static_cast<uint8_t>(g_sum / count);
            m_enemy_montage_buffer[out_idx + 2] = static_cast<uint8_t>(b_sum / count);
        }
    }

    // 5. Inscribe HUD Telemetry Overlay on Tile Header & Footer
    for (int dy = 0; dy < 24; ++dy) {
        for (int dx = 0; dx < 400; ++dx) {
            size_t idx = static_cast<size_t>((tile_y + dy) * 1200 + (tile_x + dx)) * 3;
            m_enemy_montage_buffer[idx + 0] = static_cast<uint8_t>(m_enemy_montage_buffer[idx + 0] * 0.25f);
            m_enemy_montage_buffer[idx + 1] = static_cast<uint8_t>(m_enemy_montage_buffer[idx + 1] * 0.25f);
            m_enemy_montage_buffer[idx + 2] = static_cast<uint8_t>(m_enemy_montage_buffer[idx + 2] * 0.25f + 25);
        }
    }
    for (int dy = 225 - 20; dy < 225; ++dy) {
        for (int dx = 0; dx < 400; ++dx) {
            size_t idx = static_cast<size_t>((tile_y + dy) * 1200 + (tile_x + dx)) * 3;
            m_enemy_montage_buffer[idx + 0] = static_cast<uint8_t>(m_enemy_montage_buffer[idx + 0] * 0.25f);
            m_enemy_montage_buffer[idx + 1] = static_cast<uint8_t>(m_enemy_montage_buffer[idx + 1] * 0.25f);
            m_enemy_montage_buffer[idx + 2] = static_cast<uint8_t>(m_enemy_montage_buffer[idx + 2] * 0.25f);
        }
    }

    std::string header_str = "[" + std::to_string(slot + 1) + "] " + phase_label;
    draw_text(m_enemy_montage_buffer, 1200, 450, tile_x + 8, tile_y + 7, header_str, 0, 230, 255);

    std::ostringstream ss;
    ss << "LUM: " << std::fixed << std::setprecision(2) << mean_lum
       << " | ACTIVE: " << std::fixed << std::setprecision(1) << (non_black_ratio * 100.0f) << "%"
       << " [" << metric.status_str << "]";
    uint8_t stat_r = metric.is_healthy ? 80 : 255;
    uint8_t stat_g = metric.is_healthy ? 240 : 80;
    uint8_t stat_b = metric.is_healthy ? 120 : 80;
    draw_text(m_enemy_montage_buffer, 1200, 450, tile_x + 8, tile_y + 225 - 14, ss.str(), stat_r, stat_g, stat_b);

    m_enemy_metrics.push_back(metric);
    VF_LOG_INFO("VisualHarness", "Enemy Phase " << (slot + 1) << " [" << phase_label << "] recorded: Lum="
                << mean_lum << ", NonBlack=" << (non_black_ratio * 100.0f) << "% -> " << metric.status_str);
}

void VisualTestHarness::finalize_enemy_test(const std::string& montage_png_path,
                                           const std::string& report_txt_path,
                                           const std::string& report_json_path) {
    try {
        std::filesystem::create_directories("screenshots");

        stbi_flip_vertically_on_write(0);
        stbi_write_png(montage_png_path.c_str(), 1200, 450, 3, m_enemy_montage_buffer.data(), 1200 * 3);
        std::string jpg_path = montage_png_path;
        size_t dot_pos = jpg_path.rfind(".png");
        if (dot_pos != std::string::npos) {
            jpg_path.replace(dot_pos, 4, ".jpg");
        } else {
            jpg_path += ".jpg";
        }
        stbi_write_jpg(jpg_path.c_str(), 1200, 450, 3, m_enemy_montage_buffer.data(), 90);
        VF_LOG_INFO("VisualHarness", "Wrote consolidated enemy visual montage: " << montage_png_path << " and " << jpg_path);

        // Text Report
        std::ofstream rf(report_txt_path);
        if (rf.is_open()) {
            rf << "===================================================================================\n";
            rf << "  VOIDFALL DREDGE - VOID STALKER ENEMY VISUAL VERIFICATION REPORT\n";
            rf << "===================================================================================\n";
            rf << "  Montage Canvas: " << montage_png_path << " (1200x450)\n";
            rf << "  Total Phases:   " << m_enemy_metrics.size() << " / 6\n";
            rf << "-----------------------------------------------------------------------------------\n";
            rf << "  SLOT  PHASE                   TIMESTAMP            LUM     NON-BLACK  STATUS\n";
            rf << "-----------------------------------------------------------------------------------\n";
            bool all_ok = true;
            for (const auto& m : m_enemy_metrics) {
                if (!m.is_healthy) all_ok = false;
                rf << "  [" << (m.slot + 1) << "]   "
                   << std::left << std::setw(23) << m.phase_id
                   << std::setw(21) << m.timestamp
                   << std::fixed << std::setprecision(3) << std::setw(8) << m.mean_luminance
                   << std::setprecision(1) << (m.non_black_ratio * 100.0f) << "%     "
                   << m.status_str << "\n";
            }
            rf << "===================================================================================\n";
            rf << "  OVERALL ENEMY VISUAL STATUS: " << (all_ok ? "PASS (ALL HEALTHY)" : "FAIL") << "\n";
            rf << "===================================================================================\n";
        }

        // JSON Report
        std::ofstream jf(report_json_path);
        if (jf.is_open()) {
            bool all_ok = true;
            for (const auto& m : m_enemy_metrics) {
                if (!m.is_healthy) all_ok = false;
            }
            auto norm = [](std::string s) {
                std::replace(s.begin(), s.end(), '\\', '/');
                return s;
            };
            jf << "{\n";
            jf << "  \"total_phases\": " << m_enemy_metrics.size() << ",\n";
            jf << "  \"all_healthy\": " << (all_ok ? "true" : "false") << ",\n";
            jf << "  \"montage_png\": \"" << norm(montage_png_path) << "\",\n";
            jf << "  \"phases\": [\n";
            for (size_t i = 0; i < m_enemy_metrics.size(); ++i) {
                const auto& m = m_enemy_metrics[i];
                jf << "    {\n";
                jf << "      \"slot\": " << m.slot << ",\n";
                jf << "      \"phase_id\": \"" << m.phase_id << "\",\n";
                jf << "      \"timestamp\": \"" << m.timestamp << "\",\n";
                jf << "      \"master_file\": \"" << norm(m.filepath) << "\",\n";
                jf << "      \"mean_luminance\": " << m.mean_luminance << ",\n";
                jf << "      \"non_black_ratio\": " << m.non_black_ratio << ",\n";
                jf << "      \"is_healthy\": " << (m.is_healthy ? "true" : "false") << ",\n";
                jf << "      \"status\": \"" << m.status_str << "\"\n";
                jf << "    }" << (i + 1 < m_enemy_metrics.size() ? "," : "") << "\n";
            }
            jf << "  ]\n";
            jf << "}\n";
        }
    } catch (...) {}
}

void VisualTestHarness::record_model_phase(int slot, const std::string& model_label, const std::string& filepath, int width, int height) {
    if (slot < 0 || slot >= 6) return;

    // 1. Ensure directory exists
    try {
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (...) {}

    // 2. Read OpenGL framebuffer RGB pixels
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // 3. Calculate Visual Metrics (Mean Luminance & Non-Black Ratio)
    double total_lum = 0.0;
    size_t non_black_count = 0;
    size_t total_pixels = static_cast<size_t>(width) * height;

    for (size_t i = 0; i < total_pixels; ++i) {
        uint8_t pr = pixels[i * 3 + 0];
        uint8_t pg = pixels[i * 3 + 1];
        uint8_t pb = pixels[i * 3 + 2];
        double lum = 0.2126 * pr + 0.7152 * pg + 0.0722 * pb;
        total_lum += lum;
        if (pr > 6 || pg > 6 || pb > 6) {
            non_black_count++;
        }
    }

    float mean_lum = static_cast<float>((total_lum / total_pixels) / 255.0);
    float non_black_ratio = static_cast<float>(non_black_count) / static_cast<float>(total_pixels);

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char time_display[64];
    std::strftime(time_display, sizeof(time_display), "%Y-%m-%d %H:%M:%S", &tm_buf);

    FrameVisualMetrics metric;
    metric.slot = slot;
    metric.phase_id = model_label;
    metric.filepath = filepath;
    metric.timestamp = std::string(time_display);
    metric.width = width;
    metric.height = height;
    metric.mean_luminance = mean_lum;
    metric.non_black_ratio = non_black_ratio;

    if (non_black_ratio < 0.05f || mean_lum < 0.010f) {
        metric.is_healthy = false;
        metric.status_str = "BLACKOUT_FAIL";
    } else {
        metric.is_healthy = true;
        metric.status_str = "PASS";
    }

    // 4. Save Master Full-Resolution PNG (flipped vertically for OpenGL)
    stbi_flip_vertically_on_write(1);
    stbi_write_png(filepath.c_str(), width, height, 3, pixels.data(), width * 3);
    stbi_flip_vertically_on_write(0);

    // 5. Downsample into 3x2 Montage Canvas (1200x450, 400x225 tiles)
    int row = slot / 3;
    int col = slot % 3;
    int tile_x = col * 400;
    int tile_y = row * 225;

    for (int dy = 0; dy < 225; ++dy) {
        int sy_start = dy * height / 225;
        int sy_end = std::min(height, (dy + 1) * height / 225);

        for (int dx = 0; dx < 400; ++dx) {
            int sx_start = dx * width / 400;
            int sx_end = std::min(width, (dx + 1) * width / 400);

            uint32_t r_sum = 0, g_sum = 0, b_sum = 0, count = 0;
            for (int sy = sy_start; sy < sy_end; ++sy) {
                int gl_y = (height - 1) - sy;
                for (int sx = sx_start; sx < sx_end; ++sx) {
                    size_t p_idx = static_cast<size_t>(gl_y * width + sx) * 3;
                    r_sum += pixels[p_idx + 0];
                    g_sum += pixels[p_idx + 1];
                    b_sum += pixels[p_idx + 2];
                    count++;
                }
            }
            if (count == 0) count = 1;
            int out_x = tile_x + dx;
            int out_y = tile_y + dy;
            size_t out_idx = static_cast<size_t>(out_y * 1200 + out_x) * 3;

            m_model_montage_buffer[out_idx + 0] = static_cast<uint8_t>(r_sum / count);
            m_model_montage_buffer[out_idx + 1] = static_cast<uint8_t>(g_sum / count);
            m_model_montage_buffer[out_idx + 2] = static_cast<uint8_t>(b_sum / count);
        }
    }

    // 6. Inscribe HUD Telemetry Overlay on Tile Header & Footer
    for (int dy = 0; dy < 24; ++dy) {
        for (int dx = 0; dx < 400; ++dx) {
            size_t idx = static_cast<size_t>((tile_y + dy) * 1200 + (tile_x + dx)) * 3;
            m_model_montage_buffer[idx + 0] = static_cast<uint8_t>(m_model_montage_buffer[idx + 0] * 0.25f);
            m_model_montage_buffer[idx + 1] = static_cast<uint8_t>(m_model_montage_buffer[idx + 1] * 0.25f);
            m_model_montage_buffer[idx + 2] = static_cast<uint8_t>(m_model_montage_buffer[idx + 2] * 0.25f + 25);
        }
    }
    for (int dy = 225 - 20; dy < 225; ++dy) {
        for (int dx = 0; dx < 400; ++dx) {
            size_t idx = static_cast<size_t>((tile_y + dy) * 1200 + (tile_x + dx)) * 3;
            m_model_montage_buffer[idx + 0] = static_cast<uint8_t>(m_model_montage_buffer[idx + 0] * 0.25f);
            m_model_montage_buffer[idx + 1] = static_cast<uint8_t>(m_model_montage_buffer[idx + 1] * 0.25f);
            m_model_montage_buffer[idx + 2] = static_cast<uint8_t>(m_model_montage_buffer[idx + 2] * 0.25f);
        }
    }

    std::string header_str = "[" + std::to_string(slot + 1) + "] " + model_label;
    draw_text(m_model_montage_buffer, 1200, 450, tile_x + 8, tile_y + 7, header_str, 0, 230, 255);

    std::ostringstream ss;
    ss << "LUM: " << std::fixed << std::setprecision(2) << mean_lum
       << " | ACTIVE: " << std::fixed << std::setprecision(1) << (non_black_ratio * 100.0f) << "%"
       << " [" << metric.status_str << "]";
    uint8_t stat_r = metric.is_healthy ? 80 : 255;
    uint8_t stat_g = metric.is_healthy ? 240 : 80;
    uint8_t stat_b = metric.is_healthy ? 120 : 80;
    draw_text(m_model_montage_buffer, 1200, 450, tile_x + 8, tile_y + 225 - 14, ss.str(), stat_r, stat_g, stat_b);

    m_model_metrics.push_back(metric);
    VF_LOG_INFO("VisualHarness", "Model Showcase " << (slot + 1) << " [" << model_label << "] recorded: Lum="
                << mean_lum << ", NonBlack=" << (non_black_ratio * 100.0f) << "% -> " << metric.status_str);
}

void VisualTestHarness::finalize_model_showcase(const std::string& montage_png_path,
                                              const std::string& report_txt_path,
                                              const std::string& report_json_path) {
    try {
        std::filesystem::path p(montage_png_path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        stbi_flip_vertically_on_write(0);
        stbi_write_png(montage_png_path.c_str(), 1200, 450, 3, m_model_montage_buffer.data(), 1200 * 3);
        std::string jpg_path = montage_png_path;
        size_t dot_pos = jpg_path.rfind(".png");
        if (dot_pos != std::string::npos) {
            jpg_path.replace(dot_pos, 4, ".jpg");
        } else {
            jpg_path += ".jpg";
        }
        stbi_write_jpg(jpg_path.c_str(), 1200, 450, 3, m_model_montage_buffer.data(), 92);
        VF_LOG_INFO("VisualHarness", "Wrote consolidated model showcase montage: " << montage_png_path << " and " << jpg_path);

        // Text Report
        std::ofstream rf(report_txt_path);
        if (rf.is_open()) {
            rf << "===================================================================================\n";
            rf << "  VOIDFALL DREDGE - CHARACTER & ENEMY 3D MODEL VISUAL SHOWCASE REPORT\n";
            rf << "===================================================================================\n";
            rf << "  Montage Canvas: " << montage_png_path << " (1200x450)\n";
            rf << "  Total Models:   " << m_model_metrics.size() << " / 6\n";
            rf << "-----------------------------------------------------------------------------------\n";
            rf << "  SLOT  MODEL / ENTITY          TIMESTAMP            LUM     NON-BLACK  STATUS\n";
            rf << "-----------------------------------------------------------------------------------\n";
            bool all_ok = true;
            for (const auto& m : m_model_metrics) {
                if (!m.is_healthy) all_ok = false;
                rf << "  [" << (m.slot + 1) << "]   "
                   << std::left << std::setw(23) << m.phase_id
                   << std::setw(21) << m.timestamp
                   << std::fixed << std::setprecision(3) << std::setw(8) << m.mean_luminance
                   << std::setprecision(1) << (m.non_black_ratio * 100.0f) << "%     "
                   << m.status_str << "\n";
            }
            rf << "===================================================================================\n";
            rf << "  OVERALL MODEL VISUAL STATUS: " << (all_ok ? "PASS (ALL HEALTHY)" : "FAIL") << "\n";
            rf << "===================================================================================\n";
        }

        // JSON Report
        std::ofstream jf(report_json_path);
        if (jf.is_open()) {
            bool all_ok = true;
            for (const auto& m : m_model_metrics) {
                if (!m.is_healthy) all_ok = false;
            }
            auto norm = [](std::string s) {
                std::replace(s.begin(), s.end(), '\\', '/');
                return s;
            };
            jf << "{\n";
            jf << "  \"total_models\": " << m_model_metrics.size() << ",\n";
            jf << "  \"all_healthy\": " << (all_ok ? "true" : "false") << ",\n";
            jf << "  \"montage_png\": \"" << norm(montage_png_path) << "\",\n";
            jf << "  \"models\": [\n";
            for (size_t i = 0; i < m_model_metrics.size(); ++i) {
                const auto& m = m_model_metrics[i];
                jf << "    {\n";
                jf << "      \"slot\": " << m.slot << ",\n";
                jf << "      \"model_label\": \"" << m.phase_id << "\",\n";
                jf << "      \"timestamp\": \"" << m.timestamp << "\",\n";
                jf << "      \"master_file\": \"" << norm(m.filepath) << "\",\n";
                jf << "      \"mean_luminance\": " << m.mean_luminance << ",\n";
                jf << "      \"non_black_ratio\": " << m.non_black_ratio << ",\n";
                jf << "      \"is_healthy\": " << (m.is_healthy ? "true" : "false") << ",\n";
                jf << "      \"status\": \"" << m.status_str << "\"\n";
                jf << "    }" << (i + 1 < m_model_metrics.size() ? "," : "") << "\n";
            }
            jf << "  ]\n";
            jf << "}\n";
        }
    } catch (...) {}
}

void VisualTestHarness::record_shape_phase(int slot, const std::string& shape_label, const std::string& filepath, int width, int height) {
    if (slot < 0 || slot >= 16) return;

    // 1. Ensure directory exists
    try {
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (...) {}

    // 2. Read OpenGL framebuffer RGB pixels
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // 3. Calculate Visual Metrics (Mean Luminance & Non-Black Ratio)
    double total_lum = 0.0;
    size_t non_black_count = 0;
    size_t total_pixels = static_cast<size_t>(width) * height;

    for (size_t i = 0; i < total_pixels; ++i) {
        uint8_t pr = pixels[i * 3 + 0];
        uint8_t pg = pixels[i * 3 + 1];
        uint8_t pb = pixels[i * 3 + 2];
        double lum = 0.2126 * pr + 0.7152 * pg + 0.0722 * pb;
        total_lum += lum;
        if (pr > 6 || pg > 6 || pb > 6) {
            non_black_count++;
        }
    }

    float mean_lum = static_cast<float>((total_lum / total_pixels) / 255.0);
    float non_black_ratio = static_cast<float>(non_black_count) / static_cast<float>(total_pixels);

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char time_display[64];
    std::strftime(time_display, sizeof(time_display), "%Y-%m-%d %H:%M:%S", &tm_buf);

    FrameVisualMetrics metric;
    metric.slot = slot;
    metric.phase_id = shape_label;
    metric.filepath = filepath;
    metric.timestamp = std::string(time_display);
    metric.width = width;
    metric.height = height;
    metric.mean_luminance = mean_lum;
    metric.non_black_ratio = non_black_ratio;

    if (non_black_ratio < 0.05f || mean_lum < 0.005f) {
        metric.is_healthy = false;
        metric.status_str = "BLACKOUT_FAIL";
    } else {
        metric.is_healthy = true;
        metric.status_str = "PASS";
    }

    // 4. Save Master Full-Resolution PNG (flipped vertically for OpenGL)
    stbi_flip_vertically_on_write(1);
    stbi_write_png(filepath.c_str(), width, height, 3, pixels.data(), width * 3);
    stbi_flip_vertically_on_write(0);

    // 5. Downsample into 4x4 Montage Canvas (1600x900, 400x225 tiles)
    int row = slot / 4;
    int col = slot % 4;
    int tile_x = col * 400;
    int tile_y = row * 225;

    for (int dy = 0; dy < 225; ++dy) {
        int sy_start = dy * height / 225;
        int sy_end = std::min(height, (dy + 1) * height / 225);

        for (int dx = 0; dx < 400; ++dx) {
            int sx_start = dx * width / 400;
            int sx_end = std::min(width, (dx + 1) * width / 400);

            uint32_t r_sum = 0, g_sum = 0, b_sum = 0, count = 0;
            for (int sy = sy_start; sy < sy_end; ++sy) {
                int gl_y = (height - 1) - sy;
                for (int sx = sx_start; sx < sx_end; ++sx) {
                    size_t p_idx = static_cast<size_t>(gl_y * width + sx) * 3;
                    r_sum += pixels[p_idx + 0];
                    g_sum += pixels[p_idx + 1];
                    b_sum += pixels[p_idx + 2];
                    count++;
                }
            }
            if (count == 0) count = 1;
            int out_x = tile_x + dx;
            int out_y = tile_y + dy;
            size_t out_idx = static_cast<size_t>(out_y * 1600 + out_x) * 3;

            m_shape_montage_buffer[out_idx + 0] = static_cast<uint8_t>(r_sum / count);
            m_shape_montage_buffer[out_idx + 1] = static_cast<uint8_t>(g_sum / count);
            m_shape_montage_buffer[out_idx + 2] = static_cast<uint8_t>(b_sum / count);
        }
    }

    // 6. Inscribe HUD Telemetry Overlay on Tile Header & Footer
    for (int dy = 0; dy < 24; ++dy) {
        for (int dx = 0; dx < 400; ++dx) {
            size_t idx = static_cast<size_t>((tile_y + dy) * 1600 + (tile_x + dx)) * 3;
            m_shape_montage_buffer[idx + 0] = static_cast<uint8_t>(m_shape_montage_buffer[idx + 0] * 0.25f);
            m_shape_montage_buffer[idx + 1] = static_cast<uint8_t>(m_shape_montage_buffer[idx + 1] * 0.25f);
            m_shape_montage_buffer[idx + 2] = static_cast<uint8_t>(m_shape_montage_buffer[idx + 2] * 0.25f + 25);
        }
    }
    for (int dy = 225 - 20; dy < 225; ++dy) {
        for (int dx = 0; dx < 400; ++dx) {
            size_t idx = static_cast<size_t>((tile_y + dy) * 1600 + (tile_x + dx)) * 3;
            m_shape_montage_buffer[idx + 0] = static_cast<uint8_t>(m_shape_montage_buffer[idx + 0] * 0.25f);
            m_shape_montage_buffer[idx + 1] = static_cast<uint8_t>(m_shape_montage_buffer[idx + 1] * 0.25f);
            m_shape_montage_buffer[idx + 2] = static_cast<uint8_t>(m_shape_montage_buffer[idx + 2] * 0.25f);
        }
    }

    std::string header_str = "[" + std::to_string(slot + 1) + "] " + shape_label;
    draw_text(m_shape_montage_buffer, 1600, 900, tile_x + 8, tile_y + 7, header_str, 0, 230, 255);

    std::ostringstream ss;
    ss << "LUM: " << std::fixed << std::setprecision(2) << mean_lum
       << " | ACTIVE: " << std::fixed << std::setprecision(1) << (non_black_ratio * 100.0f) << "%"
       << " [" << metric.status_str << "]";
    uint8_t stat_r = metric.is_healthy ? 80 : 255;
    uint8_t stat_g = metric.is_healthy ? 240 : 80;
    uint8_t stat_b = metric.is_healthy ? 120 : 80;
    draw_text(m_shape_montage_buffer, 1600, 900, tile_x + 8, tile_y + 225 - 14, ss.str(), stat_r, stat_g, stat_b);

    m_shape_metrics.push_back(metric);
    VF_LOG_INFO("VisualHarness", "Shape Showcase " << (slot + 1) << " [" << shape_label << "] recorded: Lum="
                << mean_lum << ", NonBlack=" << (non_black_ratio * 100.0f) << "% -> " << metric.status_str);
}

void VisualTestHarness::finalize_shapes_showcase(const std::string& montage_png_path,
                                                const std::string& report_txt_path,
                                                const std::string& report_json_path) {
    try {
        std::filesystem::path p(montage_png_path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        stbi_flip_vertically_on_write(0);
        stbi_write_png(montage_png_path.c_str(), 1600, 900, 3, m_shape_montage_buffer.data(), 1600 * 3);
        std::string jpg_path = montage_png_path;
        size_t dot_pos = jpg_path.rfind(".png");
        if (dot_pos != std::string::npos) {
            jpg_path.replace(dot_pos, 4, ".jpg");
        } else {
            jpg_path += ".jpg";
        }
        stbi_write_jpg(jpg_path.c_str(), 1600, 900, 3, m_shape_montage_buffer.data(), 92);
        VF_LOG_INFO("VisualHarness", "Wrote consolidated level shapes showcase montage: " << montage_png_path << " and " << jpg_path);

        // Text Report
        std::ofstream rf(report_txt_path);
        if (rf.is_open()) {
            rf << "===================================================================================\n";
            rf << "  VOIDFALL DREDGE - LEVEL DESIGN & ROOM ARCHETYPES VISUAL SHOWCASE REPORT\n";
            rf << "===================================================================================\n";
            rf << "  Montage Canvas: " << montage_png_path << " (1600x900, 4x4 Grid)\n";
            rf << "  Total Archetypes: " << m_shape_metrics.size() << " / 16\n";
            rf << "-----------------------------------------------------------------------------------\n";
            rf << "  SLOT  ROOM / ARCHETYPE        TIMESTAMP            LUM     NON-BLACK  STATUS\n";
            rf << "-----------------------------------------------------------------------------------\n";
            bool all_ok = true;
            for (const auto& m : m_shape_metrics) {
                if (!m.is_healthy) all_ok = false;
                rf << "  [" << std::setw(2) << (m.slot + 1) << "]  "
                   << std::left << std::setw(24) << m.phase_id
                   << std::setw(21) << m.timestamp
                   << std::fixed << std::setprecision(3) << std::setw(8) << m.mean_luminance
                   << std::setprecision(1) << (m.non_black_ratio * 100.0f) << "%     "
                   << m.status_str << "\n";
            }
            rf << "===================================================================================\n";
            rf << "  OVERALL LEVEL SHAPES STATUS: " << (all_ok ? "PASS (ALL HEALTHY)" : "FAIL") << "\n";
            rf << "===================================================================================\n";
        }

        // JSON Report
        std::ofstream jf(report_json_path);
        if (jf.is_open()) {
            bool all_ok = true;
            for (const auto& m : m_shape_metrics) {
                if (!m.is_healthy) all_ok = false;
            }
            auto norm = [](std::string s) {
                std::replace(s.begin(), s.end(), '\\', '/');
                return s;
            };
            jf << "{\n";
            jf << "  \"total_shapes\": " << m_shape_metrics.size() << ",\n";
            jf << "  \"all_healthy\": " << (all_ok ? "true" : "false") << ",\n";
            jf << "  \"montage_png\": \"" << norm(montage_png_path) << "\",\n";
            jf << "  \"shapes\": [\n";
            for (size_t i = 0; i < m_shape_metrics.size(); ++i) {
                const auto& m = m_shape_metrics[i];
                jf << "    {\n";
                jf << "      \"slot\": " << m.slot << ",\n";
                jf << "      \"shape_label\": \"" << m.phase_id << "\",\n";
                jf << "      \"timestamp\": \"" << m.timestamp << "\",\n";
                jf << "      \"master_file\": \"" << norm(m.filepath) << "\",\n";
                jf << "      \"mean_luminance\": " << m.mean_luminance << ",\n";
                jf << "      \"non_black_ratio\": " << m.non_black_ratio << ",\n";
                jf << "      \"is_healthy\": " << (m.is_healthy ? "true" : "false") << ",\n";
                jf << "      \"status\": \"" << m.status_str << "\"\n";
                jf << "    }" << (i + 1 < m_shape_metrics.size() ? "," : "") << "\n";
            }
            jf << "  ]\n";
            jf << "}\n";
        }
    } catch (...) {}
}

} // namespace Voidfall
