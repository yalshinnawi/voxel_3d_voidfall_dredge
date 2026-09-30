#include "core/application.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
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

    try {
        Voidfall::Application app(config);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "[Fatal Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
