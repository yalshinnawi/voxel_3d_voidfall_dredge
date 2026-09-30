#include "net_host.hpp"
#include <iostream>

namespace Voidfall {

NetHost::NetHost(uint16_t port)
    : m_port(port)
{
}

NetHost::~NetHost() {
    stop();
}

bool NetHost::start() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[NetHost] WSAStartup failed" << std::endl;
        return false;
    }
#endif

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "[NetHost] Failed to create socket" << std::endl;
        WSACleanup();
        return false;
    }
    u_long non_blocking = 1;
    ioctlsocket(m_socket, FIONBIO, &non_blocking);
#else
    if (m_socket < 0) {
        std::cerr << "[NetHost] Failed to create socket" << std::endl;
        return false;
    }
    fcntl(m_socket, F_SETFL, O_NONBLOCK);
#endif

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(m_port);

    if (bind(m_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "[NetHost] Failed to bind socket to port " << m_port << std::endl;
        stop();
        return false;
    }

    m_running = true;
    std::cout << "[NetHost] Server listening on UDP port " << m_port << std::endl;
    return true;
}

void NetHost::stop() {
    if (!m_running) return;
    m_running = false;

#ifdef _WIN32
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    WSACleanup();
#else
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }
#endif
    std::cout << "[NetHost] Server stopped" << std::endl;
}

void NetHost::send_packet_to(const sockaddr_in& addr, PacketType type, const void* data, size_t size) {
    if (!m_running) return;

    ByteStreamWriter writer;
    PacketHeader hdr;
    hdr.magic = 0x5644;
    hdr.type = type;
    writer.write(hdr);
    if (data && size > 0) {
        writer.write_bytes(data, size);
    }

    sendto(m_socket, reinterpret_cast<const char*>(writer.data()), static_cast<int>(writer.size()), 0,
           reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
}

void NetHost::broadcast_packet(PacketType type, const void* data, size_t size) {
    std::lock_guard<std::mutex> lock(m_peer_mutex);
    for (const auto& [id, peer] : m_peers) {
        send_packet_to(peer.address, type, data, size);
    }
}

void NetHost::broadcast_block_delta(int32_t cx, int32_t cy, int32_t cz, uint16_t block_idx, uint8_t mat, uint8_t flags) {
    BlockDeltaPacket pkt;
    pkt.chunk_x = cx;
    pkt.chunk_y = cy;
    pkt.chunk_z = cz;
    pkt.local_block_idx = block_idx;
    pkt.material_id = mat;
    pkt.flags_and_damage = flags;
    broadcast_packet(PacketType::BlockDelta, &pkt, sizeof(pkt));
}

void NetHost::broadcast_debris_spawn(uint32_t debris_id, const glm::vec3& origin, const glm::vec3& vel, uint8_t mat, size_t count) {
    DynamicDebrisSpawnPacket pkt;
    pkt.debris_id = debris_id;
    pkt.origin = origin;
    pkt.linear_velocity = vel;
    pkt.material_id = mat;
    pkt.block_count = static_cast<uint16_t>(count);
    broadcast_packet(PacketType::DynamicDebrisSpawn, &pkt, sizeof(pkt));
}

void NetHost::broadcast_sonar_ping(uint32_t caster_id, const glm::vec3& origin, float radius) {
    SonarPingPacket pkt;
    pkt.caster_id = caster_id;
    pkt.origin = origin;
    pkt.max_radius = radius;
    broadcast_packet(PacketType::SonarPingBroadcast, &pkt, sizeof(pkt));
}

void NetHost::poll_network_events() {
    if (!m_running) return;

    uint8_t buffer[2048];
    sockaddr_in client_addr{};
#ifdef _WIN32
    int addr_len = sizeof(client_addr);
#else
    socklen_t addr_len = sizeof(client_addr);
#endif

    while (true) {
        int bytes_read = recvfrom(m_socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                  reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        if (bytes_read < static_cast<int>(sizeof(PacketHeader))) {
            break;
        }

        ByteStreamReader reader(buffer, bytes_read);
        PacketHeader hdr;
        if (!reader.read(hdr)) continue;
        if (hdr.magic != 0x5644) continue;

        switch (hdr.type) {
            case PacketType::HandshakeRequest: {
                HandshakeRequestPacket req;
                if (reader.read(req)) {
                    std::lock_guard<std::mutex> lock(m_peer_mutex);
                    uint32_t pid = m_next_player_id++;
                    ConnectedPeer peer;
                    peer.player_id = pid;
                    peer.address = client_addr;
                    peer.name = req.player_name;
                    m_peers[pid] = peer;

                    std::cout << "[NetHost] Peer connected: " << peer.name << " (ID: " << pid << ")" << std::endl;

                    HandshakeResponsePacket resp;
                    resp.assigned_player_id = pid;
                    resp.world_seed = 1337;
                    resp.server_tick_rate = 60;
                    snprintf(resp.server_name, sizeof(resp.server_name), "Voidfall Host");
                    send_packet_to(client_addr, PacketType::HandshakeResponse, &resp, sizeof(resp));
                }
                break;
            }

            case PacketType::BlockDelta: {
                BlockDeltaPacket delta;
                if (reader.read(delta)) {
                    // Re-broadcast to all other peers
                    std::lock_guard<std::mutex> lock(m_peer_mutex);
                    for (const auto& [id, peer] : m_peers) {
                        send_packet_to(peer.address, PacketType::BlockDelta, &delta, sizeof(delta));
                    }
                    if (m_on_block_delta) {
                        m_on_block_delta(delta);
                    }
                }
                break;
            }

            case PacketType::SonarPingBroadcast: {
                SonarPingPacket ping;
                if (reader.read(ping)) {
                    broadcast_packet(PacketType::SonarPingBroadcast, &ping, sizeof(ping));
                }
                break;
            }

            default:
                break;
        }
    }
}

} // namespace Voidfall
