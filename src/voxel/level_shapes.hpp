#pragma once
#include "chunk.hpp"
#include <vector>
#include <string>
#include <memory>
#include <array>
#include <glm/glm.hpp>
#include <cstdint>

namespace Voidfall {

// ─────────────────────────────────────────────────────────────
// ROOM / SHAPE ARCHETYPES
// ─────────────────────────────────────────────────────────────
enum class RoomShapeType {
    SpawnStagingCavern,       // Reinforced arrival bay, flat pad, clear headroom (All levels)
    MiningPillarHall,         // Grand cavern with 4 massive resource columns & climbable rungs (Level 1+)
    CrystallineGeode,         // Hollow sphere lined with emissive Voidite crystals & stepping stones (Level 1+)
    TerracedQuarry,           // Multi-tiered stepped excavation pit with ramps & ore ledges (Level 1+)
    IndustrialVaultBunker,    // Reinforced vault bunker sealed by vault blast gates & mezzanine catwalks (Level 2+)
    FaultLineCrevasse,        // Deep tectonic hazard trench with magma pockets, gas & eroded stone bridge (Level 2+)
    AbyssalVerticalChasm,     // Deep 24m vertical drop shaft with climbable spiral ledges & grapple anchors (Level 3)
    RadioactiveCoreSanctuary, // Dangerous core chamber with radioactive moat, stepping stones & Voidite monolith (Level 3)
    ExtractionLandingBay,     // Evacuation beacon landing pad with high vertical launch shaft & holdout barricades (All levels)
    MagmaCalderaLake,         // Searing volcanic chamber with molten thermite slag lake & basalt stepping stones (Level 2+)
    SpikeTrenchArena,         // Sunken arena floor lined with punji spikes & high balance beam catwalks (Level 2+)
    VoidSingularityRift,      // Bottomless cosmic void chasm with floating obsidian platforms & zero-g monolith (Level 3)
    FungoidBioGrotto,         // Subterranean bioluminescent mushroom grotto with bouncy spore platforms & gas pods (Level 1+)
    LaserDefenseFoundry,      // Precursor industrial facility with overhead catwalks, smelting flumes & security grids (Level 2+)
    CrumblingArchCanyon,      // Deep canyon spanned by fragile natural stone arches & grapple stalactites (Level 2+)

    // ── NEW EXPANDED DIVERSE ROOM ARCHETYPES ──
    SubterraneanAquiferOasis,     // Verdant underground oasis with crystal pools, cascading waterfall & glowing flora (Level 1+)
    ColossalAbyssalChasm,         // Mega vertical drop chasm with high tension bridges, punji pit & death drop (Level 2+)
    MoltenMagmaFoundry,           // Vast volcanic smelting basin with bubbling thermite channels & steam flumes (Level 2+)
    ToxicMiasmaSwamp,             // Low-lying bog filled with sinuous toxic gas pockets, visible spore clouds & fungal arches (Level 2+)
    PrismaticCrystalCathedral,    // Towering vaulted mega-hall of colossal hexagonal crystal monoliths & crystal bridges (Level 1+)
    AncientTitanNecropolis,       // Deep fossil excavation site spanned by giant prehistoric skeletal ribcage arches (Level 2+)
    BioluminescentGlowwormGrotto, // Starry-sky cavern with hundreds of bioluminescent points & reflecting pool (Level 1+)
    PrecursorCoolantReservoir,    // Precursor industrial vault with ruptured subterranean coolant pipelines & strobes (Level 2+)

    // ── VAST VERTICAL EXPANSE & COLOSSAL MEGA-SHAPES ──
    ColossalVaultedDredgeCathedral, // Towering vaulted mega-hall, soaring ribbed arches, high gantry catwalk, vast verticality (All levels)
    TectonicAbyssalSinkhole,        // Staggering tectonic sinkhole drop, high rim overlook, spiral terraces, glowing fissure bed (Level 2+)
    CyclopeanExcavationSilo,        // Titanic precursor excavation shaft, double-tiered maintenance rings, crane gantry (Level 1+)
    BioluminescentFirmamentAbyss    // Vast underground celestial cavern, starry crystal canopy at y=25, soaring stone arch bridge (All levels)
};

enum class CorridorType {
    Standard,       // Traditional axis-aligned mining artery
    Diagonal,       // Organic diagonal passage connecting diagonal rooms
    Ravine,         // Deep jagged tectonic fissure with modulated rock walls
    SlopingRamp     // Smooth stepped ramp/stairway transitioning between floors
};

struct RoomPlacement {
    RoomShapeType type{RoomShapeType::SpawnStagingCavern};
    int grid_x{0};
    int grid_z{0};
    glm::ivec3 center{0};
    int half_width{9};
    int half_depth{9};
    int floor_y{4};
    int ceiling_y{20};
    int floor_level{0};      // 0 = Lower Floor, 1 = Upper Floor, 2 = Multi-Floor (both floors)
    bool is_massive{false};  // true for massive chambers (>= 20x20)
    bool is_mega{false};     // true for very large roaming mega-caverns (>= 26x26)
    bool connected_north{false}; // +Z
    bool connected_south{false}; // -Z
    bool connected_east{false};  // +X
    bool connected_west{false};  // -X
    bool connected_ne{false};    // +X, +Z diagonal
    bool connected_nw{false};    // -X, +Z diagonal
    bool connected_se{false};    // +X, -Z diagonal
    bool connected_sw{false};    // -X, -Z diagonal
};

struct CorridorPlacement {
    glm::ivec3 start_pos{0};
    glm::ivec3 end_pos{0};
    int floor_y{4};
    int ceiling_y{14};
    int end_floor_y{4};
    int end_ceiling_y{14};
    int half_width{3};
    CorridorType type{CorridorType::Standard};
    bool is_x_axis{true};
    bool is_diagonal{false};
    bool is_ravine{false};
    bool has_bulkhead_door{false};
    bool has_rubble_collapse{false};
};

enum class LuminaryType {
    VoiditeCrystalGeode,     // Vibrant bioluminescent void violet
    RadioactiveCore,         // Searing toxic radioactive emerald
    ThermalMagmaVent,        // Molten incandescent thermite amber/orange
    IndustrialVaultBeacon,   // Industrial hazard amber/gold
    BioluminescentLedge,     // Phosphorescent cyan crystal seam
    ExtractionGuideBeacon,   // Touchdown cyan / green marker
    CorridorBulkheadLight,   // Warm low-voltage navigation light
    VoidSingularityPulse,    // Deep pulsing cosmic ultraviolet
    FungoidSporeGlow,        // Eerie organic phosphorescent green/teal
    LaserSecurityStrobe,     // Intense automated red/amber security strobe
    AquiferOasisGlow,        // Serene turquoise bioluminescent glow
    PrismaticCrystalRadiance,// Brilliant refractive prismatic glow
    ToxicMiasmaGreen,        // Murky sickly-green spore luminescence
    TitanFossilAura,         // Deep ancient amber fossil luminescence
    GrandVaultRadiance,      // Celestial radiant violet/gold high-vault glow
    FirmamentStarlight,      // Deep cosmic starlight cyan/violet
    CyclopeanFloodlight      // Industrial sodium amber beam
};

struct CavernLuminary {
    glm::vec3 position{0.0f};
    glm::vec3 base_color{1.0f};
    float base_radius{16.0f};
    float base_intensity{2.5f};
    LuminaryType type{LuminaryType::VoiditeCrystalGeode};
    float pulse_speed{2.0f};
    float pulse_depth{0.20f};
    float flicker_rate{0.0f}; // Non-zero for radioactive or hazard flicker
    std::string name{"Cavern Luminary"};

    // Evaluates real-time animated color, radius, and intensity given total elapsed time
    void evaluate(float total_time, glm::vec3& out_color, float& out_radius, float& out_intensity) const {
        out_color = base_color;
        float pulse = 1.0f + std::sin(total_time * pulse_speed) * pulse_depth;
        if (flicker_rate > 0.0f) {
            float flicker = std::sin(total_time * flicker_rate) * std::cos(total_time * (flicker_rate * 1.618f));
            pulse += flicker * 0.15f;
        }
        out_intensity = std::max(0.1f, base_intensity * pulse);
        out_radius = base_radius * (0.95f + 0.05f * pulse);
    }
};

class LevelGenerator {
public:
    static constexpr int GRID_CELL_SIZE = 24;
    static constexpr int MAX_GRID_SIZE = 5;

    // Legacy baseline constants (Sector 1)
    static constexpr int GRID_WIDTH = 3;
    static constexpr int GRID_DEPTH = 3;
    static constexpr int WORLD_WIDTH = GRID_CELL_SIZE * GRID_WIDTH; // 72 voxels
    static constexpr int WORLD_DEPTH = GRID_CELL_SIZE * GRID_DEPTH; // 72 voxels
    static constexpr int WORLD_HEIGHT = 32;                         // 32 voxels (1 chunk: 0)

    LevelGenerator(int sector_index, uint32_t seed, int vertical_chunks = 1);

    void generate_layout();
    Voxel sample_voxel(int x, int y, int z) const;
    bool is_in_bounds(int x, int y, int z) const;

    int grid_width() const { return m_grid_w; }
    int grid_depth() const { return m_grid_d; }
    int world_width() const { return m_grid_w * GRID_CELL_SIZE; }
    int world_depth() const { return m_grid_d * GRID_CELL_SIZE; }
    int world_height() const { return m_world_h; }

    const std::vector<RoomPlacement>& rooms() const { return m_rooms; }
    const std::vector<CorridorPlacement>& corridors() const { return m_corridors; }
    const std::vector<CavernLuminary>& luminaries() const { return m_luminaries; }
    const RoomPlacement* get_room_at_grid(int gx, int gz) const;
    const RoomPlacement* get_room_at_position(const glm::vec3& pos) const;

    glm::vec3 spawn_position() const;
    glm::vec3 extraction_position() const;

    int sector_index() const { return m_sector_index; }
    uint32_t seed() const { return m_seed; }

    void create_single_room_test_layout(RoomShapeType type);
    void create_corridor_test_layout();
    void set_room_type(int gx, int gz, RoomShapeType type);

private:
    uint32_t hash_coord(int x, int y, int z, uint32_t salt = 0) const;
    float pseudo_rand(int x, int y, int z, uint32_t salt = 0) const;

    // Shape-specific carving methods
    void sample_spawn_cavern(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_pillar_hall(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_crystalline_geode(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_terraced_quarry(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_vault_bunker(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_fault_crevasse(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_abyssal_chasm(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_radioactive_sanctuary(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_extraction_bay(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;

    // Expanded room archetypes
    void sample_magma_caldera(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_spike_trench(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_void_singularity(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_fungoid_grotto(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_laser_foundry(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_crumbling_canyon(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;

    // New 8 deep/oasis/hazard archetypes
    void sample_aquifer_oasis(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_colossal_abyssal_chasm(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_molten_magma_foundry(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_toxic_miasma_swamp(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_prismatic_crystal_cathedral(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_ancient_titan_necropolis(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_bioluminescent_glowworm_grotto(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_precursor_coolant_reservoir(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;

    // Vast vertical expanse & colossal mega-shapes
    void sample_vaulted_dredge_cathedral(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_tectonic_sinkhole(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_cyclopean_silo(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_firmament_abyss(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;

    void generate_luminaries();

    int m_sector_index{1};
    uint32_t m_seed{1337};
    int m_grid_w{3};
    int m_grid_d{3};
    int m_world_h{WORLD_HEIGHT};
    std::vector<RoomPlacement> m_rooms;
    std::vector<CorridorPlacement> m_corridors;
    std::vector<CavernLuminary> m_luminaries;
    std::array<std::array<int, MAX_GRID_SIZE>, MAX_GRID_SIZE> m_grid_room_indices;
};

} // namespace Voidfall
