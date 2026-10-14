#pragma once
#include "chunk.hpp"
#include "material_scarcity.hpp"
#include <vector>
#include <string>
#include <memory>
#include <array>
#include <random>
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
    BioluminescentFirmamentAbyss,   // Vast underground celestial cavern, starry crystal canopy at y=25, soaring stone arch bridge (All levels)

    // ── DEDICATED PRECURSOR VAULT ROOMS & SANCTUARIES ──
    PrecursorSanctumVault,          // Grand precursor sanctum with vaulted archways, runic dais & sealed relic portal (Level 1+)
    SubterraneanVaultBastion,       // Fortified precursor military bastion with observation catwalks, slag trench & blast gate portal (Level 2+)
    AbyssalVaultChasm               // Deep tectonic vault chasm with lethal drops, molten/radiation moat & boss roost (Level 3)
};

// ─────────────────────────────────────────────────────────────
// ROOM CATEGORIES & THEMATIC CLUSTERING
// ─────────────────────────────────────────────────────────────
enum class RoomCategory : uint8_t {
    SafeTerminal = 0,       // Spawn Staging, Extraction Landing
    NaturalCrystalline,     // Mining Pillar Hall, Crystalline Geode, Prismatic Cathedral, Aquifer Oasis, Firmament Abyss, Glowworm Grotto
    BioOrganic,             // Fungoid Bio-Grotto, Toxic Miasma Swamp
    PrecursorIndustrial,    // Industrial Vault Bunker, Laser Defense Foundry, Coolant Reservoir, Excavation Silo, Terraced Quarry, Sanctum Vault, Vault Bastion
    VolcanicThermal,        // Magma Caldera Lake, Molten Magma Foundry
    TectonicChasm,          // Fault-Line Crevasse, Crumbling Arch Canyon, Spike Trench Arena, Ancient Titan Necropolis, Tectonic Sinkhole
    CosmicVoidAbyss         // Abyssal Vertical Chasm, Colossal Abyssal Chasm, Void Singularity Rift, Radioactive Core Sanctuary, Vaulted Cathedral, Abyssal Vault Chasm
};

RoomCategory get_room_category(RoomShapeType type);
const char* get_room_category_name(RoomCategory cat);
bool is_harsh_hazard_room(RoomShapeType type);
bool is_extreme_spawn_hazard(RoomShapeType type);
bool is_extraction_climax_room(RoomShapeType type);
bool is_colossal_or_massive_room(RoomShapeType type);
float get_room_adjacency_affinity(RoomShapeType typeA, RoomShapeType typeB);

enum class CorridorType {
    Standard,       // Traditional axis-aligned mining artery
    Diagonal,       // Organic diagonal passage connecting diagonal rooms
    Ravine,         // Deep jagged tectonic fissure with modulated rock walls
    SlopingRamp     // Smooth stepped ramp/stairway transitioning between floors
};

enum class FluidStructureType : uint8_t {
    ContainedPool = 0,       // Natural enclosed circular/oval basin with decorative rim
    SteppingStonePool = 1,   // Grotto pool with central island and hop-across stepping stones
    WallCascade = 2,         // Small fall / wall niche cascade pouring down into splash basin
    CascadingTwinPools = 3,  // Tiered double terrace pools with weir overflow step
    HazardTrench = 4,        // Dangerous room obstacle trench with crossing stepping stones
    DropChasmSump = 5        // Plunge pool / hazard sump at the bottom of large drop shafts
};

// ─────────────────────────────────────────────────────────────
// VAULT PLACEMENT & LEVEL-SCALED OBSTACLE CONFIGURATION
// ─────────────────────────────────────────────────────────────
enum class VaultObstacleType : uint8_t {
    None = 0,
    AquiferSteppingStones,  // Level 1: Scenic aquifer moat with stepping stones & mild sentry
    HazardTrenchSlag,       // Level 2: Boiling molten slag / corrosive acid trench with basalt steps & hardened sentries
    ChasmAbyssRadiation    // Level 3+: Deep lethal drop chasm with radiation field & Chitin Goliath boss guardian
};

enum class VaultGuardianRole : uint8_t {
    None = 0,
    StalkerMelee,
    StalkerShooter,
    ChitinGoliath           // Boss Monster for deep/late sectors
};

struct VaultPlacement {
    bool exists{false};
    int room_index{-1};
    RoomShapeType room_type{RoomShapeType::PrecursorSanctumVault};
    glm::ivec3 chamber_center{0};
    glm::ivec3 chamber_min{0};
    glm::ivec3 chamber_max{0};
    glm::ivec3 door_pos{0};
    glm::ivec3 relic_pos{0};
    int door_height{3};
    glm::ivec3 approach_pos{0};
    VaultObstacleType obstacle_type{VaultObstacleType::None};
    bool has_guardian{false};
    bool is_boss_guardian{false};
    VaultGuardianRole guardian_role{VaultGuardianRole::None};
    glm::vec3 guardian_spawn_pos{0.0f};
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
    bool has_random_fluid{false};
    FluidStructureType fluid_structure_type{FluidStructureType::ContainedPool};
    uint8_t random_fluid_material{MAT_CRYSTAL_AQUIFER};
    uint8_t random_fluid_rim_material{MAT_BIOLUMINESCENT_FLORA};
    float random_fluid_offset_x{0.0f};
    float random_fluid_offset_z{0.0f};
    float random_fluid_radius{2.0f};
    float random_fluid_length{5.0f};
    int fluid_orientation{0};
    int objective_deposit_count{0};
    int spawned_objective_voxels{0};
    bool has_vault{false};
    VaultPlacement vault;
    bool is_dark_zone{false};        // True if room is an unlit / blackout cavern (no luminaries)
    float darkness_factor{0.0f};     // Darkness intensity: 0.0 (normal ambient) to 1.0 (pitch-black void)

    int min_x() const { return center.x - half_width; }
    int max_x() const { return center.x + half_width; }
    int min_z() const { return center.z - half_depth; }
    int max_z() const { return center.z + half_depth; }
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

    LevelGenerator(int sector_index, uint32_t seed, int vertical_chunks = 1, int target_objective = -1);

    void generate_layout();
    Voxel sample_voxel(int x, int y, int z, bool* out_is_vein = nullptr) const;
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

    // ── Precursor Vault Placement Accessors ──
    const VaultPlacement& vault_placement() const { return m_vault_placement; }
    VaultPlacement& vault_placement_mut() { return m_vault_placement; }
    bool has_vault() const { return m_vault_placement.exists; }

    glm::vec3 spawn_position() const;
    glm::vec3 extraction_position() const;

    int sector_index() const { return m_sector_index; }
    uint32_t seed() const { return m_seed; }

    // ── Dark Zone & Illumination Queries ──
    bool is_dark_zone_at(const glm::vec3& pos) const;
    float get_darkness_at(const glm::vec3& pos) const;
    int dark_room_count() const;
    float sector_darkness_ratio() const;

struct RoomValidationReport {
    bool valid{true};
    int spawned_voxels{0};
    int deposit_count{0};
    bool is_near_spawn{false};
    bool exceeds_capacity{false};
    std::string details;
};

    // ── Objective Resource Scarcity, Balancing & Completeness Validation ──
    int target_objective_amount() const { return m_target_objective; }
    int total_objective_material_count() const { return static_cast<int>(m_objective_positions.size()); }
    int objective_room_spread_count() const;
    int rooms_with_zero_objective_count() const;
    bool validate_resource_compleatability() const;
    bool validate_level_resources() const;
    RoomValidationReport validate_room_resources(const RoomPlacement& room) const;
    bool is_objective_voxel(int x, int y, int z) const;
    const std::vector<glm::ivec3>& objective_deposit_locations() const { return m_objective_positions; }
    std::vector<glm::ivec3> get_room_candidate_spawn_offsets(RoomShapeType type, int half_w, int half_d, int floor_y, int ceil_y) const;
    Voxel sample_base_voxel(int x, int y, int z, bool* out_is_vein = nullptr) const;
    bool find_wall_surface_voxel(const glm::ivec3& anchor, const RoomPlacement* room,
                                 glm::ivec3& out_surface, glm::ivec3& out_inward_normal) const;

    // ── Material Scarcity & Vein Sampling ──
    float get_material_scarcity(uint8_t material_id) const {
        return MaterialScarcitySystem::instance().get_scarcity(material_id, m_sector_index);
    }
    float get_material_spawn_weight(uint8_t material_id) const {
        return MaterialScarcitySystem::instance().get_spawn_weight(material_id, m_sector_index);
    }
    uint8_t sample_vein_material(uint32_t vein_hash) const {
        return MaterialScarcitySystem::instance().sample_vein_ore(m_sector_index, vein_hash);
    }
    bool is_vein_voxel(int x, int y, int z) const {
        bool is_vein = false;
        sample_voxel(x, y, z, &is_vein);
        return is_vein;
    }

    void create_single_room_test_layout(RoomShapeType type);
    void create_corridor_test_layout();
    void set_room_type(int gx, int gz, RoomShapeType type);

    // ── Room Adjacency, Compatibility & Diversity Validation ──
    static RoomCategory get_room_category_static(RoomShapeType type) { return get_room_category(type); }
    static float get_adjacency_affinity_static(RoomShapeType typeA, RoomShapeType typeB) {
        return get_room_adjacency_affinity(typeA, typeB);
    }
    float get_adjacency_affinity(RoomShapeType typeA, RoomShapeType typeB) const {
        return get_room_adjacency_affinity(typeA, typeB);
    }
    bool are_rooms_adjacent(int gx1, int gz1, int gx2, int gz2, bool allow_diagonal = false) const;
    bool validate_room_adjacency_rules() const;
    int count_adjacent_same_type_violations() const;
    int count_diagonal_same_type_violations() const;

private:
    uint32_t hash_coord(int x, int y, int z, uint32_t salt = 0) const;
    float pseudo_rand(int x, int y, int z, uint32_t salt = 0) const;

    // Objective deposit placement & level validation
    void place_objective_resources();
    void validate_and_balance_objectives();
    void add_objective_voxel(int x, int y, int z);
    void add_objective_deposit(const glm::ivec3& anchor, int deposit_size, RoomPlacement* room = nullptr);

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

    // Dedicated Precursor Vault Chambers & Shapes
    void sample_precursor_sanctum_vault(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_subterranean_vault_bastion(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    void sample_abyssal_vault_chasm(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    bool sample_vault_structure(int x, int y, int z, Voxel& out) const;
    void configure_vault_placement(std::mt19937& rng);

    bool sample_random_fluid_structure(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    bool sample_random_fluid_pool(const RoomPlacement& room, int x, int y, int z, Voxel& out) const {
        return sample_random_fluid_structure(room, x, y, z, out);
    }
    bool sample_fluid_contained_pool(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    bool sample_fluid_stepping_stone_pool(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    bool sample_fluid_wall_cascade(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    bool sample_fluid_cascading_twin_pools(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    bool sample_fluid_hazard_trench(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;
    bool sample_fluid_drop_chasm_sump(const RoomPlacement& room, int x, int y, int z, Voxel& out) const;

    void generate_luminaries();

    int m_sector_index{1};
    uint32_t m_seed{1337};
    int m_grid_w{3};
    int m_grid_d{3};
    int m_world_h{WORLD_HEIGHT};
    int m_target_objective{25};
    bool m_validation_passed{false};
    std::vector<glm::ivec3> m_objective_positions;
    std::vector<uint8_t> m_objective_bitset;
    std::vector<RoomPlacement> m_rooms;
    std::vector<CorridorPlacement> m_corridors;
    std::vector<CavernLuminary> m_luminaries;
    std::array<std::array<int, MAX_GRID_SIZE>, MAX_GRID_SIZE> m_grid_room_indices;
    VaultPlacement m_vault_placement;
};

} // namespace Voidfall
