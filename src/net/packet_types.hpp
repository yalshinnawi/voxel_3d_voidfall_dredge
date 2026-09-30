#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <glm/glm.hpp>

namespace Voidfall {

enum class PacketType : uint8_t {
    None = 0,
    HandshakeRequest = 1,
    HandshakeResponse = 2,
    WorldSeedSync = 3,
    PlayerInput = 4,
    PlayerStateSnapshot = 5,
    BlockDelta = 6,
    DynamicDebrisSpawn = 7,
    DynamicDebrisSync = 8,
    SonarPingBroadcast = 9,
    SkillCast = 10,
    ExtractionState = 11,
    HazardClockSync = 12,
    ChatMessage = 13
};

#pragma pack(push, 1)
struct PacketHeader {
    uint16_t magic{0x5644}; // "VD" (Voidfall Dredge)
    PacketType type{PacketType::None};
    uint8_t flags{0};       // bit 0: reliable, bit 1: compressed
    uint32_t sequence{0};
    uint32_t ack{0};
};

struct HandshakeRequestPacket {
    uint32_t protocol_version{1};
    char player_name[32]{0};
};

struct HandshakeResponsePacket {
    uint32_t assigned_player_id{0};
    uint32_t world_seed{0};
    uint32_t server_tick_rate{60};
    char server_name[32]{0};
};

struct WorldSeedSyncPacket {
    uint32_t seed{1337};
    int32_t spawn_chunk_x{0};
    int32_t spawn_chunk_y{0};
    int32_t spawn_chunk_z{0};
};

// Player Input flags
enum PlayerButtonFlags : uint16_t {
    BTN_FORWARD      = 1 << 0,
    BTN_BACKWARD     = 1 << 1,
    BTN_LEFT         = 1 << 2,
    BTN_RIGHT        = 1 << 3,
    BTN_JUMP         = 1 << 4,
    BTN_THRUSTER     = 1 << 5, // Exo-Suit vertical / glide thruster
    BTN_GRAPPLE_FIRE = 1 << 6, // Fire tension cable grapple
    BTN_GRAPPLE_REEL = 1 << 7, // Reel grapple in
    BTN_MINE_DRILL   = 1 << 8, // Subterranean mining drill
    BTN_PLACE_BLOCK  = 1 << 9, // Structural reinforcement placement
    BTN_SKILL_DEMO   = 1 << 10, // Demolitions shaped charge / thermite
    BTN_SKILL_SONAR  = 1 << 11, // Seismic Sonar pulse scan
    BTN_SPRINT       = 1 << 12
};

struct PlayerInputPacket {
    uint32_t client_tick{0};
    uint32_t sequence_id{0};
    glm::vec3 move_dir{0.0f};
    glm::vec2 look_angles{0.0f}; // pitch, yaw
    uint16_t buttons{0};
    float delta_time{0.0f};
};

struct PlayerStateSnapshot {
    uint32_t player_id{0};
    uint32_t server_tick{0};
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec2 look_angles{0.0f};
    uint16_t buttons_state{0};
    float suit_power{100.0f};
    float suit_heat{0.0f};
    float suit_integrity{100.0f};
    uint8_t grapple_active{0};
    glm::vec3 grapple_point{0.0f};
};

// Delta compressed block modification
struct BlockDeltaPacket {
    int32_t chunk_x{0};
    int32_t chunk_y{0};
    int32_t chunk_z{0};
    uint16_t local_block_idx{0}; // 0..32767
    uint8_t material_id{0};
    uint8_t flags_and_damage{0};
    uint32_t server_tick{0};
};

// Spawns unanchored falling voxel cluster
struct DynamicDebrisSpawnPacket {
    uint32_t debris_id{0};
    glm::vec3 origin{0.0f};
    glm::vec3 linear_velocity{0.0f};
    glm::vec3 angular_velocity{0.0f};
    uint16_t block_count{0};
    uint8_t material_id{0};
};

// Synchronized seismic sonar scan for Surveying squad synergy
struct SonarPingPacket {
    uint32_t caster_id{0};
    glm::vec3 origin{0.0f};
    float max_radius{60.0f};
    float expansion_speed{25.0f};
    uint32_t timestamp_ms{0};
};

// Skill activation (Demolitions, Surveying, Exo-Suit, Acrobatics)
struct SkillCastPacket {
    uint32_t caster_id{0};
    uint8_t skill_tree{0}; // 0: Demolitions, 1: Surveying, 2: Exo-Suit, 3: Acrobatics
    uint8_t skill_id{0};
    glm::vec3 target_pos{0.0f};
    glm::vec3 normal{0.0f};
};

// Synchronized Hazard Clock & 90s Extraction Beacon state
struct HazardClockPacket {
    float radiation_level{0.0f};      // 0..100%
    float cave_tremor_timer{0.0f};    // Seconds to next seismic tremor
    float extraction_countdown{90.0f}; // 90s defense countdown
    uint8_t extraction_active{0};     // 0: Dormant, 1: Beacon Active, 2: Pod Landed, 3: Extracted
    uint32_t wave_counter{0};
};
#pragma pack(pop)

// Lightweight packet buffer reader and writer
class ByteStreamWriter {
public:
    template<typename T>
    void write(const T& val) {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&val);
        m_buffer.insert(m_buffer.end(), ptr, ptr + sizeof(T));
    }

    void write_bytes(const void* data, size_t length) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);
        m_buffer.insert(m_buffer.end(), ptr, ptr + length);
    }

    const std::vector<uint8_t>& buffer() const { return m_buffer; }
    std::vector<uint8_t>& buffer() { return m_buffer; }
    size_t size() const { return m_buffer.size(); }
    const uint8_t* data() const { return m_buffer.data(); }

private:
    std::vector<uint8_t> m_buffer;
};

class ByteStreamReader {
public:
    ByteStreamReader(const uint8_t* data, size_t size)
        : m_data(data), m_size(size), m_offset(0) {}

    template<typename T>
    bool read(T& out_val) {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        if (m_offset + sizeof(T) > m_size) {
            return false;
        }
        std::memcpy(&out_val, m_data + m_offset, sizeof(T));
        m_offset += sizeof(T);
        return true;
    }

    bool read_bytes(void* out_data, size_t length) {
        if (m_offset + length > m_size) {
            return false;
        }
        std::memcpy(out_data, m_data + m_offset, length);
        m_offset += length;
        return true;
    }

    size_t remaining() const { return (m_offset < m_size) ? (m_size - m_offset) : 0; }
    size_t offset() const { return m_offset; }

private:
    const uint8_t* m_data{nullptr};
    size_t m_size{0};
    size_t m_offset{0};
};

} // namespace Voidfall
