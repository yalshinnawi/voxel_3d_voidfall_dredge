#pragma once
#include "packet_types.hpp"
#include <string>
#include <functional>
#include <glm/glm.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace Voidfall {

class NetClient {
public:
    NetClient();
    ~NetClient();

    NetClient(const NetClient&) = delete;
    NetClient& operator=(const NetClient&) = delete;

    bool connect_to_host(const std::string& host_ip, uint16_t port = 27015, const std::string& player_name = "Delver");
    void disconnect();
    bool is_connected() const { return m_connected; }

    void send_input(const PlayerInputPacket& input);
    void send_block_delta(const BlockDeltaPacket& delta);
    void send_sonar_ping(const glm::vec3& origin);

    void poll_network_events();

    uint32_t player_id() const { return m_player_id; }
    uint32_t world_seed() const { return m_world_seed; }

    using SeedCallback = std::function<void(uint32_t)>;
    using BlockDeltaCallback = std::function<void(const BlockDeltaPacket&)>;
    using DebrisSpawnCallback = std::function<void(const DynamicDebrisSpawnPacket&)>;
    using SonarPingCallback = std::function<void(const SonarPingPacket&)>;

    void set_on_seed_received(SeedCallback cb) { m_on_seed = std::move(cb); }
    void set_on_block_delta(BlockDeltaCallback cb) { m_on_block_delta = std::move(cb); }
    void set_on_debris_spawn(DebrisSpawnCallback cb) { m_on_debris_spawn = std::move(cb); }
    void set_on_sonar_ping(SonarPingCallback cb) { m_on_sonar_ping = std::move(cb); }

private:
    void send_packet(PacketType type, const void* data, size_t size);

#ifdef _WIN32
    SOCKET m_socket{INVALID_SOCKET};
#else
    int m_socket{-1};
#endif
    sockaddr_in m_server_addr{};
    bool m_connected{false};
    uint32_t m_player_id{0};
    uint32_t m_world_seed{1337};

    SeedCallback m_on_seed;
    BlockDeltaCallback m_on_block_delta;
    DebrisSpawnCallback m_on_debris_spawn;
    SonarPingCallback m_on_sonar_ping;
};

} // namespace Voidfall
