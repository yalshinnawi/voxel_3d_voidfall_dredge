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

    FrameVisualMetrics metric;
    metric.slot = slot;
    metric.phase_id = phase_label;
    metric.filepath = filepath;
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

    // 5. Draw Semi-Transparent Banner & Phase Title Label
    for (int by = 0; by < 18; ++by) {
        for (int bx = 0; bx < TILE_W; ++bx) {
            int out_x = tile_x + bx;
            int out_y = tile_y + by;
            size_t out_idx = static_cast<size_t>(out_y * MONTAGE_W + out_x) * 3;
            // Blend 25% original + 75% dark navy banner
            m_montage_buffer[out_idx + 0] = static_cast<uint8_t>((m_montage_buffer[out_idx + 0] + 16 * 3) / 4);
            m_montage_buffer[out_idx + 1] = static_cast<uint8_t>((m_montage_buffer[out_idx + 1] + 22 * 3) / 4);
            m_montage_buffer[out_idx + 2] = static_cast<uint8_t>((m_montage_buffer[out_idx + 2] + 32 * 3) / 4);
        }
    }

    std::string banner_text = "[" + std::to_string(slot + 1) + "] " + phase_label;
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, tile_x + 8, tile_y + 5, banner_text, 100, 220, 255);

    // 6. Generate Fast Lightweight 2x Preview JPEG (e.g. 800x450, ~45 KB)
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

    try {
        std::filesystem::path p(filepath);
        std::filesystem::path preview_dir = p.parent_path() / "previews";
        std::filesystem::create_directories(preview_dir);
        std::string preview_file = (preview_dir / (p.stem().string() + ".jpg")).generic_string();
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
    // 1. Draw Slot 7: Integrated Visual Health Matrix Card
    int diag_x = 3 * TILE_W;
    int diag_y = 1 * TILE_H;

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
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 8, "[08] VISUAL HEALTH MATRIX", 0, 230, 255);
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 24, "PHASE              LUM    STATUS", 150, 160, 180);
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 34, "--------------------------------", 70, 80, 100);

    bool all_healthy = true;
    for (size_t i = 0; i < m_metrics.size() && i < 7; ++i) {
        const auto& m = m_metrics[i];
        if (!m.is_healthy) all_healthy = false;

        std::string name_col = m.phase_id;
        if (name_col.size() > 16) name_col = name_col.substr(0, 16);
        while (name_col.size() < 16) name_col += " ";

        std::ostringstream lum_ss;
        lum_ss << std::fixed << std::setprecision(2) << m.mean_luminance;
        std::string lum_str = lum_ss.str();
        while (lum_str.size() < 6) lum_str += " ";

        std::string row_str = "P" + std::to_string(m.slot + 1) + " " + name_col + lum_str + " " + m.status_str;
        int row_y = diag_y + 48 + static_cast<int>(i) * 16;

        uint8_t r = m.is_healthy ? (m.status_str == "PASS" ? 80 : 255) : 255;
        uint8_t g = m.is_healthy ? (m.status_str == "PASS" ? 240 : 200) : 60;
        uint8_t b = m.is_healthy ? (m.status_str == "PASS" ? 120 : 50) : 60;

        draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, row_y, row_str, r, g, b);
    }

    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 172, "--------------------------------", 70, 80, 100);
    if (all_healthy) {
        draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 186, "OVERALL: 7/7 PHASES HEALTHY [PASS]", 80, 255, 120);
    } else {
        draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 186, "OVERALL: VISUAL ANOMALIES DETECTED", 255, 80, 80);
    }
    draw_text(m_montage_buffer, MONTAGE_W, MONTAGE_H, diag_x + 10, diag_y + 204, "TILES: 400x225 (16:9) | GRID: 4x2", 100, 180, 220);

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

} // namespace Voidfall
