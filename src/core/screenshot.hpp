#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace Voidfall {

struct FrameVisualMetrics {
    int slot{0};
    std::string phase_id;
    std::string filepath;
    std::string preview_path;
    int width{0};
    int height{0};
    float mean_luminance{0.0f};
    float non_black_ratio{0.0f};
    bool is_healthy{true};
    std::string status_str{"PASS"};
};

// Captures a single screenshot to PNG (standard full-resolution)
bool capture_screenshot_png(const std::string& filepath, int width, int height);

// Visual Test Harness: builds an optimized 7-phase contact sheet montage, 
// lightweight previews, and visual diagnostics reports
class VisualTestHarness {
public:
    static VisualTestHarness& instance();

    void reset();
    void record_phase(int slot, const std::string& phase_label, const std::string& filepath, int width, int height);
    void finalize(const std::string& montage_png_path = "screenshots/00_all_phases_montage.png",
                  const std::string& report_txt_path = "screenshots/visual_report.txt",
                  const std::string& report_json_path = "screenshots/visual_report.json");

    const std::vector<FrameVisualMetrics>& get_metrics() const { return m_metrics; }

private:
    VisualTestHarness();

    void draw_text(std::vector<uint8_t>& buffer, int buf_w, int buf_h, int x, int y,
                   const std::string& text, uint8_t r, uint8_t g, uint8_t b);

    std::vector<FrameVisualMetrics> m_metrics;
    std::vector<uint8_t> m_montage_buffer; // 1600 x 450 RGB canvas
    static constexpr int MONTAGE_W = 1600;
    static constexpr int MONTAGE_H = 450;
    static constexpr int TILE_W = 400;
    static constexpr int TILE_H = 225;
    static constexpr int NUM_SLOTS = 8;
};

} // namespace Voidfall

