#pragma once
#include "packet_types.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace Voidfall {

struct ConnectedPeer {
    uint32_t player_id{0};
    sockaddr_in address;
    std::string name{"Delver"};
    uint32_t last_packet_time{0};
    PlayerStateSnapshot latest_state;
};

class NetHost {
public:
    explicit NetHost(uint16_t port = 27015);
    ~NetHost();

    NetHost(const NetHost&) = delete;
    NetHost& operator=(const NetHost&) = delete;

    bool start();
    void stop();
    bool is_running() const { return m_running; }

    void poll_network_events();

    void broadcast_packet(PacketType type, const void* data, size_t size);
    void send_packet_to(const sockaddr_in& addr, PacketType type, const void* data, size_t size);

    void broadcast_block_delta(int32_t cx, int32_t cy, int32_t cz, uint16_t block_idx, uint8_t mat, uint8_t flags);
    void broadcast_debris_spawn(uint32_t debris_id, const glm::vec3& origin, const glm::vec3& vel, uint8_t mat, size_t count);
    void broadcast_sonar_ping(uint32_t caster_id, const glm::vec3& origin, float radius);

    const std::unordered_map<uint32_t, ConnectedPeer>& peers() const { return m_peers; }

    using BlockDeltaCallback = std::function<void(const BlockDeltaPacket&)>;
    void set_on_block_delta(BlockDeltaCallback cb) { m_on_block_delta = std::move(cb); }

private:
    uint16_t m_port{27015};
    bool m_running{false};

#ifdef _WIN32
    SOCKET m_socket{INVALID_SOCKET};
#else
    int m_socket{-1};
#endif

    uint32_t m_next_player_id{1};
    std::unordered_map<uint32_t, ConnectedPeer> m_peers;
    std::mutex m_peer_mutex;

    BlockDeltaCallback m_on_block_delta;
};

} // namespace Voidfall
