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
    std::string timestamp;
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
    void cleanup_previews(const std::string& preview_dir = "screenshots/previews");

    // Enemy Stalker Dedicated Visual Test Suite
    void record_enemy_phase(int slot, const std::string& phase_label, const std::string& filepath, int width, int height);
    void finalize_enemy_test(const std::string& montage_png_path = "screenshots/enemy_visual_montage.png",
                             const std::string& report_txt_path = "screenshots/enemy_visual_report.txt",
                             const std::string& report_json_path = "screenshots/enemy_visual_report.json");

    // Model Showcase Dedicated Visual Suite
    void record_model_phase(int slot, const std::string& model_label, const std::string& filepath, int width, int height);
    void finalize_model_showcase(const std::string& montage_png_path = "docs/models/models_roster_showcase.png",
                                 const std::string& report_txt_path = "docs/models/models_visual_report.txt",
                                 const std::string& report_json_path = "docs/models/models_visual_report.json");

    // Level Shapes Dedicated Visual Suite (16 slots: 4x4 grid)
    void record_shape_phase(int slot, const std::string& shape_label, const std::string& filepath, int width, int height);
    void finalize_shapes_showcase(const std::string& montage_png_path = "docs/level_design/level_shapes_showcase.png",
                                  const std::string& report_txt_path = "docs/level_design/level_shapes_report.txt",
                                  const std::string& report_json_path = "docs/level_design/level_shapes_report.json");

    const std::vector<FrameVisualMetrics>& get_metrics() const { return m_metrics; }
    const std::vector<FrameVisualMetrics>& get_enemy_metrics() const { return m_enemy_metrics; }
    const std::vector<FrameVisualMetrics>& get_model_metrics() const { return m_model_metrics; }
    const std::vector<FrameVisualMetrics>& get_shape_metrics() const { return m_shape_metrics; }

private:
    VisualTestHarness();

    void draw_text(std::vector<uint8_t>& buffer, int buf_w, int buf_h, int x, int y,
                   const std::string& text, uint8_t r, uint8_t g, uint8_t b);

    std::vector<FrameVisualMetrics> m_metrics;
    std::vector<uint8_t> m_montage_buffer; // 1600 x 675 RGB canvas (4x3 grid)
    static constexpr int MONTAGE_W = 1600;
    static constexpr int MONTAGE_H = 675;
    static constexpr int TILE_W = 400;
    static constexpr int TILE_H = 225;
    static constexpr int NUM_SLOTS = 12;

    std::vector<FrameVisualMetrics> m_enemy_metrics;
    std::vector<uint8_t> m_enemy_montage_buffer; // 1200 x 450 RGB canvas (3x2 grid)

    std::vector<FrameVisualMetrics> m_model_metrics;
    std::vector<uint8_t> m_model_montage_buffer; // 1200 x 450 RGB canvas (3x2 grid)

    std::vector<FrameVisualMetrics> m_shape_metrics;
    std::vector<uint8_t> m_shape_montage_buffer; // 1600 x 900 RGB canvas (4x4 grid: 16 tiles)
};

} // namespace Voidfall

