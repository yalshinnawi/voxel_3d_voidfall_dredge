#include "core/application.hpp"
#include "core/logger.hpp"
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // If launched from outside the game folder and assets not found in CWD, switch CWD to exe dir
    try {
        if (!std::filesystem::exists("assets")) {
            if (argc > 0 && argv[0]) {
                std::filesystem::path exe_dir = std::filesystem::absolute(argv[0]).parent_path();
                if (std::filesystem::exists(exe_dir / "assets")) {
                    std::filesystem::current_path(exe_dir);
                }
            }
        }
    } catch (...) {}

    Voidfall::Logger::init("voidfall.log");
    Voidfall::Logger::setup_crash_handler();

    Voidfall::AppConfig config;
    config.is_host = true;
    config.connect_ip = "127.0.0.1";
    config.port = 27015;
    config.world_seed = 1337;
    config.player_name = "Delver_1";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "Voidfall: Dredge - Command Line Options:\n"
                      << "  --fullscreen, -f         Launch in fullscreen mode\n"
                      << "  --windowed               Launch in standard 1600x900 windowed mode\n"
                      << "  --auto-screen            Auto-detect monitor resolution & maximize (default)\n"
                      << "  --width, -w <W>          Set explicit window width\n"
                      << "  --height, -h <H>         Set explicit window height\n"
                      << "  --mute, --silent         Mute all game audio output\n"
                      << "  --auto-play-test         Run automated 10-phase visual test playthrough\n"
                      << "  --help                   Show this help message\n";
            return 0;
        } else if (arg == "--host") {
            config.is_host = true;
        } else if (arg == "--client" || arg == "--connect") {
            config.is_host = false;
            if (i + 1 < argc) {
                config.connect_ip = argv[++i];
            }
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--seed" && i + 1 < argc) {
            config.world_seed = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--name" && i + 1 < argc) {
            config.player_name = argv[++i];
        } else if (arg == "--auto-test" || arg == "--auto-play-test") {
            config.auto_play_test = true;
            config.test_mode = true;
            config.is_test_save = true;
            config.fresh_save = true;
            config.auto_screen_size = false;
            config.window_width = 1600;
            config.window_height = 900;
        } else if (arg == "--test-enemy" || arg == "--test-stalker") {
            config.test_enemy = true;
            config.test_mode = true;
            config.is_test_save = true;
            config.fresh_save = true;
            config.auto_screen_size = false;
            config.window_width = 1600;
            config.window_height = 900;
        } else if (arg == "--capture-models" || arg == "--test-models") {
            config.capture_models = true;
            config.test_mode = true;
            config.is_test_save = true;
            config.fresh_save = true;
            config.auto_screen_size = false;
            config.window_width = 1600;
            config.window_height = 900;
        } else if (arg == "--capture-level-shapes" || arg == "--capture-shapes" || arg == "--test-shapes" || arg == "--test-level-shapes") {
            config.capture_level_shapes = true;
            config.test_mode = true;
            config.is_test_save = true;
            config.fresh_save = true;
            config.auto_screen_size = false;
            config.window_width = 1600;
            config.window_height = 900;
        } else if (arg == "--test" || arg == "--test-mode") {
            config.test_mode = true;
        } else if (arg == "--fresh-save") {
            config.fresh_save = true;
        } else if (arg == "--test-save") {
            config.is_test_save = true;
            config.test_mode = true;
        } else if (arg == "--save-file" && i + 1 < argc) {
            config.save_file_path = argv[++i];
        } else if (arg == "--fullscreen" || arg == "-f") {
            config.fullscreen = true;
            config.auto_screen_size = true;
        } else if (arg == "--windowed") {
            config.fullscreen = false;
            config.auto_screen_size = false;
            config.window_width = 1600;
            config.window_height = 900;
        } else if (arg == "--auto-screen" || arg == "--auto-size") {
            config.auto_screen_size = true;
        } else if (arg == "--fixed-size") {
            config.auto_screen_size = false;
            config.window_width = 1600;
            config.window_height = 900;
        } else if ((arg == "--width" || arg == "-w") && i + 1 < argc) {
            config.window_width = std::stoi(argv[++i]);
            config.auto_screen_size = false;
        } else if ((arg == "--height" || arg == "-h") && i + 1 < argc) {
            config.window_height = std::stoi(argv[++i]);
            config.auto_screen_size = false;
        } else if (arg == "--hidden" || arg == "--headless") {
            config.hidden_window = true;
        } else if (arg == "--screenshot" && i + 1 < argc) {
            config.single_screenshot_path = argv[++i];
            config.auto_screen_size = false;
            config.window_width = 1600;
            config.window_height = 900;
        } else if (arg == "--mute" || arg == "--silent" || arg == "--no-audio" || arg == "--mute-audio") {
            config.mute_audio = true;
        } else if (arg == "--audible" || arg == "--sound" || arg == "--audio") {
            config.force_audible = true;
        }
    }

    VF_LOG_INFO("Init", "Launching Voidfall: Dredge [Role: " << (config.is_host ? "HOST" : "CLIENT")
        << ", Port: " << config.port << ", Seed: " << config.world_seed << ", Player: " << config.player_name << "]");

    int exit_code = 0;
    try {
        Voidfall::Application app(config);
        app.run();
        VF_LOG_INFO("Shutdown", "Application exited cleanly.");
    } catch (const std::exception& e) {
        VF_LOG_FATAL("Fatal", "Unhandled Exception: " << e.what());
        exit_code = 1;
    } catch (...) {
        VF_LOG_FATAL("Fatal", "Unhandled unknown exception occurred.");
        exit_code = 1;
    }

    Voidfall::Logger::shutdown();
    return exit_code;
}
