#include "net_client.hpp"
#include "../core/logger.hpp"
#include <iostream>

namespace Voidfall {

NetClient::NetClient() {
}

NetClient::~NetClient() {
    disconnect();
}

bool NetClient::connect_to_host(const std::string& host_ip, uint16_t port, const std::string& player_name) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        VF_LOG_ERROR("NetClient", "WSAStartup failed with error code: " << WSAGetLastError());
        return false;
    }
#endif

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    if (m_socket == INVALID_SOCKET) {
        VF_LOG_ERROR("NetClient", "Failed to create UDP socket: " << WSAGetLastError());
        WSACleanup();
        return false;
    }
    u_long non_blocking = 1;
    ioctlsocket(m_socket, FIONBIO, &non_blocking);
#else
    if (m_socket < 0) {
        VF_LOG_ERROR("NetClient", "Failed to create UDP socket");
        return false;
    }
    fcntl(m_socket, F_SETFL, O_NONBLOCK);
#endif

    m_server_addr.sin_family = AF_INET;
    m_server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host_ip.c_str(), &m_server_addr.sin_addr);

    HandshakeRequestPacket req;
    req.protocol_version = 1;
    snprintf(req.player_name, sizeof(req.player_name), "%s", player_name.c_str());

    send_packet(PacketType::HandshakeRequest, &req, sizeof(req));
    m_connected = true;
    VF_LOG_INFO("NetClient", "Sent Handshake to " << host_ip << ":" << port);
    return true;
}

void NetClient::disconnect() {
    if (!m_connected) return;
    m_connected = false;

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
    std::cout << "[NetClient] Disconnected" << std::endl;
}

void NetClient::send_packet(PacketType type, const void* data, size_t size) {
    if (!m_connected) return;

    ByteStreamWriter writer;
    PacketHeader hdr;
    hdr.magic = 0x5644;
    hdr.type = type;
    writer.write(hdr);
    if (data && size > 0) {
        writer.write_bytes(data, size);
    }

    sendto(m_socket, reinterpret_cast<const char*>(writer.data()), static_cast<int>(writer.size()), 0,
           reinterpret_cast<const sockaddr*>(&m_server_addr), sizeof(m_server_addr));
}

void NetClient::send_input(const PlayerInputPacket& input) {
    send_packet(PacketType::PlayerInput, &input, sizeof(input));
}

void NetClient::send_block_delta(const BlockDeltaPacket& delta) {
    send_packet(PacketType::BlockDelta, &delta, sizeof(delta));
}

void NetClient::send_sonar_ping(const glm::vec3& origin) {
    SonarPingPacket pkt;
    pkt.caster_id = m_player_id;
    pkt.origin = origin;
    pkt.max_radius = 65.0f;
    send_packet(PacketType::SonarPingBroadcast, &pkt, sizeof(pkt));
}

void NetClient::poll_network_events() {
    if (!m_connected) return;

    uint8_t buffer[2048];
    sockaddr_in from_addr{};
#ifdef _WIN32
    int addr_len = sizeof(from_addr);
#else
    socklen_t addr_len = sizeof(from_addr);
#endif

    while (true) {
        int bytes_read = recvfrom(m_socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                  reinterpret_cast<sockaddr*>(&from_addr), &addr_len);
        if (bytes_read < static_cast<int>(sizeof(PacketHeader))) {
            break;
        }

        ByteStreamReader reader(buffer, bytes_read);
        PacketHeader hdr;
        if (!reader.read(hdr)) continue;
        if (hdr.magic != 0x5644) continue;

        switch (hdr.type) {
            case PacketType::HandshakeResponse: {
                HandshakeResponsePacket resp;
                if (reader.read(resp)) {
                    m_player_id = resp.assigned_player_id;
                    m_world_seed = resp.world_seed;
                    std::cout << "[NetClient] Connected to " << resp.server_name
                              << " with Player ID: " << m_player_id
                              << " (Seed: " << m_world_seed << ")" << std::endl;
                    if (m_on_seed) {
                        m_on_seed(m_world_seed);
                    }
                }
                break;
            }

            case PacketType::BlockDelta: {
                BlockDeltaPacket delta;
                if (reader.read(delta)) {
                    if (m_on_block_delta) {
                        m_on_block_delta(delta);
                    }
                }
                break;
            }

            case PacketType::DynamicDebrisSpawn: {
                DynamicDebrisSpawnPacket debris;
                if (reader.read(debris)) {
                    if (m_on_debris_spawn) {
                        m_on_debris_spawn(debris);
                    }
                }
                break;
            }

            case PacketType::SonarPingBroadcast: {
                SonarPingPacket ping;
                if (reader.read(ping)) {
                    if (m_on_sonar_ping) {
                        m_on_sonar_ping(ping);
                    }
                }
                break;
            }

            default:
                break;
        }
    }
}

} // namespace Voidfall
