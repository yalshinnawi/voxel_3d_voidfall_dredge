#include "core/application.hpp"
#include "core/logger.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
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
        if (arg == "--host") {
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
