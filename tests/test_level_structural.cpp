#include <iostream>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <queue>
#include <unordered_set>

#include "../src/voxel/level_shapes.hpp"
#include "../src/voxel/world.hpp"
#include "../src/voxel/greedy_mesher.hpp"
#include "../src/player/controller.hpp"
#include "../src/systems/extraction.hpp"
#include "../src/systems/mission_system.hpp"

using namespace Voidfall;

#define TEST_CHECK(expr, msg) \
    if (!(expr)) { \
        std::cerr << "\n[TEST FAILED] " << msg << " (" #expr ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    }

static int s_passed_tests = 0;
void log_pass(const std::string& name) {
    s_passed_tests++;
    std::cout << "  [PASS " << s_passed_tests << "] " << name << std::endl;
}

// ─────────────────────────────────────────────────────────────
// 1. LEVEL DESIGN & ROOM SHAPES TOPOLOGY TESTS
// ─────────────────────────────────────────────────────────────
void test_scalable_material_scarcity_and_vein_purity() {
    std::cout << "\n=== [MODULE 13] Scalable Material Scarcity, Level Progression & Pure Single-Ore Veins ===" << std::endl;

    const auto& scarcity_sys = MaterialScarcitySystem::instance();

    // 1. Grade-Based Scarcity Invariant: In every sector, higher grade materials have higher scarcity rate
    for (int sector = 1; sector <= 5; ++sector) {
        float s_titanium = scarcity_sys.get_scarcity(MAT_TITANIUM, sector);
        float s_voidite = scarcity_sys.get_scarcity(MAT_VOIDITE_CRYSTAL, sector);
        float s_radio = scarcity_sys.get_scarcity(MAT_RADIOACTIVE_ORE, sector);
        float s_prism = scarcity_sys.get_scarcity(MAT_PRISMATIC_CRYSTAL, sector);
        float s_astral = scarcity_sys.get_scarcity(MAT_ASTRAL_ORE, sector);

        // Grade 1 vs Grade 2
        TEST_CHECK(s_titanium < s_voidite, "Grade 1 (Titanium) scarcity must be strictly lower than Grade 2 (Voidite) in sector " + std::to_string(sector));

        if (sector >= 2) {
            // Grade 2 vs Grade 3
            TEST_CHECK(s_voidite < s_radio, "Grade 2 (Voidite) scarcity must be lower than Grade 3 (Radioactive) in sector " + std::to_string(sector));
            // Grade 3 vs Grade 4
            TEST_CHECK(s_radio < s_prism, "Grade 3 (Radioactive) scarcity must be lower than Grade 4 (Prismatic) in sector " + std::to_string(sector));
        }

        if (sector >= 3) {
            // Grade 4 vs Grade 5
            TEST_CHECK(s_prism < s_astral, "Grade 4 (Prismatic) scarcity must be lower than Grade 5 (Astral Ore) in sector " + std::to_string(sector));
        }
    }

    // 2. Progression Invariant: As the user gets to later levels, scarcity of previous materials becomes LESS
    for (int sector = 1; sector <= 4; ++sector) {
        float s_titanium_now = scarcity_sys.get_scarcity(MAT_TITANIUM, sector);
        float s_titanium_next = scarcity_sys.get_scarcity(MAT_TITANIUM, sector + 1);
        TEST_CHECK(s_titanium_next < s_titanium_now, "Titanium scarcity must decrease as level increases");

        float s_voidite_now = scarcity_sys.get_scarcity(MAT_VOIDITE_CRYSTAL, sector);
        float s_voidite_next = scarcity_sys.get_scarcity(MAT_VOIDITE_CRYSTAL, sector + 1);
        TEST_CHECK(s_voidite_next < s_voidite_now, "Voidite scarcity must decrease as level increases");

        if (sector >= 2) {
            float s_radio_now = scarcity_sys.get_scarcity(MAT_RADIOACTIVE_ORE, sector);
            float s_radio_next = scarcity_sys.get_scarcity(MAT_RADIOACTIVE_ORE, sector + 1);
            TEST_CHECK(s_radio_next < s_radio_now, "Radioactive ore scarcity must decrease as level increases");

            float s_prism_now = scarcity_sys.get_scarcity(MAT_PRISMATIC_CRYSTAL, sector);
            float s_prism_next = scarcity_sys.get_scarcity(MAT_PRISMATIC_CRYSTAL, sector + 1);
            TEST_CHECK(s_prism_next < s_prism_now, "Prismatic crystal scarcity must decrease as level increases");
        }

        if (sector >= 3) {
            float s_astral_now = scarcity_sys.get_scarcity(MAT_ASTRAL_ORE, sector);
            float s_astral_next = scarcity_sys.get_scarcity(MAT_ASTRAL_ORE, sector + 1);
            TEST_CHECK(s_astral_next < s_astral_now, "Astral ore scarcity must decrease as level increases");
        }
    }

    // 3. Late Level Material Invariant: Astral Ore is 100% locked in sectors 1-2, and unlocks in sector 3+
    TEST_CHECK(scarcity_sys.get_scarcity(MAT_ASTRAL_ORE, 1) == 1.0f, "Astral ore must have 100% scarcity (locked) in Sector 1");
    TEST_CHECK(scarcity_sys.get_scarcity(MAT_ASTRAL_ORE, 2) == 1.0f, "Astral ore must have 100% scarcity (locked) in Sector 2");
    TEST_CHECK(scarcity_sys.get_scarcity(MAT_ASTRAL_ORE, 3) < 1.0f, "Astral ore must unlock in Sector 3");
    TEST_CHECK(scarcity_sys.get_spawn_weight(MAT_ASTRAL_ORE, 1) == 0.0f, "Astral ore spawn weight must be 0 in Sector 1");
    TEST_CHECK(scarcity_sys.get_spawn_weight(MAT_ASTRAL_ORE, 3) > 0.0f, "Astral ore spawn weight must be > 0 in Sector 3");

    // 4. Single-Ore Vein Purity Invariant: In a given vein, only that specific ore is allowed to spawn
    const uint32_t vein_seeds[] = {101, 777, 1337, 424242};
    for (int sector = 1; sector <= 4; ++sector) {
        for (uint32_t seed : vein_seeds) {
            LevelGenerator gen(sector, seed);

            // Map from vein cell (x/3, y/2, z/3) to the single detected ore type
            std::unordered_map<uint64_t, uint8_t> vein_ore_types;
            int total_vein_voxels = 0;

            for (int y = 4; y <= 25; ++y) {
                for (int z = 5; z < gen.world_depth() - 5; ++z) {
                    for (int x = 5; x < gen.world_width() - 5; ++x) {
                        bool is_vein = false;
                        Voxel v = gen.sample_voxel(x, y, z, &is_vein);
                        if (is_vein) {
                            uint64_t cell_key = (static_cast<uint64_t>(x / 3) & 0xFFFFFu) |
                                               ((static_cast<uint64_t>(y / 2) & 0xFFFFFu) << 20) |
                                               ((static_cast<uint64_t>(z / 3) & 0xFFFFFu) << 40);

                            auto it = vein_ore_types.find(cell_key);
                            if (it == vein_ore_types.end()) {
                                vein_ore_types[cell_key] = v.material_id;
                            } else {
                                // Purity check: EVERY block in this vein cluster must match the first block's ore!
                                TEST_CHECK(it->second == v.material_id,
                                    "Vein purity violation at (" + std::to_string(x) + ", " +
                                    std::to_string(y) + ", " + std::to_string(z) + "): expected " +
                                    std::to_string(it->second) + " but found " + std::to_string(v.material_id));
                            }
                            total_vein_voxels++;

                            // In Sector 1 and 2, Astral Ore should NEVER spawn in veins
                            if (sector <= 2) {
                                TEST_CHECK(v.material_id != MAT_ASTRAL_ORE, "Astral Ore must never spawn in Sector " + std::to_string(sector));
                            }
                        }
                    }
                }
            }

            TEST_CHECK(total_vein_voxels > 0, "Level must generate procedural mineral seams in rock matrix");
            TEST_CHECK(!vein_ore_types.empty(), "Level must have distinct coherent mineral veins");
        }
    }

    log_pass("Scalable material scarcity rates, progression curves, late-level Astral Ore unlock, and 100% pure single-ore vein generation verified");
}

// ─────────────────────────────────────────────────────────────
// 14. ROOM ADJACENCY CONSTRAINTS & CONTEXTUAL AFFINITY RULES
// ─────────────────────────────────────────────────────────────
void test_room_adjacency_and_diversity_rules() {
    std::cout << "\n=== [MODULE 14] Room Adjacency, Diversity & Contextual Selection Rules ===" << std::endl;

    int total_levels_tested = 0;
    int total_rooms_inspected = 0;

    for (int sector = 1; sector <= 3; ++sector) {
        for (uint32_t seed_offset = 0; seed_offset < 30; ++seed_offset) {
            uint32_t seed = 424200 + sector * 1000 + seed_offset * 17;
            LevelGenerator gen(sector, seed);
            gen.generate_layout();

            total_levels_tested++;
            const auto& rooms = gen.rooms();
            total_rooms_inspected += static_cast<int>(rooms.size());

            // 1. Hard invariant: NO two adjacent rooms of the same type anywhere in the level
            int adjacent_violations = gen.count_adjacent_same_type_violations();
            TEST_CHECK(adjacent_violations == 0,
                "Adjacency violation detected! Level sector=" + std::to_string(sector) +
                " seed=" + std::to_string(seed) + " has " + std::to_string(adjacent_violations) +
                " pairs of adjacent rooms sharing the exact same RoomShapeType!");

            // 2. Full rule set validation (extreme hazards away from spawn, hazard cluster limits)
            TEST_CHECK(gen.validate_room_adjacency_rules(),
                "validate_room_adjacency_rules failed for sector=" + std::to_string(sector) +
                " seed=" + std::to_string(seed));

            // 3. Spawn room isolation: Spawn cavern is exclusively at (0, 0)
            const RoomPlacement* spawn_room = gen.get_room_at_grid(0, 0);
            TEST_CHECK(spawn_room != nullptr, "Spawn room at (0, 0) must exist");
            if (spawn_room) {
                TEST_CHECK(spawn_room->type == RoomShapeType::SpawnStagingCavern,
                    "Room at (0, 0) must always be SpawnStagingCavern");
            }

            // 4. No other room in the level may be a SpawnStagingCavern
            for (const auto& room : rooms) {
                if (room.grid_x != 0 || room.grid_z != 0) {
                    TEST_CHECK(room.type != RoomShapeType::SpawnStagingCavern,
                        "SpawnStagingCavern must never be placed outside (0, 0)");
                }
            }

            // 5. Contextual affinity verification: verify neighbor room transitions
            for (const auto& r1 : rooms) {
                for (const auto& r2 : rooms) {
                    if (&r1 == &r2) continue;
                    int m_dist = std::abs(r1.grid_x - r2.grid_x) + std::abs(r1.grid_z - r2.grid_z);
                    if (m_dist == 1) { // Orthogonal neighbors
                        TEST_CHECK(r1.type != r2.type,
                            "Adjacent rooms (" + std::to_string(r1.grid_x) + "," + std::to_string(r1.grid_z) +
                            ") and (" + std::to_string(r2.grid_x) + "," + std::to_string(r2.grid_z) +
                            ") share identical type " + std::to_string(static_cast<int>(r1.type)));

                        float affinity = gen.get_adjacency_affinity(r1.type, r2.type);
                        TEST_CHECK(affinity > -500.0f, "Adjacent room pair must not be a forbidden pair (-9999 / extreme spawn hazard)");
                    }
                }
            }
        }
    }

    log_pass("Validated 90 level layouts (" + std::to_string(total_rooms_inspected) +
             " rooms): 0 adjacent same-type violations, 100% compliant with contextual adjacency selection matrix");
}

// ─────────────────────────────────────────────────────────────
// 15. EXTRACTION POD GROUNDING, CLEARANCE & RIGIDITY TESTS
// ─────────────────────────────────────────────────────────────
void test_structural_connectivity_and_grounding() {
    std::cout << "\n=== [MODULE 16] Structural Connectivity & Floating Block Prevention ===" << std::endl;

    // Verify across all 3 sectors with multiple random procedural seeds
    const int test_seeds[] = { 101, 777, 1337, 2026, 4242 };
    for (int sector = 1; sector <= 3; ++sector) {
        for (int seed : test_seeds) {
            World world(seed, false);
            world.generate_world(sector, seed);

            int unanchored = world.count_unanchored_blocks();
            TEST_CHECK(unanchored == 0,
                       "Sector " + std::to_string(sector) + " Seed " + std::to_string(seed) +
                       " has " + std::to_string(unanchored) + " floating/unanchored blocks!");

            int disconnected_clutter = world.count_disconnected_clutter();
            TEST_CHECK(disconnected_clutter == 0,
                       "Sector " + std::to_string(sector) + " Seed " + std::to_string(seed) +
                       " has " + std::to_string(disconnected_clutter) + " floating/disconnected clutter props!");
        }
    }

    // Stress test: Manually create a floating block in mid-air and verify ensure_structural_connectivity grounds or removes it
    {
        World world(5555, false);
        world.generate_world(1, 5555);

        // Place a floating block in the middle of open room air
        int test_x = 20, test_y = 12, test_z = 20;
        world.set_voxel(test_x, test_y, test_z, Voxel{MAT_VOLCANIC_BASALT, 0}, false);

        int unanchored_before = world.count_unanchored_blocks();
        TEST_CHECK(unanchored_before > 0, "Synthesized floating block must be detected by structural check");

        world.ensure_structural_connectivity();
        int unanchored_after = world.count_unanchored_blocks();
        TEST_CHECK(unanchored_after == 0, "ensure_structural_connectivity must eliminate all floating blocks");
    }

    log_pass("Structural connectivity verified across all sectors: 0 floating blocks, 0 disconnected clutter");
}

// ─────────────────────────────────────────────────────────────
// 17. DEDICATED PRECURSOR VAULT ROOMS, OBSTACLES & BOSS GUARDIANS
// ─────────────────────────────────────────────────────────────
void test_precursor_vault_rooms_and_level_obstacles() {
    std::cout << "\n=== [MODULE 17] Dedicated Precursor Vault Rooms, Level Obstacles & Boss Guardians ===" << std::endl;

    const uint32_t seeds[] = {101, 777, 1337, 8888};

    for (int sector = 1; sector <= 3; ++sector) {
        for (uint32_t seed : seeds) {
            LevelGenerator gen(sector, seed);
            TEST_CHECK(gen.has_vault(), "Every generated sector layout must configure a designated precursor vault");
            const auto& vp = gen.vault_placement();
            TEST_CHECK(vp.exists, "Vault placement flag must be true");
            TEST_CHECK(vp.room_index >= 0 && vp.room_index < static_cast<int>(gen.rooms().size()), "Vault room index must be in valid range");
            TEST_CHECK(vp.door_height == 3, "Vault door portal must be 3 blocks tall");

            // Chamber bounds sanity
            TEST_CHECK(vp.chamber_max.x >= vp.chamber_min.x + 4, "Chamber X span must be >= 5 voxels");
            TEST_CHECK(vp.chamber_max.y >= vp.chamber_min.y + 4, "Chamber Y span must be >= 5 voxels");
            TEST_CHECK(vp.chamber_max.z >= vp.chamber_min.z + 4, "Chamber Z span must be >= 5 voxels");

            // Level-specific obstacle and guardian validation
            if (sector == 1) {
                TEST_CHECK(vp.obstacle_type == VaultObstacleType::AquiferSteppingStones,
                           "Sector 1 vault must feature AquiferSteppingStones obstacle");
                TEST_CHECK(vp.has_guardian, "Sector 1 vault must feature a guardian sentry");
                TEST_CHECK(!vp.is_boss_guardian, "Sector 1 vault guardian must NOT be a boss monster");
                TEST_CHECK(vp.guardian_role == VaultGuardianRole::StalkerMelee,
                           "Sector 1 vault guardian must be StalkerMelee");
            } else if (sector == 2) {
                TEST_CHECK(vp.obstacle_type == VaultObstacleType::HazardTrenchSlag,
                           "Sector 2 vault must feature HazardTrenchSlag obstacle");
                TEST_CHECK(vp.has_guardian, "Sector 2 vault must feature a guardian sentry");
                TEST_CHECK(!vp.is_boss_guardian, "Sector 2 vault guardian must NOT be a boss monster");
                TEST_CHECK(vp.guardian_role == VaultGuardianRole::StalkerMelee || vp.guardian_role == VaultGuardianRole::StalkerShooter,
                           "Sector 2 vault guardian must be Melee or Shooter");
            } else { // Sector 3
                TEST_CHECK(vp.obstacle_type == VaultObstacleType::ChasmAbyssRadiation,
                           "Sector 3 vault must feature ChasmAbyssRadiation obstacle");
                TEST_CHECK(vp.has_guardian, "Sector 3 vault must feature a guardian sentry");
                TEST_CHECK(vp.is_boss_guardian, "Sector 3 vault guardian MUST be designated Boss Monster!");
                TEST_CHECK(vp.guardian_role == VaultGuardianRole::ChitinGoliath,
                           "Sector 3 vault guardian must be ChitinGoliath boss");
            }

            // World generation & voxel sampling integrity
            World world(seed, false);
            world.generate_world(sector, seed);

            // Door portal must be sealed with MAT_REINFORCED_VAULT_DOOR
            Voxel door_vox = world.get_voxel(vp.door_pos.x, vp.door_pos.y + 1, vp.door_pos.z);
            TEST_CHECK(door_vox.is_solid() && door_vox.material_id == MAT_REINFORCED_VAULT_DOOR,
                       "Vault entry portal must be sealed with MAT_REINFORCED_VAULT_DOOR before breach");

            // Relic Hyper-Core must exist inside chamber at vp.relic_pos
            Voxel relic_vox = world.get_voxel(vp.relic_pos.x, vp.relic_pos.y, vp.relic_pos.z);
            TEST_CHECK(relic_vox.material_id == MAT_PRISMATIC_CRYSTAL && (relic_vox.flags_and_damage & VOXEL_FLAG_EMISSIVE) != 0,
                       "Relic hyper-core must be MAT_PRISMATIC_CRYSTAL with VOXEL_FLAG_EMISSIVE");

            // Pedestal underneath relic
            Voxel ped_vox = world.get_voxel(vp.relic_pos.x, vp.relic_pos.y - 1, vp.relic_pos.z);
            TEST_CHECK(ped_vox.is_solid() && (ped_vox.material_id == MAT_PRECURSOR_STONE || ped_vox.material_id == MAT_TITANIUM || ped_vox.material_id == MAT_INDUSTRIAL_BULKHEAD),
                       "Relic pedestal must be solid foundation");

            // MissionSystem integration & Satchel Breach Verification
            MissionSystem missions;
            missions.embed_precursor_vault(world, sector);
            const auto& mv = missions.vault();
            TEST_CHECK(mv.exists, "MissionSystem vault must exist");
            TEST_CHECK(mv.state == VaultObjectiveState::Undiscovered, "Initial vault state must be Undiscovered");
            TEST_CHECK(mv.door_pos == vp.door_pos, "MissionSystem vault door must match LevelGenerator vault placement");
            TEST_CHECK(mv.relic_pos == vp.relic_pos, "MissionSystem relic pos must match LevelGenerator vault placement");

            // Proximity discovery
            glm::vec3 approach_vec(static_cast<float>(vp.approach_pos.x) + 0.5f,
                                   static_cast<float>(vp.approach_pos.y) + 1.0f,
                                   static_cast<float>(vp.approach_pos.z) + 0.5f);
            missions.update(0.1f, world, approach_vec);
            TEST_CHECK(missions.vault().state == VaultObjectiveState::Discovered, "Approaching vault door must discover vault");

            // Satchel charge breach detonation at door
            glm::vec3 blast_pos(static_cast<float>(vp.door_pos.x) + 0.5f,
                                static_cast<float>(vp.door_pos.y) + 1.0f,
                                static_cast<float>(vp.door_pos.z) + 0.5f);
            bool breached = missions.check_satchel_vault_breach(world, blast_pos, 2.5f);
            TEST_CHECK(breached, "Satchel charge at door must successfully breach vault");
            TEST_CHECK(missions.vault().state == VaultObjectiveState::Breached, "Vault state must transition to Breached");

            // After breach, door voxel must become air, allowing passage
            Voxel door_after = world.get_voxel(vp.door_pos.x, vp.door_pos.y + 1, vp.door_pos.z);
            TEST_CHECK(!door_after.is_solid() && door_after.material_id == MAT_AIR,
                       "Vault door voxels must be blown open into MAT_AIR after satchel breach");

            // Sector 3 Boss Guardian Verification in World Fauna
            if (sector == 3) {
                bool found_goliath = false;
                glm::vec3 target_pos(static_cast<float>(vp.approach_pos.x),
                                     static_cast<float>(vp.approach_pos.y),
                                     static_cast<float>(vp.approach_pos.z));
                for (const auto& ent : world.GetActiveEntities()) {
                    if (ent.role == StalkerRole::ChitinGoliath) {
                        found_goliath = true;
                        float dist_to_vault = glm::distance(ent.pos, target_pos);
                        TEST_CHECK(dist_to_vault < 12.0f, "Chitin Goliath boss monster must spawn guarding the precursor vault approach");
                        break;
                    }
                }
                TEST_CHECK(found_goliath, "Sector 3 world fauna must include designated Chitin Goliath boss monster");
            }
        }
    }

    log_pass("Dedicated Precursor Vault Rooms, Obstacles (Aquifer, Slag, Chasm), and Sector 3 Boss Monster verified");
}

void test_precursor_relic_model_and_geometry() {
    std::cout << "\n--- Module 5: Precursor Relic Custom Model & Interactive Retrieval ---" << std::endl;

    World world(42);
    world.generate_world(1, 42);

    const auto& vp = world.level_generator()->vault_placement();
    TEST_CHECK(vp.exists, "Vault placement must exist");

    // 1. Relic voxel in chamber must have VOXEL_FLAG_CUSTOM_MODEL
    Voxel relic_vox = world.get_voxel(vp.relic_pos.x, vp.relic_pos.y, vp.relic_pos.z);
    TEST_CHECK(relic_vox.material_id == MAT_PRISMATIC_CRYSTAL, "Relic voxel material must be MAT_PRISMATIC_CRYSTAL");
    TEST_CHECK(relic_vox.is_custom_model(), "Relic voxel must report is_custom_model() == true");
    TEST_CHECK(!relic_vox.is_renderable(), "Relic voxel must NOT be renderable by static chunk mesher (rendered via 3D entity model)");
    TEST_CHECK(relic_vox.is_solid(), "Relic voxel must remain solid so player raycast look targets detect it");

    // 2. Standard crystal voxels in cave environments must NOT have custom model flag
    Voxel standard_crystal{MAT_PRISMATIC_CRYSTAL, VOXEL_FLAG_EMISSIVE};
    TEST_CHECK(!standard_crystal.is_custom_model(), "Standard cave crystal must report is_custom_model() == false");
    TEST_CHECK(standard_crystal.is_renderable(), "Standard cave crystal must be renderable by greedy mesher");

    // 3. Mission retrieval lifecycle
    MissionSystem mission;
    mission.embed_precursor_vault(world, 1);
    TEST_CHECK(mission.vault().exists, "Mission vault must exist");
    TEST_CHECK(!mission.is_relic_retrieved(), "Relic must not be retrieved initially");

    // Before breach, relic cannot be interacted
    glm::vec3 relic_world = glm::vec3(mission.vault().relic_pos) + glm::vec3(0.5f);
    TEST_CHECK(!mission.can_interact_relic(relic_world), "Relic must not be interactable before vault is breached");

    // Satchel breach
    glm::vec3 door_world = glm::vec3(mission.vault().door_pos) + glm::vec3(0.5f);
    bool breached = mission.check_satchel_vault_breach(world, door_world, 3.5f);
    TEST_CHECK(breached, "Satchel blast must breach the vault door");
    TEST_CHECK(mission.is_vault_breached(), "Vault must be marked breached");

    // After breach, relic is interactable in proximity
    TEST_CHECK(mission.can_interact_relic(relic_world), "Relic must be interactable in proximity after breach");

    bool retrieved = mission.interact_relic(world, relic_world);
    TEST_CHECK(retrieved, "Interacting with relic must retrieve it");
    TEST_CHECK(mission.is_relic_retrieved(), "Mission must mark relic retrieved");

    Voxel post_retrieval_vox = world.get_voxel(mission.vault().relic_pos.x, mission.vault().relic_pos.y, mission.vault().relic_pos.z);
    TEST_CHECK(post_retrieval_vox.material_id == MAT_AIR, "Retrieved relic must be replaced with air");

    log_pass("Precursor Relic Custom Model, Mesher Suppression, Solid Raycast Collision, and Retrieval lifecycle verified");
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE -- STRUCTURAL & VAULT INTEGRITY TEST SUITE" << std::endl;
    std::cout << "==========================================================" << std::endl;

    test_scalable_material_scarcity_and_vein_purity();
    test_room_adjacency_and_diversity_rules();
    test_structural_connectivity_and_grounding();
    test_precursor_vault_rooms_and_level_obstacles();
    test_precursor_relic_model_and_geometry();

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "  ALL " << s_passed_tests << " STRUCTURAL INTEGRITY & VAULT TESTS PASSED!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
