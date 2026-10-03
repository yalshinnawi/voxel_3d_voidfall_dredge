#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "application.hpp"
#include "logger.hpp"
#include "screenshot.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>
#include <chrono>
#include <algorithm>

namespace Voidfall {

namespace {

inline std::string get_relative_direction_str(const glm::vec3& player_pos, float /*player_yaw*/, const glm::vec3& target_pos) {
    glm::vec3 diff = target_pos - player_pos;
    float dist = glm::length(glm::vec2(diff.x, diff.z));
    int dist_m = std::max(1, static_cast<int>(std::round(dist)));

    // World compass angle: atan2(diff.x, -diff.z) where North is -Z, East is +X
    float world_angle_deg = glm::degrees(std::atan2(diff.x, -diff.z));
    if (world_angle_deg < 0.0f) world_angle_deg += 360.0f;

    static const char* const CARDINALS[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    int sector = static_cast<int>(std::round(world_angle_deg / 45.0f)) % 8;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "[%dm %s]", dist_m, CARDINALS[sector]);
    return std::string(buf);
}

} // namespace

Application::Application(const AppConfig& config)
    : m_config(config)
{
    init_systems();
}

Application::~Application() {
}

void Application::init_systems() {
    // 1. Window
    WindowConfig win_cfg;
    win_cfg.title = "Voidfall: Dredge (Multiplayer Edition) - [" +
                    std::string(m_config.is_host ? "HOST" : "CLIENT") + "]";
    win_cfg.width = m_config.window_width > 0 ? m_config.window_width : 1600;
    win_cfg.height = m_config.window_height > 0 ? m_config.window_height : 900;
    win_cfg.visible = !m_config.hidden_window;
    m_window = std::make_unique<Window>(win_cfg);

    // Start with cursor unlocked for menus
    m_window->set_cursor_locked(false);

    m_window->set_resize_callback([this](int w, int h) {
        if (m_renderer) m_renderer->resize(w, h);
        if (m_hud) m_hud->resize(w, h);
        if (m_hub_ui) m_hub_ui->resize(w, h);
        if (m_pause_menu) m_pause_menu->resize(w, h);
    });

    // 2. Renderer & First-Person Viewmodel
    m_renderer = std::make_unique<Renderer>(m_window->width(), m_window->height());
    m_viewmodel = std::make_unique<ViewModel>();
    m_viewmodel->set_on_spark_callback([this](const glm::vec3& origin, const glm::vec3& dir) {
        if (m_renderer) {
            m_renderer->spawn_crack_debris(origin, glm::ivec3(glm::sign(dir)), 0.75f, MAT_VOIDITE_CRYSTAL);
        }
    });

    // 3. Voxel World & Procedural Caverns
    m_world = std::make_unique<World>(m_config.world_seed);

    // 4. Player Controller
    m_player = std::make_unique<PlayerController>(glm::vec3(16.0f, 22.0f, 16.0f));
    m_player->clamp_to_surface(*m_world);

    m_player->set_on_block_break([this](int x, int y, int z, const glm::ivec3& normal, uint8_t mat, uint8_t flags) {
        on_block_broken(x, y, z, normal, mat, flags);
    });

    m_window->set_key_callback([this](int key, int action) {
        process_input(key, action);
    });

    m_player->set_on_block_place([this](int x, int y, int z, uint8_t mat) {
        on_block_placed(x, y, z, mat);
    });

    m_player->set_on_bulkhead_dismantle([this](int x, int y, int z) {
        on_bulkhead_dismantled(x, y, z);
    });

    m_player->set_can_place_predicate([this]() {
        return m_inventory.has_bulkhead_material();
    });

    m_player->set_on_warning([this](const std::string& msg) {
        if (m_hud) m_hud->show_warning(msg);
    });

    m_player->set_on_sonar_cast([this](const glm::vec3& origin) {
        on_sonar_cast(origin);
    });

    m_player->set_on_explosive_blast([this](const glm::ivec3& origin, const glm::ivec3& dir, bool is_micro) {
        on_explosive_blast(origin, dir, is_micro);
    });

    m_player->set_on_tactical_ability([this](CharacterClass cls, const glm::vec3& pos, const glm::vec3& dir) {
        on_tactical_ability(cls, pos, dir);
    });

    m_player->set_on_jump([this](const glm::vec3& pos) {
        m_noise_meter.add_jump_sound(pos);
    });

    m_player->set_on_land([this](const glm::vec3& pos, float impact_speed) {
        m_noise_meter.add_landing_sound(pos, impact_speed);
    });

    // 5. Systems
    setup_hazard_system();

    m_noise_meter.set_on_wave([this](NoiseMeter::AlertLevel level, float pct) {
        if (!m_player || !m_world) return;
        int count = 1;
        if (level == NoiseMeter::AlertLevel::Alerted) {
            count = 1 + (m_selected_level >= 2 ? 1 : 0);
            if (m_hud) m_hud->show_warning("! SEISMIC NOISE: HOSTILE DETECTED !", 2.5f);
        } else if (level == NoiseMeter::AlertLevel::Agitated) {
            count = 2 + (m_selected_level >= 2 ? 1 : 0);
            if (m_hud) m_hud->show_warning("! HEAVY NOISE: VOID STALKER PACK APPROACHING !", 3.0f);
            m_player->add_trauma(0.2f);
        } else if (level == NoiseMeter::AlertLevel::Swarming) {
            count = 3 + (m_selected_level >= 2 ? 2 : 0);
            if (m_hud) m_hud->show_warning("! CRITICAL NOISE: VOID SWARM INCOMING !", 3.5f);
            m_player->add_trauma(0.4f);
        }
        m_stalkers.spawn_wave(m_player->position(), count, *m_world);
    });

    m_extraction = std::make_unique<ExtractionSystem>();
    m_extraction->set_on_complete([this]() {
        m_expedition_success = true;
        m_inventory.finalize_run(m_selected_level, true);
        m_state = GameState::Debrief;
        m_window->set_cursor_locked(false);
        std::cout << "[Extraction] Delver extraction complete! Returning to debrief." << std::endl;
    });

    // 6. UI systems
    m_hud = std::make_unique<HUD>(m_window->width(), m_window->height());
    m_hub_ui = std::make_unique<OrbitalHubUI>(m_window->width(), m_window->height());
    m_hub_ui->set_test_mode(is_test_mode());
    m_pause_menu = std::make_unique<PauseMenu>(m_window->width(), m_window->height());

    // 7. Audio Engine
    m_audio = std::make_unique<AudioEngine>();
    m_audio->init();
    m_audio->set_master_volume(m_settings.master_volume);

    // 8. Networking
    if (m_config.is_host) {
        m_host = std::make_unique<NetHost>(m_config.port);
        m_host->start();
        m_host->set_on_block_delta([this](const BlockDeltaPacket& delta) {
            int wx = delta.chunk_x * CHUNK_SIZE + (delta.local_block_idx % CHUNK_SIZE);
            int wy = delta.chunk_y * CHUNK_SIZE + ((delta.local_block_idx / CHUNK_SIZE) % CHUNK_SIZE);
            int wz = delta.chunk_z * CHUNK_SIZE + (delta.local_block_idx / (CHUNK_SIZE * CHUNK_SIZE));
            m_world->set_voxel(wx, wy, wz, Voxel{delta.material_id, delta.flags_and_damage}, true);
        });
    } else {
        m_client = std::make_unique<NetClient>();
        m_client->connect_to_host(m_config.connect_ip, m_config.port, m_config.player_name);
        m_client->set_on_seed_received([this](uint32_t seed) {
            m_world->set_seed(seed);
        });
        m_client->set_on_block_delta([this](const BlockDeltaPacket& delta) {
            int wx = delta.chunk_x * CHUNK_SIZE + (delta.local_block_idx % CHUNK_SIZE);
            int wy = delta.chunk_y * CHUNK_SIZE + ((delta.local_block_idx / CHUNK_SIZE) % CHUNK_SIZE);
            int wz = delta.chunk_z * CHUNK_SIZE + (delta.local_block_idx / (CHUNK_SIZE * CHUNK_SIZE));
            m_world->set_voxel(wx, wy, wz, Voxel{delta.material_id, delta.flags_and_damage}, true);
        });
        m_client->set_on_debris_spawn([this](const DynamicDebrisSpawnPacket& pkt) {
            m_debris.emplace_back(pkt.debris_id, pkt.origin, pkt.linear_velocity,
                                 pkt.angular_velocity, pkt.material_id, pkt.block_count);
        });
        m_client->set_on_sonar_ping([this](const SonarPingPacket& pkt) {
            m_renderer->trigger_sonar_pulse(pkt.origin);
        });
    }

    // Pre-generate initial chunks around spawn
    m_world->update(m_player->position(), 2);

    // 8. Determine save file & load profile persistence
    if (!m_config.save_file_path.empty()) {
        m_active_save_file = m_config.save_file_path;
    } else if (m_config.is_test_save || m_config.auto_play_test) {
        m_active_save_file = SaveSystem::TEST_SAVE_FILE;
    } else {
        m_active_save_file = SaveSystem::DEFAULT_SAVE_FILE;
    }

    if (m_config.fresh_save) {
        m_user_profile = UserProfile{};
        m_user_profile.player_name = m_config.player_name;
        SaveSystem::save_profile(m_user_profile, m_active_save_file);
        VF_LOG_INFO("Init", "Fresh save profile initialized at " << m_active_save_file 
                    << " (Level: " << m_user_profile.get_player_level()
                    << ", EXP: " << m_user_profile.total_exp 
                    << ", Coins: " << m_user_profile.total_coins
                    << ", Voidite: " << m_user_profile.total_voidite 
                    << ", Titanium: " << m_user_profile.total_titanium << ")");
    } else {
        SaveSystem::load_profile(m_user_profile, m_active_save_file);
    }

    m_settings = m_user_profile.settings;
    m_settings.sanitize();
    sync_audio_settings();

    // Sync all sector records, not just the first 3, to support endless progression
    for (int s = 1; s < PlayerInventory::MAX_SECTOR_RECORDS; ++s) {
        m_inventory.sector_records[s] = m_user_profile.sector_records[s];
    }
    sync_profile_with_player();
}

void Application::sync_audio_settings() {
    if (m_audio) {
        m_audio->set_master_volume(m_settings.master_volume);
        m_audio->set_sfx_volume(m_settings.sfx_volume);
        m_audio->set_enemy_volume(m_settings.enemy_volume);
        m_audio->set_ambient_volume(m_settings.ambient_volume);
        m_audio->set_ui_volume(m_settings.ui_volume);
        m_audio->set_mute_all(m_settings.mute_all);
    }
    if (m_renderer) {
        m_renderer->set_brightness(m_settings.brightness);
    }
}

void Application::sync_profile_with_player() {
    CharacterClass cls = static_cast<CharacterClass>(m_user_profile.selected_class_id);
    if (m_player) {
        m_player->apply_attributes_and_upgrades(cls, m_user_profile.upgrades);
    }
    if (m_viewmodel) {
        m_viewmodel->set_character_class(cls);
        m_viewmodel->set_drill_speed_tier(m_user_profile.upgrades.drillSpeedTier);
        m_viewmodel->set_drill_durability_tier(m_user_profile.upgrades.drillDurabilityTier);
    }
}

void Application::start_expedition(int level) {
    VF_LOG_INFO("Expedition", "Launching expedition into Sector " << level);
    m_selected_level = level;
    if (m_renderer) {
        m_renderer->set_sector(level);
    }

    // Voidite target scales: Sector 1 = 25, Sector 2 = 35, Sector 3 = 50, then +15 per sector beyond 3, capped at 200
    int voidite_target = (level <= 1) ? 25 :
                         (level == 2) ? 35 :
                         (level == 3) ? 50 :
                         std::min(50 + (level - 3) * 15, 200);
    m_inventory.reset(voidite_target);
    m_expedition_time = 0.0f;

    // Apply Delver Archetype & Upgrades
    CharacterClass cls = static_cast<CharacterClass>(m_user_profile.selected_class_id);
    auto attr = get_character_attributes(cls);
    sync_profile_with_player();

    // Baseline Bulkheads per class
    m_inventory.max_bulkheads = attr.maxBulkheads;
    m_inventory.bulkheads = std::min(m_inventory.bulkheads, m_inventory.get_max_bulkheads());

    // Generate Level Sector Procedural Topology
    m_world->generate_world(level, m_world->seed());
    m_world->update(m_player->position(), 2);

    m_player->set_position(glm::vec3(16.0f, 22.0f, 16.0f));
    m_player->clamp_to_surface(*m_world);

    // Reset systems
    m_noise_meter.reset();
    m_stalkers.reset();
    m_burrowers.reset();
    m_plasma_bolts.clear();
    setup_hazard_system();
    m_hazard->set_sector_parameters(level);

    // Pre-populate ambient subterranean predators roaming in distant cavern chambers (Smart Spawning)
    if (m_world->level_generator()) {
        const auto& rooms = m_world->level_generator()->rooms();
        for (size_t i = 1; i < rooms.size(); ++i) { // Skip room 0 (spawn arrival bay)
            // SMART SPAWNING: Enforce a generous safety buffer around the player's initial entry bay.
            // Do not spawn any ambient stalkers in rooms or corridors within 28 meters of the player's arrival point.
            float dist_from_spawn = glm::length(glm::vec2(
                static_cast<float>(rooms[i].center.x) - m_player->position().x,
                static_cast<float>(rooms[i].center.z) - m_player->position().z
            ));
            if (dist_from_spawn < 28.0f) {
                continue; // Protected initial deployment zone
            }

            glm::vec3 rpos(static_cast<float>(rooms[i].center.x),
                           static_cast<float>(rooms[i].floor_y) + 1.2f,
                           static_cast<float>(rooms[i].center.z));
            m_stalkers.spawn_ambient_stalker(rpos, *m_world);
        if (level >= 2 && (i % 2 == 0)) {
                // Secondary prowling predator in larger hazard chambers
                m_stalkers.spawn_ambient_stalker(rpos + glm::vec3(3.5f, 0.0f, -3.5f), *m_world);
            }
            // Add a third stalker in deep sectors (5+)
            if (level >= 5 && (i % 3 == 0)) {
                m_stalkers.spawn_ambient_stalker(rpos + glm::vec3(-2.5f, 0.0f, 4.0f), *m_world);
            }

            // High-threat tactical ambush spawns based on room archetype — scale shooter accuracy with sector
            float aggression = 1.0f + (level - 1) * 0.08f; // +8% per sector
            if (rooms[i].type == RoomShapeType::SpikeTrenchArena) {
                glm::vec3 perch(rpos.x + 3.0f, static_cast<float>(rooms[i].floor_y + 6.0f), rpos.z);
                m_stalkers.spawn_shooter(perch, aggression);
            } else if (rooms[i].type == RoomShapeType::RadioactiveCoreSanctuary) {
                m_stalkers.spawn_shooter(rpos + glm::vec3(4.0f, 1.0f, 4.0f), aggression);
                m_stalkers.spawn_melee(rpos + glm::vec3(-4.0f, 1.0f, -4.0f), aggression);
            } else if (rooms[i].type == RoomShapeType::VoidSingularityRift && level >= 3) {
                m_stalkers.spawn_shooter(rpos + glm::vec3(0.0f, 2.0f, 5.0f), aggression);
            }
        }
        // Burrowers spawn from Sector 3 onward; add one extra per 3 sectors beyond that
        int burrower_count = (level >= 3 && rooms.size() > 4) ? 1 + (level - 3) / 3 : 0;
        for (int b = 0; b < burrower_count && (4 + b) < static_cast<int>(rooms.size()); ++b) {
            int room_idx = 4 + b;
            glm::vec3 bpos(static_cast<float>(rooms[room_idx].center.x),
                           static_cast<float>(rooms[room_idx].floor_y) + 2.0f,
                           static_cast<float>(rooms[room_idx].center.z));
            m_burrowers.spawn_burrower(bpos, 1.2f + b * 0.1f);
        }
    }

    m_extraction = std::make_unique<ExtractionSystem>();
    m_extraction->set_on_complete([this]() {
        m_expedition_success = true;
        if (m_audio) m_audio->play_sound_2d(SoundCue::EvacTouchdown, 1.0f);
        m_inventory.finalize_run(m_selected_level, true); // Safe evac!
        m_state = GameState::Debrief;
        m_window->set_cursor_locked(false);
    });

    if (m_audio) {
        m_audio->stop_all();
        m_audio->set_current_sector(level);

        // Map sector to audio cue bank: 3 audio themes cycle per 3 sectors
        int audio_tier = std::min((level - 1) / 3, 2); // 0 = Sector 1-3, 1 = 4-6, 2 = 7+
        SoundCue arrival_stinger = (audio_tier == 0) ? SoundCue::SectorArrival1 :
                                   (audio_tier == 1) ? SoundCue::SectorArrival2 :
                                                       SoundCue::SectorArrival3;
        SoundCue ambient_drone   = (audio_tier == 0) ? SoundCue::AmbientSector1 :
                                   (audio_tier == 1) ? SoundCue::AmbientSector2 :
                                                       SoundCue::AmbientSector3;

        m_audio->play_sound_2d(arrival_stinger, 0.85f);
        m_audio->play_sound_2d(ambient_drone, 0.24f, 1.0f, true);
    }

    // From Sector 3 onward, impose an escalating hard-extraction countdown.
    // Sector 3 = 180s, each additional sector reduces by 10s, floor at 60s.
    if (level >= 3) {
        m_level3_timer = static_cast<float>(std::max(180 - (level - 3) * 10, 60));
    }

    m_death_sequence = false;
    m_death_timer = 0.0f;
    m_debrief_input_lock = 0.0f;
    m_tremor_spall_timer = 0.0f;
    if (m_hud) {
        m_hud->set_death_sequence(false);
    }

    m_window->set_cursor_locked(true);
    m_state = GameState::Gameplay;
}

void Application::on_block_broken(int x, int y, int z, const glm::ivec3& normal, uint8_t mat, uint8_t flags) {
    if (m_host) {
        int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int ly = (y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        uint16_t idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
        m_host->broadcast_block_delta(cx, cy, cz, idx, MAT_AIR, 0);
    } else if (m_client) {
        int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int ly = (y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        BlockDeltaPacket pkt;
        pkt.chunk_x = cx; pkt.chunk_y = cy; pkt.chunk_z = cz;
        pkt.local_block_idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
        pkt.material_id = MAT_AIR;
        pkt.flags_and_damage = 0;
        m_client->send_block_delta(pkt);
    }

    // Spawn 12 dynamic billboard debris quads
    m_renderer->spawn_break_particles(glm::vec3(x, y, z), normal, mat);

    // Update inventory, floating text, and award Demolitions XP
    glm::vec3 popup_pos(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.8f, static_cast<float>(z) + 0.5f);

    // Anti-exploit check: Player-placed voxels
    bool is_player_placed = (flags & VOXEL_FLAG_PLAYER_PLACED) != 0;
    if (is_player_placed) {
        // Award 0 score, 0 XP, and 0 rare loot drops for player-placed voxels.
        // Only refund the base 1 Bulkhead plate if dismantled/broken with tool.
        if (mat == MAT_INDUSTRIAL_BULKHEAD) {
            m_inventory.refund_bulkhead();
            m_hud->add_floating_loot(popup_pos, "+1 BULKHEAD REFUNDED", glm::vec4(0.7f, 0.8f, 0.9f, 0.9f));
        }
        return;
    }

    bool is_demolitionist = (m_user_profile.selected_class_id == 0);

    if (mat == MAT_VOIDITE_CRYSTAL) {
        int amt = 1;
        // Demolitionist trait: 25% chance of double refined crystal yield (+2 Voidite)
        static uint32_t s_demo_seed = 1337;
        s_demo_seed = s_demo_seed * 1103515245 + 12345;
        if (is_demolitionist && ((s_demo_seed % 4) == 0)) {
            amt = 2;
        }
        m_inventory.add_voidite(amt);
        int bonus_xp = is_demolitionist ? 3 : 0;
        int bonus_score = is_demolitionist ? 5 : 0;
        m_inventory.total_run_score += bonus_score;
        m_skills.add_demolitions_xp(12 + bonus_xp);
        std::string popup = (amt > 1) ? "+2 VOIDITE (+25 PTS [DEMO REFINING])" : "+1 VOIDITE (+15 PTS)";
        m_hud->add_floating_loot(popup_pos, popup, glm::vec4(0.0f, 0.94f, 1.0f, 1.0f)); // Glowing Bright Cyan
    } else if (mat == MAT_INDUSTRIAL_BULKHEAD) {
        m_inventory.add_titanium(2); // Grants +2 Titanium Cores
        m_hud->add_floating_loot(popup_pos, "+2 TITANIUM (+12 PTS)", glm::vec4(1.0f, 0.70f, 0.0f, 1.0f)); // Bright Golden-Orange (#FFB300)
        m_skills.add_demolitions_xp(15);
        if (m_inventory.bulkheads == 0 && m_inventory.titanium >= 1) {
            m_inventory.bulkheads++;
            m_inventory.titanium--;
            if (m_hud) m_hud->show_warning("+1 BULKHEAD FABRICATED FROM TITANIUM", 2.5f);
        }
    } else if (mat == MAT_FRACTURED_GRANITE) {
        if (m_inventory.add_scrap_from_granite()) {
            if (m_hud) m_hud->show_warning("+1 BULKHEAD FABRICATED FROM SCRAP", 2.5f);
        }
        m_hud->add_floating_loot(popup_pos, "+1 SCRAP METAL", glm::vec4(0.85f, 0.85f, 0.9f, 1.0f));
        m_skills.add_demolitions_xp(5);
    } else if (mat == MAT_RADIOACTIVE_ORE) {
        m_inventory.add_salvage(15);
        int bonus_xp = is_demolitionist ? 5 : 0;
        int bonus_score = is_demolitionist ? 20 : 0;
        m_inventory.total_run_score += bonus_score;
        m_skills.add_demolitions_xp(20 + bonus_xp);
        std::string popup = is_demolitionist ? "+15 RADIOACTIVE (+100 PTS [DEMO +25%])" : "+15 RADIOACTIVE (+80 PTS)";
        m_hud->add_floating_loot(popup_pos, popup, glm::vec4(0.2f, 1.0f, 0.35f, 1.0f));
    } else if (mat == MAT_REINFORCED_VAULT_DOOR) {
        m_inventory.vault_breached = true;
        m_inventory.add_relic();
        m_hud->add_floating_loot(popup_pos, "+1 RELIC HYPER-CORE (+250 PTS)", glm::vec4(1.0f, 0.82f, 0.2f, 1.0f));
        m_skills.add_demolitions_xp(50);
        m_trauma = std::min(m_trauma + 0.35f, 1.0f);
    } else {
        m_inventory.add_salvage(1);
        m_hud->add_floating_loot(popup_pos, "+1 SALVAGE (+10 PTS)", glm::vec4(0.7f, 0.75f, 0.8f, 1.0f));
        m_skills.add_demolitions_xp(5);
    }

    // Accumulate mining fracture sound event based on material hardness & 3D location
    m_noise_meter.add_voxel_break_sound(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), mat);
    if (m_hazard) {
        m_hazard->on_rock_mined(mat);
    }

    // Drill contact damage to any ambush stalker right in front of drill bit
    m_stalkers.damage_nearest(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), 2.5f, 25.0f);

    // 3D Spatial Audio Cue for Voxel Fracture
    if (m_audio) {
        SoundCue cue = SoundCue::VoxelBreakBasalt;
        if (mat == MAT_TITANIUM) cue = SoundCue::VoxelBreakTitanium;
        else if (mat == MAT_VOIDITE_CRYSTAL) cue = SoundCue::VoxelBreakVoidite;
        else if (mat == MAT_INDUSTRIAL_BULKHEAD) cue = SoundCue::VoxelBreakBulkhead;
        else if (mat == MAT_RADIOACTIVE_ORE) cue = SoundCue::VoxelBreakRadioactive;
        m_audio->play_sound_3d(cue, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), 1.0f);
    }

    // Authoritative Anchored Island BFS Cave-in Solver
    if (m_config.is_host) {
        auto unanchored = StructuralCheck::solve_cavein(*m_world, x, y, z, 512);
        for (const auto& island : unanchored) {
            uint32_t did = m_next_debris_id++;
            glm::vec3 vel(0.0f, -1.5f, 0.0f);
            glm::vec3 rot(0.2f, 0.5f, 0.1f);
            m_debris.emplace_back(did, island.center_of_mass, vel, rot, island.primary_material, island.blocks.size());
            if (m_host) {
                m_host->broadcast_debris_spawn(did, island.center_of_mass, vel, island.primary_material, island.blocks.size());
            }
        }
    }
}

void Application::on_block_placed(int x, int y, int z, uint8_t mat) {
    if (mat == MAT_INDUSTRIAL_BULKHEAD || mat == MAT_BULKHEAD) {
        m_inventory.consume_bulkhead();
    }

    // Metallic installation sound event in 3D cavern space
    m_noise_meter.add_bulkhead_sound(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), true);

    if (m_audio) {
        m_audio->play_sound_3d(SoundCue::BulkheadDeploy, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), 1.0f);
    }

    uint8_t flags = (mat == MAT_INDUSTRIAL_BULKHEAD || mat == MAT_BULKHEAD) ? VOXEL_FLAG_PLAYER_PLACED : 0;

    if (m_host) {
        int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int ly = (y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        uint16_t idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
        m_host->broadcast_block_delta(cx, cy, cz, idx, mat, flags);
    } else if (m_client) {
        int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int ly = (y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        BlockDeltaPacket pkt;
        pkt.chunk_x = cx; pkt.chunk_y = cy; pkt.chunk_z = cz;
        pkt.local_block_idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
        pkt.material_id = mat;
        pkt.flags_and_damage = flags;
        m_client->send_block_delta(pkt);
    }
}

void Application::on_bulkhead_dismantled(int x, int y, int z) {
    // 1. Remove voxel cleanly from world without triggering cave-in check
    m_world->set_voxel(x, y, z, Voxel{MAT_AIR, 0}, true);

    // Bulkhead dismantling sound event
    m_noise_meter.add_bulkhead_sound(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), false);

    if (m_audio) {
        m_audio->play_sound_3d(SoundCue::BulkheadDismantle, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), 1.0f);
    }

    // 2. Refund bulkhead material to inventory
    m_inventory.refund_bulkhead();

    // 3. Floating feedback
    glm::vec3 popup_pos(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.8f, static_cast<float>(z) + 0.5f);
    if (m_hud) {
        m_hud->add_floating_loot(popup_pos, "+1 BULKHEAD RETRIEVED", glm::vec4(1.0f, 0.70f, 0.0f, 1.0f));
    }

    // 4. Particle effect
    if (m_renderer) {
        m_renderer->spawn_break_particles(glm::vec3(x, y, z), glm::ivec3(0, 1, 0), MAT_INDUSTRIAL_BULKHEAD);
    }

    // 5. Network broadcast
    if (m_host) {
        int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int ly = (y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        uint16_t idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
        m_host->broadcast_block_delta(cx, cy, cz, idx, MAT_AIR, 0);
    } else if (m_client) {
        int cx = (x >= 0) ? (x / CHUNK_SIZE) : ((x - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cy = (y >= 0) ? (y / CHUNK_SIZE) : ((y - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cz = (z >= 0) ? (z / CHUNK_SIZE) : ((z - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int lx = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int ly = (y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        BlockDeltaPacket pkt;
        pkt.chunk_x = cx; pkt.chunk_y = cy; pkt.chunk_z = cz;
        pkt.local_block_idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
        pkt.material_id = MAT_AIR;
        pkt.flags_and_damage = 0;
        m_client->send_block_delta(pkt);
    }
}

void Application::on_sonar_cast(const glm::vec3& origin) {
    CharacterClass cls = static_cast<CharacterClass>(m_user_profile.selected_class_id);
    auto attr = get_character_attributes(cls);
    int effective_rank = std::max(m_user_profile.upgrades.sonarFrequencyTier, m_skills.get_surveying_rank());
    float radius = attr.sonarRadius + 2.0f * static_cast<float>(effective_rank);
    float linger_bonus = attr.scanLingerBonus;

    m_surveying.trigger_scan(origin, *m_world, radius, linger_bonus, effective_rank);
    m_renderer->trigger_sonar_pulse(origin);
    m_skills.add_surveying_xp(40);

    if (m_audio) {
        m_audio->play_sound_3d(SoundCue::SonarPulse, origin, 1.0f);
    }

    // Sonar pulse emits spatial acoustic ping noise and stuns nearby hostile organisms
    m_noise_meter.add_sonar_sound(origin, 6.0f);
    m_stalkers.apply_sonar_stun(origin, radius);

    float effective_cooldown = std::max(4.0f, attr.sonarCooldown - (effective_rank * 1.0f));
    m_player->set_sonar_max_cooldown(effective_cooldown);

    if (m_host) {
        m_host->broadcast_sonar_ping(1, origin, radius * 2.0f);
    } else if (m_client) {
        m_client->send_sonar_ping(origin);
    }
}

void Application::on_explosive_blast(const glm::ivec3& origin, const glm::ivec3& dir, bool is_micro) {
    bool is_demolitionist = (m_user_profile.selected_class_id == 0);

    glm::vec3 blast_origin = glm::vec3(origin) + glm::vec3(0.5f);
    if (m_audio) {
        m_audio->play_sound_3d(SoundCue::ExplosiveBlast, blast_origin, is_micro ? 0.95f : 1.25f);
    }

    // Explosive concussive blast generates severe spatial noise and shatters nearby stalkers
    m_noise_meter.add_demolition_sound(blast_origin, is_micro);
    if (m_hazard) {
        m_hazard->on_explosive_detonation(is_micro ? 16.0f : 32.0f);
    }
    m_stalkers.damage_nearest(glm::vec3(origin), is_micro ? 4.0f : 8.0f, is_micro ? 35.0f : 70.0f);

    if (is_micro) {
        // Surgical 1x1x3 (or 1x1x4 for Demolitionist) directional blast that removes a tunnel without destroying adjacent fragile ore
        glm::ivec3 forward_step = (glm::length(glm::vec3(dir)) > 0.1f) ? -dir : glm::ivec3(0, 0, -1);
        int blast_steps = is_demolitionist ? 4 : 3;
        for (int step = 0; step < blast_steps; ++step) {
            glm::ivec3 bpos = origin + forward_step * step;
            Voxel v = m_world->get_voxel(bpos.x, bpos.y, bpos.z);
            if (v.is_solid() && v.material_id != MAT_DREDGE_BEDROCK) {
                // Do not destroy adjacent voidite ore surgically
                if (v.material_id != MAT_VOIDITE_CRYSTAL) {
                    m_world->set_voxel(bpos.x, bpos.y, bpos.z, Voxel{MAT_AIR, 0}, true);
                    on_block_broken(bpos.x, bpos.y, bpos.z, -forward_step, v.material_id, v.flags_and_damage);
                }
            }
        }
        m_trauma = std::min(m_trauma + 0.2f, 1.0f);
    } else {
        // Heavy Demolition Charge: 3x3x3 spherical blast radius (expanded +1 perimeter for Demolitionist)
        if (m_inventory.demolition_charges <= 0) return;
        m_inventory.demolition_charges--;

        int blast_rad = is_demolitionist ? 2 : 1;
        for (int dx = -blast_rad; dx <= blast_rad; ++dx) {
            for (int dy = -blast_rad; dy <= blast_rad; ++dy) {
                for (int dz = -blast_rad; dz <= blast_rad; ++dz) {
                    if (is_demolitionist && (std::abs(dx) + std::abs(dy) + std::abs(dz) > 3)) continue;
                    glm::ivec3 bpos = origin + glm::ivec3(dx, dy, dz);
                    Voxel v = m_world->get_voxel(bpos.x, bpos.y, bpos.z);
                    if (v.is_solid() && v.material_id != MAT_DREDGE_BEDROCK) {
                        m_world->set_voxel(bpos.x, bpos.y, bpos.z, Voxel{MAT_AIR, 0}, true);
                        on_block_broken(bpos.x, bpos.y, bpos.z, glm::ivec3(0, 1, 0), v.material_id, v.flags_and_damage);
                    }
                }
            }
        }
        m_trauma = std::min(m_trauma + 0.45f, 1.0f);
    }
}

void Application::on_tactical_ability(CharacterClass cls, const glm::vec3& pos, const glm::vec3& dir) {
    if (!m_player || !m_world) return;

    if (cls == CharacterClass::Demolitionist) {
        // Concussion shaped charge shockwave forward
        if (m_hud) m_hud->show_warning("TACTICAL: CONCUSSION SHOCKWAVE BLAST!", 2.5f);
        m_player->add_trauma(0.35f);
        m_noise_meter.add_explosive_noise(25.0f);

        glm::vec3 eye = pos + glm::vec3(0.0f, 0.7f, 0.0f);
        RaycastHit hit = m_world->raycast(eye, dir, 8.0f);
        glm::ivec3 center = hit.hit ? hit.block_pos : glm::ivec3(glm::floor(eye + dir * 5.0f));

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dz = -1; dz <= 1; ++dz) {
                    glm::ivec3 bp = center + glm::ivec3(dx, dy, dz);
                    Voxel vox = m_world->get_voxel(bp.x, bp.y, bp.z);
                    if (vox.is_solid() && vox.material_id != MAT_DREDGE_BEDROCK && vox.material_id != MAT_REINFORCED_VAULT_DOOR) {
                        m_world->set_voxel(bp.x, bp.y, bp.z, Voxel{MAT_AIR, 0}, true);
                        if (m_renderer) {
                            m_renderer->spawn_break_particles(glm::vec3(bp) + glm::vec3(0.5f), glm::ivec3(0, 1, 0), vox.material_id);
                        }
                    }
                }
            }
        }
        if (m_renderer) {
            m_renderer->trigger_dust_kickup(2.5f);
        }
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::ExplosiveBlast, glm::vec3(center), 1.15f);
        }
        m_stalkers.damage_nearest(glm::vec3(center), 6.0f, 100.0f);
        m_burrowers.damage_nearest(glm::vec3(center), 8.0f, 100.0f, true);

    } else if (cls == CharacterClass::Vanguard) {
        // Fortress deployable barricade: 3 wide x 2 high barrier in front of player
        if (m_hud) m_hud->show_warning("TACTICAL: FORTRESS BARRICADE DEPLOYED!", 2.5f);
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::TacticalBarricade, pos, 1.0f);
        }
        glm::vec3 flat_dir = glm::normalize(glm::vec3(dir.x, 0.0f, dir.z));
        if (glm::length(flat_dir) < 0.1f) flat_dir = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 side(-flat_dir.z, 0.0f, flat_dir.x);

        glm::vec3 wall_center = pos + flat_dir * 1.8f;
        int cy = static_cast<int>(std::floor(pos.y));

        for (int s = -1; s <= 1; ++s) {
            glm::vec3 col_pos = wall_center + side * static_cast<float>(s);
            int cx = static_cast<int>(std::floor(col_pos.x));
            int cz = static_cast<int>(std::floor(col_pos.z));
            for (int y = cy; y <= cy + 1; ++y) {
                Voxel cur = m_world->get_voxel(cx, y, cz);
                if (cur.material_id == MAT_AIR) {
                    m_world->set_voxel(cx, y, cz, Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_PLAYER_PLACED}, true);
                }
            }
        }
        m_player->set_trauma(0.0f);
        m_player->set_health(m_player->health() + 15.0f);

    } else if (cls == CharacterClass::Scout) {
        // Kinetic Dash Overcharge
        if (m_hud) m_hud->show_warning("TACTICAL: KINETIC DASH OVERCHARGE!", 2.0f);
        if (m_audio) {
            m_audio->play_sound_2d(SoundCue::TacticalOvercharge, 1.0f);
        }
        glm::vec3 dash_vel = dir * 20.0f;
        dash_vel.y = std::max(dash_vel.y + 2.5f, 3.0f);
        m_player->set_velocity(dash_vel);

        auto& exo = m_player->exo_mut();
        exo.power = std::min(exo.max_power, exo.power + 40.0f);
        exo.heat = 0.0f;
        exo.overheated = false;
        m_player->grapple_mut().active = false;
        m_player->add_trauma(0.08f);
    }
}

void Application::setup_hazard_system() {
    m_hazard = std::make_unique<HazardClock>();

    // 0.75 seconds before ceiling blocks detach:
    m_hazard->set_on_tremor_warning([this]() {
        if (m_hud) {
            m_hud->show_warning("[!] INCOMING TECTONIC SPALL - EVADE [!]", 2.0f);
        }
        if (m_audio) {
            m_audio->play_sound_2d(SoundCue::SeismicTremor, 0.45f, 1.15f);
        }
        glm::vec3 p = m_player->position();
        for (int i = 0; i < 6; ++i) {
            float ox = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 3.5f;
            float oz = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 3.5f;
            float oy = 5.0f + static_cast<float>(rand() % 5);
            m_renderer->spawn_break_particles(p + glm::vec3(ox, oy, oz), glm::ivec3(0, -1, 0), MAT_FRACTURED_GRANITE);
        }
        if (m_renderer) {
            m_renderer->trigger_dust_kickup(2.0f);
        }
    });

    m_hazard->set_on_tremor([this](float intensity) {
        m_player->add_trauma(intensity * 0.45f);
        m_trauma = std::min(m_trauma + intensity * 0.45f, 1.0f);
        if (m_audio) {
            m_audio->set_seismic_rumble(intensity);
        }
        if (m_renderer) {
            m_renderer->trigger_dust_kickup(3.5f);
        }
        if (m_hud) {
            m_hud->show_warning("<< SEISMIC SENSOR: TECTONIC STRATUM SHIFT >>", 1.8f);
        }

        // Seismic Detachment Pass: locate 3-6 candidate blocks 2-14 units above player within 7-block radius
        auto detach_blocks = StructuralCheck::query_seismic_detachment_blocks(
            *m_world,
            m_player->position(),
            3, 6,   // 3 to 6 blocks
            7.0f,   // 7-block radius
            2, 14   // 2 to 14 units directly above
        );

        // Fallback: if ceiling is higher or wider, expand query
        if (detach_blocks.empty()) {
            detach_blocks = StructuralCheck::query_seismic_detachment_blocks(
                *m_world,
                m_player->position(),
                3, 6,
                9.0f,
                2, 18
            );
        }

        if (!detach_blocks.empty() && m_hud) {
            m_hud->show_warning("SEISMIC TREMOR: CEILING COLLAPSE - BLOCKS DETACHING!", 2.5f);
        }

        for (const auto& b : detach_blocks) {
            Voxel v = m_world->get_voxel(b.x, b.y, b.z);
            // Remove physical block from ceiling
            m_world->set_voxel(b.x, b.y, b.z, Voxel{MAT_AIR, 0}, true);

            // Spawn fracture dust/particles at ceiling detachment point
            if (m_renderer) {
                m_renderer->spawn_break_particles(
                    glm::vec3(b.x + 0.5f, b.y + 0.5f, b.z + 0.5f),
                    glm::ivec3(0, -1, 0),
                    v.material_id
                );
            }

            if (m_host) {
                int cx = (b.x >= 0) ? (b.x / CHUNK_SIZE) : ((b.x - CHUNK_SIZE + 1) / CHUNK_SIZE);
                int cy = (b.y >= 0) ? (b.y / CHUNK_SIZE) : ((b.y - CHUNK_SIZE + 1) / CHUNK_SIZE);
                int cz = (b.z >= 0) ? (b.z / CHUNK_SIZE) : ((b.z - CHUNK_SIZE + 1) / CHUNK_SIZE);
                int lx = (b.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
                int ly = (b.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
                int lz = (b.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
                uint16_t idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
                m_host->broadcast_block_delta(cx, cy, cz, idx, MAT_AIR, 0);
            }

            uint32_t did = m_next_debris_id++;
            float rvx = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 0.75f;
            float rvz = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 0.75f;
            glm::vec3 vel(rvx, -1.5f - static_cast<float>(rand() % 100) / 100.0f, rvz);
            float rot_x = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 2.0f;
            float rot_y = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 2.0f;
            float rot_z = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 2.0f;
            glm::vec3 rot(rot_x, rot_y, rot_z);
            m_debris.emplace_back(did, glm::vec3(b.x + 0.5f, b.y + 0.5f, b.z + 0.5f), vel, rot, v.material_id, 1);
            if (m_host) {
                m_host->broadcast_debris_spawn(did, glm::vec3(b.x + 0.5f, b.y + 0.5f, b.z + 0.5f), vel, v.material_id, 1);
            }
        }

        // Hazard Phase 3 & 4 (radiation >= 45% or Sector >= 2): chance to awaken Seismic Burrower!
        if (m_hazard->radiation_level() >= 45.0f && m_burrowers.active_count() == 0) {
            m_burrowers.spawn_hazard_wave(m_player->position(), *m_world);
            if (m_hud) {
                m_hud->show_warning("! SUBTERRANEAN SENSOR: SEISMIC BURROWER DETECTED !", 3.5f);
            }
        }
    });
}

void Application::process_input(int key, int action) {
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE) {
        if (m_state == GameState::Gameplay) {
            m_state = GameState::Paused;
            m_window->set_cursor_locked(false);
        } else if (m_state == GameState::Paused) {
            m_state = GameState::Gameplay;
            m_window->set_cursor_locked(true);
        } else if (m_state == GameState::OrbitalHub) {
            if (m_hub_ui) m_hub_ui->set_subview(MenuSubView::Main);
            m_state = GameState::MainMenu;
        } else if (m_state == GameState::Debrief) {
            if (m_hub_ui) m_hub_ui->set_subview(MenuSubView::Main);
            m_state = GameState::MainMenu;
        } else if (m_state == GameState::MainMenu) {
            if (m_hub_ui && m_hub_ui->subview() != MenuSubView::Main) {
                m_hub_ui->set_subview(MenuSubView::Main);
            }
            // In main menu: ESC does NOT exit the game.
            // Player quits intentionally via [ EXIT ] button on the title screen.
        }
    } else if (key == GLFW_KEY_H || key == GLFW_KEY_F1) {
        if (m_hud) {
            m_hud->toggle_help_briefing();
        }
    }
}

void Application::fixed_tick(float dt) {
    m_current_tick++;

    if (m_state != GameState::Gameplay) {
        return;
    }

    // Active in-world death sequence: player collapses, camera slumps, telemetry uploads
    if (m_death_sequence) {
        m_death_timer += dt;
        m_trauma = std::max(m_trauma, 0.45f * (1.0f - m_death_timer / DEATH_SEQUENCE_DURATION));
        if (m_hazard) {
            m_hazard->update(dt);
        }
        if (m_renderer) {
            m_renderer->update_particles(dt);
        }
        if (m_hud) {
            m_hud->update(dt);
        }
        if (m_death_timer >= DEATH_SEQUENCE_DURATION) {
            m_death_sequence = false;
            m_inventory.finalize_run(m_selected_level, false);
            m_state = GameState::Debrief;
            m_window->set_cursor_locked(false);
            m_debrief_input_lock = 1.2f; // Prevent immediate accidental click-through
        }
        return;
    }

    m_expedition_time += dt;

    // 1. Poll networking
    if (m_host) m_host->poll_network_events();
    if (m_client) {
        m_client->poll_network_events();
        PlayerInputPacket input = m_player->build_input_packet(m_current_tick, dt);
        m_client->send_input(input);
    }

    // Synchronize skill perks to PlayerController
    m_player->set_reel_speed_multiplier(m_skills.get_grapple_reel_multiplier());
    m_player->set_thruster_regen_multiplier(m_skills.get_thruster_regen_multiplier());
    m_player->set_allow_micro_charges(m_skills.has_micro_charges());
    m_player->set_carry_weight_multiplier(m_inventory.overburden_penalty(m_user_profile.selected_class_id));

    // 2. Physics & Player simulation
    m_player->update_physics(dt, *m_world);

    // Progression XP: Acrobatics for grappling, Suit for sprinting
    if (m_player->grapple().active) {
        m_skills.add_acrobatics_xp(1);
    }
    m_skills.add_suit_xp(1);

    // 3. Falling Debris simulation & player hazard damage
    glm::vec3 player_pos = m_player->position();
    int p_head_x = static_cast<int>(std::floor(player_pos.x));
    int p_head_y = static_cast<int>(std::floor(player_pos.y));
    int p_head_z = static_cast<int>(std::floor(player_pos.z));

    // Check Bulkhead shelter utility: player standing beneath a placed MAT_BULKHEAD
    bool covered_by_bulkhead = false;
    for (int check_y = p_head_y + 1; check_y <= p_head_y + 14; ++check_y) {
        for (int cdx = -1; cdx <= 1; ++cdx) {
            for (int cdz = -1; cdz <= 1; ++cdz) {
                Voxel v = m_world->get_voxel(p_head_x + cdx, check_y, p_head_z + cdz);
                if (v.material_id == MAT_INDUSTRIAL_BULKHEAD) {
                    covered_by_bulkhead = true;
                    break;
                }
            }
            if (covered_by_bulkhead) break;
        }
        if (covered_by_bulkhead) break;
    }

    for (auto it = m_debris.begin(); it != m_debris.end();) {
        auto res = it->update(dt, *m_world, player_pos, covered_by_bulkhead);

        if (res.shattered) {
            m_renderer->spawn_break_particles(res.shatter_pos, glm::ivec3(0, 1, 0), res.shatter_mat);
            if (m_audio) {
                m_audio->play_sound_3d(SoundCue::VoxelHit, res.shatter_pos, 0.75f);
            }
        }

        if (res.placed_on_ground) {
            // Block landed on cavern floor and settled as physical voxel block
            if (m_renderer) {
                m_renderer->spawn_break_particles(
                    glm::vec3(res.place_pos) + glm::vec3(0.5f, 0.9f, 0.5f),
                    glm::ivec3(0, 1, 0),
                    it->material_id()
                );
                m_renderer->trigger_dust_kickup(1.2f);
            }
            if (m_audio) {
                m_audio->play_sound_3d(SoundCue::VoxelHit, glm::vec3(res.place_pos), 0.85f);
            }
            if (m_host) {
                int px = res.place_pos.x;
                int py = res.place_pos.y;
                int pz = res.place_pos.z;
                int cx = (px >= 0) ? (px / CHUNK_SIZE) : ((px - CHUNK_SIZE + 1) / CHUNK_SIZE);
                int cy = (py >= 0) ? (py / CHUNK_SIZE) : ((py - CHUNK_SIZE + 1) / CHUNK_SIZE);
                int cz = (pz >= 0) ? (pz / CHUNK_SIZE) : ((pz - CHUNK_SIZE + 1) / CHUNK_SIZE);
                int lx = (px % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
                int ly = (py % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
                int lz = (pz % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
                uint16_t idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
                m_host->broadcast_block_delta(cx, cy, cz, idx, it->material_id(), 0);
            }
            if (glm::distance(player_pos, glm::vec3(res.place_pos)) < 8.0f) {
                m_player->add_trauma(0.08f);
                m_trauma = std::min(m_trauma + 0.08f, 1.0f);
            }
            // Debris crash sound event in cavern space
            m_noise_meter.add_debris_crash_sound(glm::vec3(res.place_pos), it->block_count());
        }

        if (res.hit_player) {
            float applied_dmg = m_player->take_damage(res.damage, true);
            m_player->add_trauma(0.35f);
            m_trauma = std::min(m_trauma + 0.35f, 1.0f);
            m_hud->trigger_damage_flash(0.7f);
            m_hud->show_warning("CRITICAL IMPACT: DELVER HIT BY FALLING DEBRIS (-" + std::to_string(static_cast<int>(applied_dmg)) + " HP)!", 3.0f);
        } else if (res.hit_bulkhead) {
            m_hud->show_warning("TITANIUM BULKHEAD DEFLECTED FALLING DEBRIS!", 2.0f);
        }

        if (it->is_destroyed()) {
            it = m_debris.erase(it);
        } else {
            ++it;
        }
    }

    // 4. Update Hazard Clock, Extraction, Surveying, and Particles
    m_hazard->update(dt);

    // Sustained Tectonic Tremor Shaking & Spalling
    if (m_hazard && m_hazard->is_tremoring()) {
        float tremor_int = m_hazard->tremor_intensity();
        if (tremor_int <= 0.0f) tremor_int = 1.0f;
        m_trauma = std::max(m_trauma, 0.70f * tremor_int);
        // Seismic rumble sound event emitted across cavern
        m_noise_meter.add_seismic_sound(m_player->position(), tremor_int * dt);
        if (m_audio) {
            m_audio->set_seismic_rumble(tremor_int);
        }
        m_tremor_spall_timer += dt;
        if (m_tremor_spall_timer >= 0.35f) {
            m_tremor_spall_timer = 0.0f;
            if (m_renderer && m_world && m_player) {
                m_renderer->trigger_dust_kickup(1.5f);
                glm::vec3 p = m_player->position();
                for (int i = 0; i < 4; ++i) {
                    float ox = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 4.0f;
                    float oz = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 4.0f;
                    float oy = 3.0f + static_cast<float>(rand() % 4);
                    m_renderer->spawn_break_particles(p + glm::vec3(ox, oy, oz), glm::ivec3(0, -1, 0), MAT_FRACTURED_GRANITE);
                }
            }
        }
    } else {
        m_tremor_spall_timer = 0.0f;
        if (m_audio && !m_death_sequence) {
            m_audio->set_seismic_rumble(0.0f);
        }
    }

    // Dynamic Spatial Radiation Proximity Query
    if (m_world && m_player) {
        m_proximity_radiation = m_world->query_radiation_proximity(m_player->position(), 12.0f);
        float amb_rad = m_hazard->radiation_level() / 100.0f;
        m_effective_radiation = std::clamp(m_proximity_radiation + amb_rad * 0.40f, 0.0f, 1.0f);
        if (m_audio) {
            m_audio->set_radiation_proximity(m_effective_radiation);
        }
    }

    m_extraction->update(dt, m_player->position());
    m_surveying.update(dt);
    m_hud->update(dt);
    m_renderer->update_particles(dt);

    if (m_config.capture_models || m_config.capture_level_shapes) {
        // Freeze AI physics and simulation during model/shape showcases so staged layouts remain pristine
        return;
    }

    // Update Mining Noise Meter & natural decay
    // Wire crouch stealth mechanic: Left Ctrl held = crouching = faster noise decay & stealth movement
    bool is_crouching = m_window->is_key_down(GLFW_KEY_LEFT_CONTROL);
    m_noise_meter.set_crouching(is_crouching);

    // Continuous Movement & Exo-Thruster Acoustic Sound Emissions:
    bool is_moving = (m_player->current_buttons() & (BTN_FORWARD | BTN_BACKWARD | BTN_LEFT | BTN_RIGHT)) != 0;
    bool is_sprinting = (m_player->current_buttons() & BTN_SPRINT) != 0;
    m_noise_meter.add_movement_sound(m_player->position(), dt, is_sprinting, is_crouching, m_player->is_grounded() && is_moving);

    if ((m_player->current_buttons() & BTN_THRUSTER) != 0) {
        m_noise_meter.add_jetpack_sound(m_player->position(), dt);
    }
    if (m_player->grapple().active) {
        bool is_reeling = (m_player->current_buttons() & BTN_GRAPPLE_REEL) != 0;
        m_noise_meter.add_grapple_sound(m_player->position(), is_reeling);
    }

    // Update active sound event lifetimes and noise meter decay
    m_noise_meter.update(dt);

    // Industrial Drill contact grinding: continuous damage against hostile enemies in front of the bit
    if (m_player->is_drilling()) {
        glm::vec3 drill_tip = m_player->position() + m_player->forward() * 1.5f;
        m_noise_meter.add_drill_sound(drill_tip, dt, 14.0f);
        if (m_hazard) {
            m_hazard->add_seismic_stress(1.5f * dt);
        }
        bool hit_stalker = m_stalkers.damage_nearest(drill_tip, 2.2f, 35.0f * dt);
        bool hit_burrower = m_burrowers.damage_nearest(drill_tip, 2.6f, 40.0f * dt, false);
        if ((hit_stalker || hit_burrower) && m_renderer) {
            m_renderer->spawn_crack_debris(drill_tip, glm::ivec3(0, 1, 0), 0.6f, 3);
        }
    }

    // Update Void Stalker hostile entities AI & collision (Sound-driven hearing + LOS-gated attacks)
    glm::vec3 headlamp_dir = m_player->forward();
    bool headlamp_on = m_renderer ? m_renderer->headlamp().enabled : true;
    bool is_drilling = m_player->is_drilling();

    auto stalker_res = m_stalkers.update(
        dt,
        m_player->position(),
        m_player->forward(),
        headlamp_dir,
        headlamp_on,
        m_noise_meter.noise_percent(),
        is_drilling,
        *m_world,
        m_noise_meter.recent_sounds(),
        is_crouching
    );

    if (stalker_res.total_damage > 0.0f) {
        float applied = m_player->take_damage(stalker_res.total_damage, false);
        m_player->add_trauma(0.45f);
        m_trauma = std::min(m_trauma + 0.45f, 1.0f);
        m_hud->trigger_damage_flash(0.75f);
        if (stalker_res.any_melee_hit) {
            m_hud->show_warning("RAZOR CLAW MAUL (-" + std::to_string(static_cast<int>(applied)) + " HP)!", 2.2f);
        } else {
            m_hud->show_warning("VOID SPINE PIERCE (-" + std::to_string(static_cast<int>(applied)) + " HP)!", 2.2f);
        }
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::StalkerHit, m_player->position(), 1.0f);
        }
    }
    if (stalker_res.any_projectile_fired) {
        m_hud->show_warning("<< INCOMING VOID SPINES >>", 1.5f);
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::VoxelBreakVoidite, m_player->position() + m_player->forward() * 4.0f, 0.9f);
        }
    }
    if (stalker_res.any_lunge) {
        m_player->add_trauma(0.25f);
        m_hud->show_warning("! AGGRESSIVE APEX LUNGE !", 1.6f);
    }
    if (stalker_res.any_spotted) {
        m_hud->show_warning("<< ALERT: SHADOW PREDATOR TRACKING YOU >>", 2.2f);
    } else if (stalker_res.any_heard_sound) {
        m_hud->show_warning("<< ACOUSTIC DISTURBANCE: CREATURE INVESTIGATING NOISE >>", 1.8f);
    }
    if (stalker_res.stalkers_killed > 0) {
        m_inventory.total_run_score += 100 * stalker_res.stalkers_killed;
        m_skills.add_demolitions_xp(25 * stalker_res.stalkers_killed);
        m_hud->show_warning("VOID STALKER ELIMINATED (+100 PTS)", 2.0f);
    }

    // Update Seismic Burrower hostile entities AI, excavation & cave-in triggers
    auto burrower_res = m_burrowers.update(dt, m_player->position(), *m_world, m_noise_meter.recent_sounds());

    if (burrower_res.total_damage > 0.0f) {
        float applied = m_player->take_damage(burrower_res.total_damage, false);
        m_player->add_trauma(0.55f);
        m_trauma = std::min(m_trauma + 0.55f, 1.0f);
        m_hud->trigger_damage_flash(0.85f);
        m_hud->show_warning("! TECTONIC RAM IMPACT (-" + std::to_string(static_cast<int>(applied)) + " HP) !", 2.5f);
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::ExplosiveBlast, m_player->position(), 1.0f);
        }
    }
    if (burrower_res.any_breach) {
        m_player->add_trauma(0.35f);
        m_hud->show_warning("! SEISMIC BURROWER BREACH !", 2.5f);
        if (m_renderer) {
            m_renderer->trigger_dust_kickup(3.0f);
        }
        if (m_audio) {
            m_audio->play_sound_2d(SoundCue::SeismicTremor, 0.8f);
        }
    }
    if (burrower_res.any_cavein_triggered) {
        m_hazard->force_tremor(1.4f);
        m_hud->show_warning("! BURROWER SHOCKWAVE TRIGGERED CAVE-IN !", 2.5f);
    }
    if (burrower_res.max_rumble > 0.1f) {
        m_trauma = std::max(m_trauma, burrower_res.max_rumble * 0.18f);
    }
    if (burrower_res.burrowers_killed > 0) {
        m_inventory.total_run_score += 250 * burrower_res.burrowers_killed;
        m_skills.add_demolitions_xp(60 * burrower_res.burrowers_killed);
        m_hud->show_warning("SEISMIC BURROWER DESTROYED (+250 PTS)", 3.0f);
    }
 
    // 5. Delver Combat Firearm Firing & Ballistics Simulation (Class Archetype Specialized)
    if (m_player->active_tool() == ToolSlot::CombatWeapon && m_player->try_fire_weapon(m_plasma_bolts, dt)) {
        SoundCue cue = SoundCue::PlasmaFire;
        switch (m_player->weapon_archetype()) {
            case WeaponArchetype::MagmaScattergun: cue = SoundCue::ScattergunFire; break;
            case WeaponArchetype::PlasmaCarbine:   cue = SoundCue::PlasmaFire; break;
            case WeaponArchetype::NeedlerRailgun:  cue = SoundCue::RailgunFire; break;
        }

        if (m_audio) {
            m_audio->play_sound_2d(cue, 0.85f);
        }
        // Gunshot acoustic muzzle blast sound event
        m_noise_meter.add_gunshot_sound(m_player->position() + m_player->forward() * 0.8f, static_cast<int>(m_player->weapon_archetype()));
        m_player->add_trauma(m_player->weapon_stats().trauma_kick);
    }

    // Update active plasma bolts
    for (auto& bolt : m_plasma_bolts) {
        if (!bolt.active) continue;

        glm::vec3 next_pos = bolt.position + bolt.velocity * dt;
        bolt.lifetime -= dt;
        if (bolt.lifetime <= 0.0f) {
            bolt.active = false;
            continue;
        }

        // 1. Raycast / voxel collision against solid blocks
        int bx = static_cast<int>(std::floor(next_pos.x));
        int by = static_cast<int>(std::floor(next_pos.y));
        int bz = static_cast<int>(std::floor(next_pos.z));
        Voxel v = m_world->get_voxel(bx, by, bz);
        if (v.is_solid()) {
            bolt.active = false;
            if (m_renderer) {
                m_renderer->spawn_crack_debris(next_pos, glm::ivec3(0, 1, 0), 0.5f, MAT_VOIDITE_CRYSTAL);
            }
            if (m_audio) {
                m_audio->play_sound_3d(SoundCue::PlasmaHit, next_pos, 0.70f);
            }
            float stress_impact = 2.5f;
            if (m_player->weapon_archetype() == WeaponArchetype::MagmaScattergun) {
                stress_impact = 1.0f;
            } else if (m_player->weapon_archetype() == WeaponArchetype::NeedlerRailgun) {
                stress_impact = 4.0f;
            }
            // Bullet impact acoustic distraction sound event on rock wall
            m_noise_meter.add_bullet_impact_sound(next_pos, stress_impact);
            if (m_hazard) {
                m_hazard->on_weapon_impact(stress_impact);
            }
            continue;
        }

        // 2. Enemy hit detection: Void Stalkers
        if (m_stalkers.damage_nearest(next_pos, 1.4f, bolt.damage)) {
            bolt.active = false;
            if (m_renderer) {
                m_renderer->spawn_crack_debris(next_pos, glm::ivec3(0, 1, 0), 0.8f, MAT_VOIDITE_CRYSTAL);
            }
            if (m_audio) {
                m_audio->play_sound_3d(SoundCue::PlasmaHit, next_pos, 0.95f);
            }
            m_inventory.total_run_score += 15;
            continue;
        }

        // 3. Enemy hit detection: Seismic Burrowers
        if (m_burrowers.damage_nearest(next_pos, 2.4f, bolt.damage, false)) {
            bolt.active = false;
            if (m_renderer) {
                m_renderer->spawn_crack_debris(next_pos, glm::ivec3(0, 1, 0), 0.9f, MAT_TITANIUM);
            }
            if (m_audio) {
                m_audio->play_sound_3d(SoundCue::PlasmaHit, next_pos, 0.95f);
            }
            m_inventory.total_run_score += 25;
            continue;
        }

        bolt.position = next_pos;
    }

    // Erase deactivated bolts
    std::erase_if(m_plasma_bolts, [](const PlayerPlasmaBolt& b) { return !b.active; });

    // 6. Real-Time Spatial Audio Updates
    if (m_audio) {
        m_audio->set_listener(m_player->position(), m_player->forward(), m_player->up());
        int haz_phase = std::clamp(static_cast<int>(m_hazard->radiation_level() / 25.0f), 0, 4);
        m_audio->set_hazard_phase(haz_phase);
        m_audio->set_drill_active(
            m_player->is_drilling(),
            m_player->mine_progress(),
            glm::vec3(m_player->target_block()) + glm::vec3(0.5f)
        );

        // Footsteps
        static float footstep_timer = 0.0f;
        if (m_player->isGrounded()) {
            float horizontal_speed = glm::length(glm::vec2(m_player->velocity().x, m_player->velocity().z));
            if (horizontal_speed > 1.5f) {
                footstep_timer += dt;
                float step_interval = (horizontal_speed > 6.0f) ? 0.32f : 0.45f;
                if (footstep_timer >= step_interval) {
                    footstep_timer = 0.0f;
                    float step_vol = (horizontal_speed > 6.0f) ? 0.35f : 0.28f;
                    m_audio->play_sound_2d(SoundCue::Footstep, step_vol, 0.94f + (rand() % 13) * 0.01f);
                }
            } else {
                footstep_timer = 0.0f;
            }
        } else {
            footstep_timer = 0.0f;
        }

        // Void Stalker 3D Audio Cues (Subtle, scary & atmospheric)
        for (auto& s : m_stalkers.stalkers_mut()) {
            if (s.just_screeched) {
                s.just_screeched = false;
                // Pure directional 3D audio echo without HUD text clutter (leaves the player relying on hearing)
                m_audio->play_sound_3d(SoundCue::StalkerEchoScreech, s.position, 0.90f);
            }
            if (s.just_chittered) {
                s.just_chittered = false;
                // Subtle 3D mandible clicking in the dark (very quiet, creepy stealth cue)
                m_audio->play_sound_3d(SoundCue::StalkerChitter, s.position, 0.45f);
            }
            if (s.just_spotted_player) {
                s.just_spotted_player = false;
                // Predatory throat hiss when stalking delver
                m_audio->play_sound_3d(SoundCue::StalkerHiss, s.position, 0.65f);
            }
            if (s.just_lunged) {
                s.just_lunged = false;
                // Adrenaline spike: duck background ambient drone by 70% during visceral leap attack
                m_audio->trigger_ducking(0.28f, 0.45f, 1.8f);
                m_audio->play_sound_3d(SoundCue::StalkerLunge, s.position, 1.05f);
            }
            if (s.just_hit_player) {
                s.just_hit_player = false;
                m_audio->trigger_ducking(0.35f, 0.30f, 2.2f);
                m_audio->play_sound_2d(SoundCue::StalkerHit, 1.0f);
            }
            if (s.just_died) {
                s.just_died = false;
                m_audio->play_sound_3d(SoundCue::StalkerDie, s.position, 0.70f);
            }
        }
        m_stalkers.remove_dead();

        // Seismic Burrower 3D Audio Cues
        for (auto& b : m_burrowers.burrowers_mut()) {
            if (b.just_roared) {
                b.just_roared = false;
                float dist = glm::distance(b.position, m_player->position());
                std::string loc_str = get_relative_direction_str(m_player->position(), m_player->yaw(), b.position);
                if (dist < 16.0f) {
                    m_hud->show_warning("<< SEISMIC ALERT: BURROWER ROARING " + loc_str + " >>", 2.4f);
                } else {
                    m_hud->show_warning("<< SEISMIC CONTACT: DEEP SUBTERRANEAN BORER " + loc_str + " >>", 2.0f);
                }
                m_audio->play_sound_3d(SoundCue::BurrowerRoar, b.position, 1.05f);
            }
            if (b.just_ground) {
                b.just_ground = false;
                m_audio->play_sound_3d(SoundCue::BurrowerGrind, b.position, 0.85f);
            }
            if (b.just_died) {
                b.just_died = false;
                m_audio->play_sound_3d(SoundCue::VoxelBreakBasalt, b.position, 0.80f);
            }
        }
        m_burrowers.remove_dead();

        // Extraction Beacon Siren Pulse (Clean, spacious harmonic chime every 2.4s)
        if (m_extraction->phase() == ExtractionPhase::BeaconDeployed) {
            static float siren_timer = 0.0f;
            siren_timer += dt;
            if (siren_timer >= 2.4f) {
                siren_timer = 0.0f;
                m_audio->play_sound_3d(SoundCue::BeaconSiren, m_extraction->beacon_position(), 0.85f);
            }
        }

        // Low Health Alarm
        if (m_player->health() < 30.0f && m_player->health() > 0.0f) {
            static float health_alarm_timer = 0.0f;
            health_alarm_timer += dt;
            if (health_alarm_timer >= 1.2f) {
                health_alarm_timer = 0.0f;
                m_audio->play_sound_2d(SoundCue::DamageWarning, 0.65f);
            }
        }

        // ── Room Archetype & Environmental Hazard Acoustics ──
        const RoomPlacement* current_room = nullptr;
        if (m_world && m_world->level_generator()) {
            current_room = m_world->level_generator()->get_room_at_position(m_player->position());
        }
        int room_shape = current_room ? static_cast<int>(current_room->type) : -1;
        m_audio->set_current_room_type(room_shape);

        static int s_last_room_shape = -1;
        if (room_shape != s_last_room_shape && room_shape >= 0) {
            s_last_room_shape = room_shape;
            // Room transition acoustic arrival cue
            m_audio->trigger_room_micro_event(room_shape, 0.45f);
        }

        // Calculate dynamic proximity to hazards
        float lava_dist = 999.0f;
        float spike_dist = 999.0f;
        float void_dist = 999.0f;

        if (current_room) {
            if (current_room->type == RoomShapeType::MagmaCalderaLake) {
                lava_dist = glm::distance(m_player->position(), glm::vec3(current_room->center.x, static_cast<float>(current_room->floor_y), current_room->center.z));
            } else if (current_room->type == RoomShapeType::FaultLineCrevasse) {
                lava_dist = std::abs(m_player->position().x - static_cast<float>(current_room->center.x));
            } else if (current_room->type == RoomShapeType::SpikeTrenchArena) {
                spike_dist = std::max(0.0f, m_player->position().y - (static_cast<float>(current_room->floor_y) + 1.0f));
            } else if (current_room->type == RoomShapeType::VoidSingularityRift) {
                void_dist = glm::distance(m_player->position(), glm::vec3(current_room->center.x, static_cast<float>(current_room->floor_y), current_room->center.z));
            }
        }

        // Check underfoot / vertical drop
        if (m_player->position().y <= 4.0f) {
            void_dist = std::min(void_dist, std::max(0.0f, m_player->position().y - 1.8f));
        }

        int ground_x = static_cast<int>(std::floor(m_player->position().x));
        int ground_y = static_cast<int>(std::floor(m_player->position().y - 0.96f));
        int ground_z = static_cast<int>(std::floor(m_player->position().z));
        Voxel underfoot = m_world->get_voxel(ground_x, ground_y, ground_z);
        if (underfoot.material_id == MAT_THERMITE_SLAG) {
            lava_dist = 0.5f;
        }
        if (underfoot.flags_and_damage == 0x0F) {
            spike_dist = 0.5f;
        }

        float amb_rad_val = m_hazard->radiation_level();
        float prox_rad_val = m_proximity_radiation * 100.0f;
        float rad_val = std::max(amb_rad_val, prox_rad_val);
        m_audio->update_hazard_proximity_audio(dt, lava_dist, rad_val, spike_dist, void_dist);

        // Update player biometrics (cardiac & respiration vitals)
        float health_pct = (m_player->max_health() > 0.0f) ? (m_player->health() / m_player->max_health()) : 0.0f;
        float threat_prox = 0.0f;
        for (const auto& s : m_stalkers.stalkers()) {
            float d = glm::distance(s.position, m_player->position());
            if (d < 12.0f) {
                threat_prox = std::max(threat_prox, 1.0f - (d / 12.0f));
            }
        }
        for (const auto& b : m_burrowers.burrowers()) {
            float d = glm::distance(b.position, m_player->position());
            if (d < 18.0f) {
                threat_prox = std::max(threat_prox, 1.0f - (d / 18.0f));
            }
        }
        m_audio->update_biometrics(health_pct, threat_prox);
    }

    // Low Thruster Fuel Warning (Power < 20%)
    if (m_player->exo().power < 20.0f && m_player->exo().power > 0.0f) {
        static float s_fuel_warn_timer = 0.0f;
        s_fuel_warn_timer += dt;
        if (s_fuel_warn_timer >= 6.0f) {
            s_fuel_warn_timer = 0.0f;
            m_hud->show_warning("! LOW THRUSTER FUEL (< 20%) !", 2.0f);
        }
    }

    // ═══ Escalating Radiation HP Drain ═══
    // Phase 2+: radiation accumulates; above 50% it starts biting.
    // 50-79%: mild (~0.08 * (rad-50) HP/s = 0-2.4 HP/s)
    // 80-94%: urgent (~0.15 * (rad-50) HP/s = 4.5-6.75 HP/s)
    // 95%+:  lethal (5.0 HP/s — death in 16-20s for base class)
    {
        float amb_rad = m_hazard->radiation_level();
        float prox_rad = m_proximity_radiation * 100.0f;
        float rad = std::max(amb_rad, prox_rad);
        m_player->exo_mut().radiation = rad;

        float rad_damage = 0.0f;
        if (rad >= 95.0f) {
            rad_damage = 5.0f;
        } else if (rad >= 80.0f) {
            rad_damage = (rad - 50.0f) * 0.15f;
        } else if (rad >= 50.0f) {
            rad_damage = (rad - 50.0f) * 0.08f;
        }

        if (rad_damage > 0.0f) {
            m_player->take_damage(rad_damage * dt, false);

            // Periodic HUD warning for radiation exposure
            static float s_rad_warn_timer = 0.0f;
            s_rad_warn_timer += dt;
            if (s_rad_warn_timer >= 3.0f) {
                s_rad_warn_timer = 0.0f;
                if (rad >= 95.0f) {
                    m_hud->show_warning("☢ LETHAL RADIATION — EVACUATE IMMEDIATELY ☢", 3.0f);
                } else if (rad >= 80.0f) {
                    m_hud->show_warning("☢ CRITICAL RADIATION EXPOSURE — SUIT FAILING ☢", 2.5f);
                } else if (m_proximity_radiation > 0.45f) {
                    m_hud->show_warning("☢ HIGH PROXIMITY RADIATION — RADIOACTIVE ORE NEARBY ☢", 2.0f);
                } else {
                    m_hud->show_warning("☢ RADIATION ACCUMULATING — CONSIDER EXTRACTION ☢", 2.0f);
                }
            }
        }

        // Legacy exo integrity drain at extreme levels
        if (rad >= 100.0f) {
            m_player->exo_mut().integrity = std::max(0.0f, m_player->exo_mut().integrity - dt * 25.0f);
        }
    }

    // ═══ Gas Pocket Damage ═══
    // Standing inside MAT_GAS voxels corrodes suit integrity and deals HP damage
    {
        glm::vec3 pp = m_player->position();
        int gx = static_cast<int>(std::floor(pp.x));
        int gy = static_cast<int>(std::floor(pp.y));
        int gz = static_cast<int>(std::floor(pp.z));
        Voxel gas_check = m_world->get_voxel(gx, gy, gz);
        Voxel gas_head  = m_world->get_voxel(gx, gy + 1, gz);
        if (gas_check.material_id == MAT_GAS || gas_head.material_id == MAT_GAS) {
            m_player->take_damage(3.5f * dt, false); // 3.5 HP/sec in gas
            m_player->exo_mut().integrity = std::max(0.0f, m_player->exo().integrity - 8.0f * dt);
            static float s_gas_warn_timer = 0.0f;
            s_gas_warn_timer += dt;
            if (s_gas_warn_timer >= 2.5f) {
                s_gas_warn_timer = 0.0f;
                m_hud->show_warning("⚠ TOXIC GAS — SUIT CORRODING ⚠", 2.0f);
            }
        }
    }

    // ═══ Extraction Holdout Wave Escalation ═══
    // During the 40s beacon countdown, spawn increasingly dangerous waves
    if (m_extraction->phase() == ExtractionPhase::BeaconDeployed) {
        float cd = m_extraction->countdown();
        // T=30s mark: 2 flanking stalkers
        if (cd <= 30.0f && cd > 29.5f) {
            int wave_count = 2 + (m_selected_level >= 2 ? 1 : 0);
            m_stalkers.spawn_wave(m_player->position(), wave_count, *m_world);
            m_hud->show_warning("! EXTRACTION HOLDOUT: HOSTILE REINFORCEMENTS !", 2.5f);
            m_player->add_trauma(0.2f);
        }
        // T=15s mark: 3 stalkers + Seismic Burrower threat
        if (cd <= 15.0f && cd > 14.5f) {
            int wave_count = 3 + (m_selected_level >= 2 ? 2 : 0);
            m_stalkers.spawn_wave(m_player->position(), wave_count, *m_world);
            if (m_burrowers.active_count() == 0) {
                m_burrowers.spawn_hazard_wave(m_player->position(), *m_world);
            }
            m_hud->show_warning("!! CRITICAL WAVE & SEISMIC BURROWER INCOMING !!", 3.5f);
            m_player->add_trauma(0.40f);
            if (m_audio) {
                m_audio->play_sound_2d(SoundCue::SeismicTremor, 0.65f);
            }
        }
        // T=5s mark: seismic tremor + final push
        if (cd <= 5.0f && cd > 4.5f) {
            m_hazard->force_tremor(1.5f);
            m_hud->show_warning("!!! FINAL SURGE — EVAC POD INBOUND !!!", 3.0f);
        }
    }

    // Check Level 3 strict countdown
    if (m_selected_level == 3) {
        m_level3_timer -= dt;
        if (m_level3_timer <= 0.0f && !m_death_sequence) {
            m_death_sequence = true;
            m_death_timer = 0.0f;
            m_inventory.suit_failed = true;
            m_expedition_success = false;
            m_trauma = 1.0f;
            if (m_audio) {
                m_audio->play_sound_2d(SoundCue::DamageWarning, 1.0f);
            }
            m_hud->show_warning("TECTONIC COLLAPSE PROTOCOL EXPIRED // DELVER M.I.A.", 4.0f);
        }
    }

    // Check player health / suit integrity failure
    if (m_player->exo().integrity <= 0.0f && !m_death_sequence) {
        m_death_sequence = true;
        m_death_timer = 0.0f;
        m_inventory.suit_failed = true;
        m_expedition_success = false;
        m_trauma = 1.0f;
        if (m_audio) {
            m_audio->play_sound_2d(SoundCue::DamageWarning, 1.0f);
        }
        std::string death_cause = "SUIT INTEGRITY COMPROMISED // DELVER M.I.A.";
        if (m_player->is_in_lava()) {
            death_cause = "INCINERATED // DELVER CONSUMED BY THERMITE SLAG";
        } else if (m_player->is_in_spikes()) {
            death_cause = "IMPALED // FATAL PUNCTURE TRAUMA IN SPIKE TRENCH";
        } else if (m_player->position().y <= 2.2f) {
            death_cause = "CRUSHED // CONSUMED BY ABYSSAL VOID SINGULARITY";
        } else if (m_player->exo().radiation >= 95.0f) {
            death_cause = "LETHAL RADIATION POISONING // ACUTE IONIZATION SICKNESS";
        }
        m_hud->show_warning(death_cause, 5.0f);
        VF_LOG_INFO("Gameplay", "Player Delver died: " << death_cause);
    }

    // 5. Stream chunks around player
    m_world->update(m_player->position(), 2);
}

void Application::render(float dt) {
    if (m_renderer) {
        m_renderer->set_brightness(m_settings.brightness);
    }
    m_world->upload_dirty_chunks();

    glm::mat4 view;
    glm::vec3 cam_pos;

    if (m_state == GameState::Gameplay || m_state == GameState::Paused) {
        view = m_player->get_view_matrix();
        cam_pos = m_player->position();

        // Camera view setup with non-linear impulse trauma screen shake (clamped to prevent wall clipping)
        if (m_trauma > 0.001f) {
            float shake_amount = std::min(m_trauma * m_trauma * 0.35f, 0.15f);
            float shake_x = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * shake_amount;
            float shake_y = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * shake_amount;
            view = glm::translate(view, glm::vec3(shake_x, shake_y, 0.0f));
            if (m_state == GameState::Gameplay) {
                m_trauma = (std::max)(0.0f, m_trauma - dt * 0.8f);
            }
        }

        // Cinematic camera slump and floor impact during Death Sequence
        if (m_death_sequence) {
            float t = std::clamp(m_death_timer / DEATH_SEQUENCE_DURATION, 0.0f, 1.0f);
            float slump_y = -0.75f * std::min(1.0f, t * 1.8f);
            float tilt_angle = glm::radians(35.0f * std::min(1.0f, t * 1.5f));
            view = glm::rotate(view, tilt_angle, glm::vec3(0.0f, 0.0f, 1.0f));
            view = glm::rotate(view, glm::radians(25.0f * std::min(1.0f, t * 1.2f)), glm::vec3(1.0f, 0.0f, 0.0f));
            view = glm::translate(view, glm::vec3(0.0f, slump_y, 0.0f));
        }

        m_renderer->headlamp().position = m_player->position();
        m_renderer->headlamp().direction = m_player->forward();

        // Atmospheric Headlamp Flicker under tectonic tremors or critical suit health
        float health_pct = (m_player->max_health() > 0.0f) ? (m_player->health() / m_player->max_health()) : 0.0f;
        float flicker = 0.0f;
        if (m_hazard && m_hazard->is_tremoring()) {
            flicker = std::max(flicker, 0.45f + 0.35f * m_hazard->tremor_intensity());
        }
        if (health_pct < 0.30f) {
            flicker = std::max(flicker, (0.30f - health_pct) / 0.30f * 0.70f);
        }
        if (m_death_sequence) {
            flicker = 0.95f;
        }
        m_renderer->set_headlamp_flicker(flicker);
    } else {
        if (m_renderer) {
            m_renderer->set_headlamp_flicker(0.0f);
        }
        // Cinematic rotating cavern background for Main Menu, Orbital Hub, and Debrief
        static float menu_cam_angle = 0.0f;
        menu_cam_angle += dt * 0.15f;
        float radius = 10.0f;
        cam_pos = glm::vec3(16.0f + std::sin(menu_cam_angle) * radius, 22.0f, 16.0f + std::cos(menu_cam_angle) * radius);
        view = glm::lookAt(cam_pos, glm::vec3(16.0f, 18.0f, 16.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_renderer->headlamp().position = cam_pos;
        m_renderer->headlamp().direction = glm::normalize(glm::vec3(16.0f, 18.0f, 16.0f) - cam_pos);
    }

    glm::mat4 proj = Renderer::create_projection(m_window->aspect_ratio(), m_settings.fov);

    // Setup dynamic point lights (Cavern luminaries, extraction beacon rotating siren, hostile stalker eyes)
    m_renderer->clear_point_lights();

    // 1. Prioritized Cavern Luminaries (glowing crystal geodes, magma vents, radioactive cores, vault strobes)
    float current_time = static_cast<float>(glfwGetTime());
    if (m_world && m_world->level_generator()) {
        const auto& cavern_luminaries = m_world->level_generator()->luminaries();
        if (!cavern_luminaries.empty()) {
            struct LuminaryCandidate {
                const CavernLuminary* luminary{nullptr};
                float score{0.0f};
            };
            // Zero dynamic allocations: stack-allocated candidate buffer
            std::array<LuminaryCandidate, 64> candidates;
            size_t count = 0;

            for (const auto& lum : cavern_luminaries) {
                if (count >= candidates.size()) break;
                float d = glm::distance(cam_pos, lum.position);
                float max_range = lum.base_radius * 2.5f;
                if (d < max_range) {
                    float dist_sq = std::max(1.0f, d * d);
                    // Importance is proportional to light power and inverse square of distance
                    float score = (lum.base_intensity * lum.base_radius * lum.base_radius) / dist_sq;
                    candidates[count++] = {&lum, score};
                }
            }

            // Target up to 10 cavern luminary slots (leaving reserve headroom for beacon sirens & active stalkers)
            size_t num_to_add = std::min(count, static_cast<size_t>(10));
            if (num_to_add > 0) {
                std::partial_sort(
                    candidates.begin(),
                    candidates.begin() + num_to_add,
                    candidates.begin() + count,
                    [](const LuminaryCandidate& a, const LuminaryCandidate& b) {
                        return a.score > b.score;
                    }
                );

                for (size_t i = 0; i < num_to_add; ++i) {
                    glm::vec3 col;
                    float rad = 0.0f, intensity = 0.0f;
                    candidates[i].luminary->evaluate(current_time, col, rad, intensity);

                    PointLight pl;
                    pl.position = candidates[i].luminary->position;
                    pl.color = col;
                    pl.radius = rad;
                    pl.intensity = intensity;
                    m_renderer->add_point_light(pl);
                }
            }
        }
    }

    // 2. Extraction Beacon Siren & Landing Pod Lights
    if (m_extraction->phase() == ExtractionPhase::BeaconDeployed || m_extraction->phase() == ExtractionPhase::PodLanded) {
        float rot_time = current_time * 4.0f;
        glm::vec3 siren_offset(std::cos(rot_time) * 1.5f, 2.0f, std::sin(rot_time) * 1.5f);

        PointLight beacon_light;
        beacon_light.position = m_extraction->beacon_position() + siren_offset;
        beacon_light.color = glm::vec3(1.0f, 0.1f, 0.05f); // Emergency rotating red siren
        beacon_light.radius = 28.0f;
        beacon_light.intensity = 5.0f * (0.6f + 0.4f * m_extraction->siren_pulse());
        m_renderer->add_point_light(beacon_light);

        if (m_extraction->phase() == ExtractionPhase::PodLanded) {
            PointLight pod_light;
            pod_light.position = m_extraction->beacon_position() + glm::vec3(0.0f, 3.5f, 0.0f);
            pod_light.color = glm::vec3(0.1f, 0.9f, 1.0f);
            pod_light.radius = 35.0f;
            pod_light.intensity = 4.0f;
            m_renderer->add_point_light(pod_light);
        }
    }

    // Dynamic point lights for stalking/lunging hostiles (max 4 nearest)
    int added_stalker_lights = 0;
    for (const auto& s : m_stalkers.stalkers()) {
        if (s.is_dead() || added_stalker_lights >= 4) continue;
        PointLight sl;
        sl.position = s.position + glm::vec3(0.0f, 0.35f, 0.0f);
        switch (s.state) {
            case StalkerState::Lunging:
                sl.color = glm::vec3(1.0f, 0.08f, 0.12f);
                sl.radius = 9.0f;
                sl.intensity = 3.2f;
                break;
            case StalkerState::Stunned:
                sl.color = glm::vec3(0.2f, 0.85f, 1.0f);
                sl.radius = 8.5f;
                sl.intensity = 2.8f;
                break;
            case StalkerState::Circling:
                sl.color = glm::vec3(0.35f, 1.0f, 0.15f);
                sl.radius = 7.0f;
                sl.intensity = 1.6f;
                break;
            case StalkerState::Fleeing:
                sl.color = glm::vec3(1.0f, 0.55f, 0.1f);
                sl.radius = 5.5f;
                sl.intensity = 1.4f;
                break;
            case StalkerState::Stalking:
                sl.color = glm::vec3(0.12f, 0.95f, 0.35f);
                sl.radius = 6.5f;
                sl.intensity = 1.4f;
                break;
            case StalkerState::Idle:
            default:
                sl.color = glm::vec3(0.75f, 0.1f, 0.95f);
                sl.radius = 5.0f;
                sl.intensity = 1.0f;
                break;
        }
        m_renderer->add_point_light(sl);
        added_stalker_lights++;
    }

    // Begin HDR Frame
    m_renderer->begin_frame(view, proj, cam_pos);

    // Render Chunks
    for (const auto& [pos, chunk] : m_world->chunks()) {
        m_renderer->render_chunk(*chunk);
    }

    // Render Falling Dynamic Debris
    for (const auto& d : m_debris) {
        m_renderer->render_debris(d);
    }

    // Render Void Stalker hostile entities and void spine projectiles
    m_renderer->render_stalkers(m_stalkers.stalkers(), m_stalkers.projectiles());

    // Render Seismic Burrower massive subterranean enemies
    m_renderer->render_burrowers(m_burrowers.burrowers());

    // Render Delver Plasma Bolts
    if (!m_plasma_bolts.empty()) {
        m_renderer->render_plasma_bolts(m_plasma_bolts);
    }

    // Render 3D Delver contractor character model (showcase / remote players)
    if (m_showcase_render_delver) {
        m_renderer->render_delver(m_showcase_delver_pos, m_showcase_delver_yaw, m_showcase_delver_class, static_cast<float>(glfwGetTime()));
    }

    // Render 3D Extraction Beacon (tripod base, light column, drifting rings, rotating siren)
    if (m_extraction->phase() == ExtractionPhase::BeaconDeployed || m_extraction->phase() == ExtractionPhase::PodLanded) {
        bool is_landed = (m_extraction->phase() == ExtractionPhase::PodLanded);
        m_renderer->render_extraction_beacon(m_extraction->beacon_position(), m_extraction->siren_pulse(), static_cast<float>(glfwGetTime()), is_landed);
    }

    // Render Grapple Cable (Module 4)
    if (m_player->grapple().active) {
        glm::vec3 tool_pos = m_player->position() + m_player->forward() * 0.4f + m_player->right() * 0.25f - glm::vec3(0.0f, 0.2f, 0.0f);
        m_renderer->render_grapple_cable(tool_pos, m_player->grapple().anchor_point);
    }

    // Render Volumetric Sonar Ping with X-Ray Wireframe (Module 2)
    if (m_surveying.is_active()) {
        m_renderer->render_sonar_wireframes(m_surveying.surveyed_voxels(), m_surveying.alpha());
    }

    // Render Progressive Block Cracks (Module 4) - only when CrackStage > 0.05f
    if (m_player->crack_stage() > 0.05f && m_player->target_block() != glm::ivec3(-1)) {
        Voxel v = m_world->get_voxel(m_player->target_block().x, m_player->target_block().y, m_player->target_block().z);
        m_renderer->render_block_cracks(m_player->target_block(), m_player->crack_stage(), m_player->target_normal(), v.material_id);
    }

    // Render 1st-Person Drill & Animated Hands Viewmodel (Module 3)
    bool render_vm = (m_state == GameState::Gameplay && m_viewmodel);
    if (m_config.capture_models) {
        // In model showcase, only render viewmodel during its dedicated slot (Frame 29..43)
        render_vm = (m_auto_test_frame >= 29 && m_auto_test_frame <= 43);
    }
    if (m_config.capture_level_shapes) {
        render_vm = false; // Suppress viewmodel for unobstructed panoramic room architecture views
    }
    if (render_vm) {
        float aspect = static_cast<float>(m_window->width()) / std::max(1.0f, static_cast<float>(m_window->height()));
        bool is_drilling = m_player->is_drilling();
        bool is_in_range = (m_player->target_block() != glm::ivec3(-1));
        glm::vec3 drill_target = is_in_range ?
            (glm::vec3(m_player->target_block()) + glm::vec3(0.5f) + glm::vec3(m_player->target_normal()) * 0.5f) :
            (m_player->position() + m_player->forward() * 2.0f);

        m_viewmodel->render(
            dt,
            aspect,
            is_drilling,
            is_in_range,
            m_player->active_tool(),
            drill_target,
            m_player->is_firing_weapon()
        );
    }

    // End HDR Frame & Post-Processing Tonemap
    m_renderer->end_frame(dt, m_hazard->radiation_level());

    // Mouse coordinates and clicks for responsive menus
    glm::dvec2 cur_pos = m_window->get_cursor_pos();
    float mouse_x = static_cast<float>(cur_pos.x);
    float mouse_y = static_cast<float>(cur_pos.y);
    bool mouse_down = m_window->is_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT);
    bool mouse_clicked = mouse_down && !m_mouse_down_last;
    m_mouse_down_last = mouse_down;

    // Render State-Specific Overlay
    if (m_state == GameState::MainMenu) {
        bool has_save = (m_user_profile.total_exp > 0 ||
                         m_user_profile.sector_records[1].highest_completion_rate > 0 ||
                         m_user_profile.sector_records[2].highest_completion_rate > 0 ||
                         m_user_profile.sector_records[3].highest_completion_rate > 0);
        MainMenuAction action = m_hub_ui->render_main_menu(m_selected_level, m_user_profile, has_save, mouse_x, mouse_y, mouse_clicked);
        m_settings = m_user_profile.settings;
        sync_audio_settings();

        if (m_hub_ui->check_and_clear_test_sound() && m_audio) {
            m_audio->play_sound_2d(SoundCue::UIBlip, 1.0f);
        }

        if (action == MainMenuAction::Continue) {
            start_expedition(m_selected_level);
        } else if (action == MainMenuAction::NewExpedition) {
            start_expedition(m_selected_level);
        } else if (action == MainMenuAction::UpgradeTerminal) {
            m_hub_ui->set_subview(MenuSubView::Upgrades);
        } else if (action == MainMenuAction::Settings) {
            m_hub_ui->set_subview(MenuSubView::Settings);
        } else if (action == MainMenuAction::Exit) {
            SaveSystem::save_profile(m_user_profile, m_active_save_file);
            glfwSetWindowShouldClose(m_window->handle(), GLFW_TRUE);
        }
    } else if (m_state == GameState::OrbitalHub) {
        // Redundant staging wall bypassed - route directly to MainMenu
        m_hub_ui->set_subview(MenuSubView::Main);
        m_state = GameState::MainMenu;
    } else if (m_state == GameState::Debrief) {
        if (m_debrief_input_lock > 0.0f) {
            m_debrief_input_lock -= dt;
            mouse_clicked = false; // Block accidental skip/clicks on death report
        }
        DebriefAction action = m_hub_ui->render_debrief(m_expedition_success, m_selected_level, m_inventory, m_skills, m_user_profile, mouse_x, mouse_y, mouse_clicked);
        if (action == DebriefAction::LaunchNextSector) {
            SaveSystem::save_profile(m_user_profile, m_active_save_file);
            if (m_expedition_success) {
                m_selected_level = std::min(3, m_selected_level + 1);
            }
            start_expedition(m_selected_level);
        } else if (action == DebriefAction::ReturnToHub) {
            SaveSystem::save_profile(m_user_profile, m_active_save_file);
            m_hub_ui->set_subview(MenuSubView::Main);
            m_state = GameState::MainMenu;
            m_window->set_cursor_locked(false);
        }
    } else if (m_state == GameState::Paused) {
        // Render Pause Menu Modal directly over the darkened frozen 3D scene (no HUD text bleed-through)
        PauseMenuAction action = m_pause_menu->render(
            m_selected_level,
            m_expedition_time,
            m_inventory,
            m_settings,
            mouse_x,
            mouse_y,
            mouse_clicked
        );

        m_user_profile.settings = m_settings;
        sync_audio_settings();

        if (m_pause_menu->check_and_clear_test_sound() && m_audio) {
            m_audio->play_sound_2d(SoundCue::UIBlip, 1.0f);
        }
        if (action != PauseMenuAction::None && m_audio) {
            m_audio->play_sound_2d(SoundCue::UIBlip, 0.8f);
        }

        if (action == PauseMenuAction::Resume) {
            SaveSystem::save_profile(m_user_profile, m_active_save_file);
            m_state = GameState::Gameplay;
            m_window->set_cursor_locked(true);
        } else if (action == PauseMenuAction::Abandon || action == PauseMenuAction::ReturnToStartup || action == PauseMenuAction::ReturnToHub) {
            SaveSystem::save_profile(m_user_profile, m_active_save_file);
            // Reset mission runtime containers (clear active debris entities, reset hazard clocks, flush world delta queues)
            m_debris.clear();
            m_stalkers.reset();
            m_noise_meter.reset();
            if (m_hazard) {
                m_hazard->reset();
            }
            m_trauma = 0.0f;
            m_hud->clear_target_info();
            m_inventory.apply_abandon_penalty();
            m_inventory.finalize_run(m_selected_level, false);
            m_expedition_success = false;

            // Bank remaining salvage into profile
            int run_exp = m_inventory.total_run_score;
            if (run_exp <= 0) {
                run_exp = m_inventory.voidite * 5 + m_inventory.titanium * 6 + m_inventory.salvage_parts * 2;
            }
            int run_coins = m_inventory.run_coins_earned;
            if (run_coins <= 0) {
                run_coins = 50 + m_inventory.run_completion_rate + (m_inventory.voidite * 2 + m_inventory.titanium * 3);
                if (m_inventory.is_abandoned) run_coins /= 2;
            }

            int lvls = 0, bonus = 0;
            m_user_profile.add_exp(run_exp, lvls, bonus);
            m_user_profile.grant_coins(run_coins);
            m_user_profile.total_voidite += m_inventory.voidite;
            m_user_profile.total_titanium += m_inventory.titanium;
            for (int s = 1; s <= 3; ++s) {
                if (m_inventory.sector_records[s].highest_completion_rate > m_user_profile.sector_records[s].highest_completion_rate) {
                    m_user_profile.sector_records[s].highest_completion_rate = m_inventory.sector_records[s].highest_completion_rate;
                }
            }
            SaveSystem::save_profile(m_user_profile, m_active_save_file);

            m_hub_ui->set_subview(MenuSubView::Main);
            m_state = GameState::MainMenu;
            m_window->set_cursor_locked(false);
        }
    } else {
        // Render HUD Overlay (Module 4) - bypass during model showcase captures for clean studio render
        if (!m_config.capture_models && !m_config.capture_level_shapes) {
            if (m_hud) {
                m_hud->set_death_sequence(m_death_sequence, m_death_timer, DEATH_SEQUENCE_DURATION);
            }
            m_hud->render(*m_player, *m_world, *m_hazard, *m_extraction, m_inventory, m_skills, m_selected_level, view, proj, &m_surveying, &m_noise_meter, m_stalkers.active_count());
        }
    }

    if (m_hub_ui->has_profile_changed()) {
        m_hub_ui->clear_profile_changed();
        SaveSystem::save_profile(m_user_profile, m_active_save_file);
        sync_profile_with_player();
    }
}

void Application::grant_testing_resources(int exp, int voidite, int titanium) {
    int coins = exp / 2; // grant proportional coins
    m_user_profile.grant_resources(exp, coins, voidite, titanium);
    SaveSystem::save_profile(m_user_profile, m_active_save_file);
    sync_profile_with_player();

    if (m_hud) {
        m_hud->add_loot_toast("TEST EXP GRANT", glm::vec4(1.0f, 0.85f, 0.2f, 1.0f), exp, 30);
        m_hud->add_loot_toast("TEST COINS", glm::vec4(1.0f, 0.85f, 0.2f, 1.0f), coins, 30);
        m_hud->add_loot_toast("TEST VOIDITE", glm::vec4(0.7f, 0.3f, 1.0f, 1.0f), voidite, 30);
        m_hud->add_loot_toast("TEST TITANIUM", glm::vec4(0.2f, 0.8f, 1.0f, 1.0f), titanium, 30);
        m_hud->show_warning("TESTING: GRANTED " + std::to_string(exp) + " EXP & " + std::to_string(coins) + " COINS");
    }
    VF_LOG_INFO("Testing", "Granted " + std::to_string(exp) + " EXP, " + 
                std::to_string(coins) + " Coins, " +
                std::to_string(voidite) + " Voidite, " + std::to_string(titanium) + 
                " Titanium (Level: " + std::to_string(m_user_profile.get_player_level()) +
                ", New Total: " + std::to_string(m_user_profile.total_exp) + " EXP, " +
                std::to_string(m_user_profile.total_coins) + " Coins).");
}

void Application::test_upgrade_and_respec_cycle() {
    VF_LOG_INFO("Testing", "Executing diagnostic Upgrade & Respec cycle test...");
    int player_lvl = m_user_profile.get_player_level();
    if (m_user_profile.total_coins < 1000) {
        m_user_profile.grant_resources(1500, 1000, 40, 20);
        player_lvl = m_user_profile.get_player_level();
    }

    // Purchase upgrades using coins and player level
    bool b1 = m_user_profile.upgrades.purchase(UpgradeType::DrillSpeed, player_lvl, m_user_profile.total_coins, m_user_profile.total_voidite, m_user_profile.total_titanium);
    bool b2 = m_user_profile.upgrades.purchase(UpgradeType::ThrusterTank, player_lvl, m_user_profile.total_coins, m_user_profile.total_voidite, m_user_profile.total_titanium);
    bool b3 = m_user_profile.upgrades.purchase(UpgradeType::ReinforcedPlating, player_lvl, m_user_profile.total_coins, m_user_profile.total_voidite, m_user_profile.total_titanium);

    int total_spent = m_user_profile.upgrades.get_total_spent_coins();
    VF_LOG_INFO("Testing", "Purchased upgrades: DrillSpeed (T" + std::to_string(m_user_profile.upgrades.drillSpeedTier) + 
                "), ThrusterTank (T" + std::to_string(m_user_profile.upgrades.thrusterTankTier) + 
                "), ReinforcedPlating (T" + std::to_string(m_user_profile.upgrades.reinforcedPlatingTier) + 
                ") [Spent: " + std::to_string(total_spent) + " Coins].");

    // Perform respec (85% recovery)
    int refunded = 0;
    m_user_profile.upgrades.respec(refunded);
    m_user_profile.total_coins += refunded;

    VF_LOG_INFO("Testing", "Respec executed: refunded " + std::to_string(refunded) + " Coins (85% recovery), all tiers reset to 0.");
    SaveSystem::save_profile(m_user_profile, m_active_save_file);
    sync_profile_with_player();

    if (m_hud) {
        m_hud->show_warning("TEST RESPEC: REFUNDED " + std::to_string(refunded) + " COINS");
    }
}

void Application::run() {
    double last_time = glfwGetTime();
    double accumulator = 0.0;
    const double fixed_dt = 1.0 / 60.0;

    std::cout << "\n========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE (TACTICAL VOXEL EXTRACTION)" << std::endl;
    std::cout << "  State Machine Active: Main Menu -> Orbital Hub -> Gameplay -> Debrief" << std::endl;
    std::cout << "========================================================\n" << std::endl;

    if (m_config.test_enemy || m_config.capture_models || m_config.capture_level_shapes) {
        start_expedition(1);
    }

    while (!m_window->should_close()) {
        double current_time = glfwGetTime();
        double frame_time = current_time - last_time;
        if (frame_time > 0.25) frame_time = 0.25;
        last_time = current_time;

        // Halt physics ticks, voxel meshing, and extraction timers during GameState::Paused
        if (m_state != GameState::Paused) {
            accumulator += frame_time;
        }

        m_window->poll_events();

        // Enforce cursor unlock during Menu, Hub, Debrief, and Paused states
        if (m_state != GameState::Gameplay) {
            if (m_window->is_cursor_locked()) {
                m_window->set_cursor_locked(false);
            }
        }

        // 1. MAIN MENU INPUTS
        if (m_state == GameState::MainMenu) {
            static bool enter_down_last = false;
            bool enter_now = m_window->is_key_down(GLFW_KEY_ENTER) ||
                             m_window->is_key_down(GLFW_KEY_SPACE);

            if (enter_now && !enter_down_last) {
                bool has_save = (m_user_profile.total_exp > 0 ||
                                 m_user_profile.sector_records[1].highest_completion_rate > 0 ||
                                 m_user_profile.sector_records[2].highest_completion_rate > 0 ||
                                 m_user_profile.sector_records[3].highest_completion_rate > 0);
                if (has_save) {
                    start_expedition(m_selected_level);
                } else {
                    m_hub_ui->set_active_tab(HubTab::SectorSelect);
                    m_state = GameState::OrbitalHub;
                }
            }
            enter_down_last = enter_now;
        }
        // 2. ORBITAL HUB INPUTS
        else if (m_state == GameState::OrbitalHub) {
            if (m_hub_ui->active_tab() == HubTab::SectorSelect) {
                if (m_window->is_key_down(GLFW_KEY_1)) m_selected_level = 1;
                if (m_window->is_key_down(GLFW_KEY_2)) m_selected_level = 2;
                if (m_window->is_key_down(GLFW_KEY_3)) m_selected_level = 3;
            }

            static bool launch_down_last = false;
            bool launch_now = m_window->is_key_down(GLFW_KEY_ENTER) ||
                              m_window->is_key_down(GLFW_KEY_SPACE);

            if (launch_now && !launch_down_last) {
                if (m_hub_ui->active_tab() == HubTab::SectorSelect) {
                    start_expedition(m_selected_level);
                }
            }
            launch_down_last = launch_now;

            static bool hub_back_down_last = false;
            bool hub_back_now = m_window->is_key_down(GLFW_KEY_TAB);
            if (hub_back_now && !hub_back_down_last) {
                int next_tab = (static_cast<int>(m_hub_ui->active_tab()) + 1) % 3;
                m_hub_ui->set_active_tab(static_cast<HubTab>(next_tab));
            }
            hub_back_down_last = hub_back_now;
        }
        // 3. DEBRIEF INPUTS
        else if (m_state == GameState::Debrief) {
            static bool debrief_down_last = false;
            bool debrief_now = m_window->is_key_down(GLFW_KEY_ENTER) ||
                               m_window->is_key_down(GLFW_KEY_SPACE);

            if (debrief_now && !debrief_down_last) {
                m_selected_level = std::min(3, m_selected_level + 1);
                start_expedition(m_selected_level);
            }
            debrief_down_last = debrief_now;

            static bool hub_back_down_last = false;
            bool hub_back_now = m_window->is_key_down(GLFW_KEY_TAB);
            if (hub_back_now && !hub_back_down_last) {
                m_state = GameState::OrbitalHub;
            }
            hub_back_down_last = hub_back_now;

            static bool retry_down_last = false;
            bool retry_now = m_window->is_key_down(GLFW_KEY_R);
            if (retry_now && !retry_down_last) {
                start_expedition(m_selected_level);
            }
            retry_down_last = retry_now;
        }
        // 4. GAMEPLAY INPUTS
        else if (m_state == GameState::Gameplay) {
            static bool b_down_last = false;
            bool b_now = m_window->is_key_down(GLFW_KEY_B);
            if (b_now && !b_down_last) {
                bool quota_met = (m_selected_level == 2)
                    ? (m_inventory.vault_breached && m_inventory.relic_extracted)
                    : (m_inventory.voidite >= m_inventory.target_voidite);
                if (quota_met) {
                    m_extraction->deploy_beacon(m_player->position());
                    m_surveying.reset(); // Clear any sonar X-ray overlay on beacon deployment
                } else {
                    if (m_hud) {
                        std::string warn = "BEACON LOCKED: REQUIRE " +
                            std::to_string(m_inventory.target_voidite) + " VOIDITE (" +
                            std::to_string(m_inventory.voidite) + "/" + std::to_string(m_inventory.target_voidite) + ")";
                        if (m_selected_level == 2) {
                            warn = "BEACON LOCKED: BREACH VAULT & EXTRACT RELIC FIRST";
                        }
                        m_hud->show_warning(warn, 2.5f);
                    }
                    if (m_audio) {
                        m_audio->play_sound_2d(SoundCue::UIBlip, 0.8f);
                    }
                }
            }
            b_down_last = b_now;

            static bool h_down_last = false;
            bool h_now = m_window->is_key_down(GLFW_KEY_H);
            if (h_now && !h_down_last) {
                m_renderer->headlamp().enabled = !m_renderer->headlamp().enabled;
                if (m_hud) {
                    m_hud->show_warning(m_renderer->headlamp().enabled ? "HEADLAMP: ACTIVE" : "HEADLAMP: DISABLED [BIOLUMINESCENT OBSERVATION MODE]", 2.0f);
                }
            }
            h_down_last = h_now;

            static bool lbrk_down_last = false;
            bool lbrk_now = m_window->is_key_down(GLFW_KEY_LEFT_BRACKET);
            if (lbrk_now && !lbrk_down_last) {
                m_settings.brightness = std::max(0.40f, m_settings.brightness - 0.10f);
                if (m_hud) {
                    m_hud->show_warning("CAVERN BRIGHTNESS: " + std::to_string(static_cast<int>(std::round(m_settings.brightness * 100.0f))) + "%", 1.5f);
                }
            }
            lbrk_down_last = lbrk_now;

            static bool rbrk_down_last = false;
            bool rbrk_now = m_window->is_key_down(GLFW_KEY_RIGHT_BRACKET);
            if (rbrk_now && !rbrk_down_last) {
                m_settings.brightness = std::min(2.00f, m_settings.brightness + 0.10f);
                if (m_hud) {
                    m_hud->show_warning("CAVERN BRIGHTNESS: " + std::to_string(static_cast<int>(std::round(m_settings.brightness * 100.0f))) + "%", 1.5f);
                }
            }
            rbrk_down_last = rbrk_now;

            static bool tab_down_last = false;
            bool tab_now = m_window->is_key_down(GLFW_KEY_TAB);
            if (tab_now && !tab_down_last) {
                m_window->set_cursor_locked(!m_window->is_cursor_locked());
            }
            m_player->set_mouse_sensitivity(m_settings.mouse_sensitivity);
            m_player->handle_input(*m_window, static_cast<float>(frame_time));
        }

        while (accumulator >= fixed_dt) {
            fixed_tick(static_cast<float>(fixed_dt));
            accumulator -= fixed_dt;
        }

        render(static_cast<float>(frame_time));

        // ── Single screenshot CLI flag ──
        if (!m_config.single_screenshot_path.empty() && m_auto_test_frame >= 5) {
            capture_screenshot_png(m_config.single_screenshot_path, m_window->width(), m_window->height());
            break;
        }

        // ── Testing Hotkeys (Available ONLY in Test Mode via --test, --test-save, or --auto-play-test) ──
        if (is_test_mode()) {
            static bool f5_down_last = false;
            bool f5_now = m_window->is_key_down(GLFW_KEY_F5);
            if (f5_now && !f5_down_last) {
                grant_testing_resources(1000, 25, 10);
            }
            f5_down_last = f5_now;

            static bool f6_down_last = false;
            bool f6_now = m_window->is_key_down(GLFW_KEY_F6);
            if (f6_now && !f6_down_last) {
                test_upgrade_and_respec_cycle();
            }
            f6_down_last = f6_now;

            static bool f7_down_last = false;
            bool f7_now = m_window->is_key_down(GLFW_KEY_F7);
            if (f7_now && !f7_down_last && m_hazard) {
                m_hazard->force_tremor(1.5f);
                if (m_hud) m_hud->show_warning("TESTING: FORCED SEISMIC TREMOR");
                VF_LOG_INFO("Testing", "Forced seismic tremor triggered via F7 hotkey.");
            }
            f7_down_last = f7_now;

            static bool f8_down_last = false;
            bool f8_now = m_window->is_key_down(GLFW_KEY_F8);
            if (f8_now && !f8_down_last && m_extraction) {
                m_extraction->force_evacuation_pod();
                if (m_hud) m_hud->show_warning("TESTING: EVAC POD TOUCHDOWN FORCED");
                VF_LOG_INFO("Testing", "Forced evacuation pod touchdown via F8 hotkey.");
            }
            f8_down_last = f8_now;

            static bool f9_down_last = false;
            bool f9_now = m_window->is_key_down(GLFW_KEY_F9);
            if (f9_now && !f9_down_last && m_extraction) {
                m_extraction->force_escape();
                VF_LOG_INFO("Testing", "Forced escape evacuation via F9 hotkey.");
            }
            f9_down_last = f9_now;
        }

        // ── In-Game F12 Screenshot Hotkey ──
        static bool f12_down_last = false;
        bool f12_now = m_window->is_key_down(GLFW_KEY_F12);
        if (f12_now && !f12_down_last) {
            auto now = std::chrono::system_clock::now();
            auto t = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf{};
#ifdef _WIN32
            localtime_s(&tm_buf, &t);
#else
            localtime_r(&t, &tm_buf);
#endif
            char buf[128];
            std::strftime(buf, sizeof(buf), "screenshots/screenshot_%Y%m%d_%H%M%S.png", &tm_buf);
            capture_screenshot_png(buf, m_window->width(), m_window->height());
        }
        f12_down_last = f12_now;

        // ── Automated Visual Test & Playthrough Sequence ──
        if (m_config.auto_play_test) {
            m_auto_test_frame++;

            // Frame 10: Capture Main Menu
            if (m_auto_test_frame == 10) {
                VisualTestHarness::instance().record_phase(0, "MAIN MENU", "screenshots/01_main_menu.png", m_window->width(), m_window->height());
                // Switch to Sector Select carousel (Level Selection)
                m_hub_ui->set_subview(MenuSubView::SectorSelect);
                m_selected_level = 2; // Preview Sector 2 card
            }
            // Frame 20: Capture Level Selection (Sector Select Carousel)
            else if (m_auto_test_frame == 20) {
                VisualTestHarness::instance().record_phase(1, "LEVEL SELECT", "screenshots/02_level_select.png", m_window->width(), m_window->height());
                // Switch to Character Selection (Delver Roster)
                m_hub_ui->set_subview(MenuSubView::Upgrades);
                m_hub_ui->set_active_tab(HubTab::DelverRoster);
            }
            // Frame 30: Capture Character Selection (Delver Roster: Demolitionist, Vanguard, Scout)
            else if (m_auto_test_frame == 30) {
                VisualTestHarness::instance().record_phase(2, "CHAR SELECT", "screenshots/03_character_select.png", m_window->width(), m_window->height());
                // Switch to Upgrade Terminal tab
                m_hub_ui->set_active_tab(HubTab::UpgradeTerminal);
            }
            // Frame 33: Testing: Grant EXP and purchase upgrades
            else if (m_auto_test_frame == 33) {
                grant_testing_resources(1500, 30, 15);
                int lvl = m_user_profile.get_player_level();
                m_user_profile.upgrades.purchase(UpgradeType::DrillSpeed, lvl, m_user_profile.total_coins, m_user_profile.total_voidite, m_user_profile.total_titanium);
                m_user_profile.upgrades.purchase(UpgradeType::ThrusterTank, lvl, m_user_profile.total_coins, m_user_profile.total_voidite, m_user_profile.total_titanium);
                VF_LOG_INFO("AutoPlayTest", "Testing: Granted resources, purchased Tier 1 DrillSpeed & ThrusterTank with Coins.");
            }
            // Frame 36: Testing: Test respec points feature (85% recovery verification)
            else if (m_auto_test_frame == 36) {
                int refunded = 0;
                m_user_profile.upgrades.respec(refunded);
                m_user_profile.total_coins += refunded;
                VF_LOG_INFO("AutoPlayTest", "Testing: Respec verified, refunded " + std::to_string(refunded) + " Coins (85% recovery).");
                // Re-purchase upgrades to showcase active purchased upgrade nodes in the terminal UI screenshot
                int lvl = m_user_profile.get_player_level();
                m_user_profile.upgrades.purchase(UpgradeType::DrillSpeed, lvl, m_user_profile.total_coins, m_user_profile.total_voidite, m_user_profile.total_titanium);
                m_user_profile.upgrades.purchase(UpgradeType::KineticDynamo, lvl, m_user_profile.total_coins, m_user_profile.total_voidite, m_user_profile.total_titanium);
                m_user_profile.upgrades.purchase(UpgradeType::ReinforcedPlating, lvl, m_user_profile.total_coins, m_user_profile.total_voidite, m_user_profile.total_titanium);
            }
            // Frame 40: Capture Upgrade Terminal (with active upgrades & granted resources)
            else if (m_auto_test_frame == 40) {
                VisualTestHarness::instance().record_phase(3, "UPGRADES TERMINAL", "screenshots/04_upgrades_terminal.png", m_window->width(), m_window->height());
                // Launch Sector 1 Expedition
                m_hub_ui->set_subview(MenuSubView::Main);
                start_expedition(1);
            }
            // Frames 45 to 74: Player walks around, tests sprint and thruster hover
            else if (m_auto_test_frame >= 45 && m_auto_test_frame < 75) {
                if (m_auto_test_frame == 45) {
                    m_player->set_look_angles(-90.0f, 0.0f);
                }
                // Forward movement with surface clamping
                if (m_auto_test_frame <= 60) {
                    m_player->set_position(m_player->position() + glm::vec3(0.0f, 0.0f, 0.12f));
                    m_player->clamp_to_surface(*m_world);
                    // Exercise thrusters and power drain
                    m_player->exo_mut().power = std::max(10.0f, m_player->exo_mut().power - 1.2f);
                    if (m_auto_test_frame % 5 == 0) {
                        m_renderer->trigger_dust_kickup(1.2f);
                    }
                } else {
                    // Strafe slightly right
                    m_player->set_position(m_player->position() + glm::vec3(0.06f, 0.0f, 0.04f));
                    m_player->clamp_to_surface(*m_world);
                }
            }
            // Frame 75: Settle in gameplay cavern and capture viewmodel + HUD
            else if (m_auto_test_frame == 75) {
                m_player->set_look_angles(0.0f, -15.0f);
                m_world->update(m_player->position(), 2);
                VisualTestHarness::instance().record_phase(4, "GAMEPLAY CAVERN", "screenshots/05_gameplay_cavern.png", m_window->width(), m_window->height());
            }
            // Frames 80 to 105: Player mines the cavern rock directly in front with drill
            else if (m_auto_test_frame >= 80 && m_auto_test_frame <= 105) {
                m_player->set_active_tool(ToolSlot::MiningDrill);
                m_player->set_look_angles(0.0f, -22.0f);
                m_player->set_drilling(true);
                m_player->MineBlock(static_cast<float>(fixed_dt), *m_world);

                // Testing: Force seismic tremor to evaluate tremor warning, screen trauma, and ceiling debris
                if (m_auto_test_frame == 95) {
                    m_hazard->force_tremor(1.4f);
                    m_stalkers.spawn_stalker(m_player->position() + glm::vec3(-2.0f, 0.2f, 5.5f));
                    VF_LOG_INFO("AutoPlayTest", "Testing: Forced seismic tremor triggered & spawned predatory Void Stalker.");
                }

                if (m_auto_test_frame == 105) {
                    VisualTestHarness::instance().record_phase(5, "DRILL CRACKS", "screenshots/06_drilling_cracks.png", m_window->width(), m_window->height());
                }
            }
            // Frame 106: Stop drilling
            else if (m_auto_test_frame == 106) {
                m_player->set_drilling(false);
            }
            // Frame 112: Test Ability 1 - Seismic Sonar Pulse scan
            else if (m_auto_test_frame == 112) {
                on_sonar_cast(m_player->position());
            }
            // Frame 118: Test Tool 2 - Industrial Bulkhead placement
            else if (m_auto_test_frame == 118) {
                m_player->set_active_tool(ToolSlot::IndustrialBulkhead);
                glm::ivec3 place_pos = glm::ivec3(std::floor(m_player->position().x), std::floor(m_player->position().y) + 1, std::floor(m_player->position().z) + 3);
                m_world->set_voxel(place_pos.x, place_pos.y, place_pos.z, Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_PLAYER_PLACED}, true);
                on_block_placed(place_pos.x, place_pos.y, place_pos.z, MAT_INDUSTRIAL_BULKHEAD);
            }
            // Frame 124: Test Tool 3 - Demolition Shaped Charge explosive micro-blast
            else if (m_auto_test_frame == 124) {
                m_player->set_active_tool(ToolSlot::DemolitionCharge);
                glm::ivec3 blast_pos = glm::ivec3(std::floor(m_player->position().x), std::floor(m_player->position().y) + 1, std::floor(m_player->position().z) + 4);
                on_explosive_blast(blast_pos, glm::ivec3(0, 0, 1), true);
            }
            // Frame 130: Test Ability 2 - Grapple Hook cable tension
            else if (m_auto_test_frame == 130) {
                glm::vec3 anchor = m_player->position() + glm::vec3(0.0f, 5.0f, 6.0f);
                m_player->grapple_mut().active = true;
                m_player->grapple_mut().anchor_point = anchor;
                m_player->grapple_mut().rest_length = glm::distance(m_player->position(), anchor);
            }
            // Frame 136: Release grapple hook and equip combat weapon: Plasma Carbine!
            else if (m_auto_test_frame == 136) {
                m_player->grapple_mut().active = false;
                m_player->set_active_tool(ToolSlot::PlasmaCarbine);
            }
            // Frame 138: Fire Combat Weapon projectile and display loot toasts
            else if (m_auto_test_frame == 138) {
                m_player->set_drilling(true);
                m_player->try_fire_weapon(m_plasma_bolts, static_cast<float>(fixed_dt));
                m_player->set_drilling(false);
                m_hud->add_loot_toast("VOIDITE CRYSTAL", glm::vec4(0.7f, 0.3f, 1.0f, 1.0f), 3, 45);
                m_hud->add_loot_toast("SCRAP METAL", glm::vec4(0.85f, 0.7f, 0.3f, 1.0f), 2, 20);
                m_inventory.add_voidite(3);
                m_inventory.add_titanium(2);
            }
            else if (m_auto_test_frame == 142) {
                m_hud->add_loot_toast("VOIDITE CRYSTAL", glm::vec4(0.7f, 0.3f, 1.0f, 1.0f), 3, 45); // Stack +3 [x2]
                m_inventory.add_voidite(3);
            }
            else if (m_auto_test_frame == 146) {
                VisualTestHarness::instance().record_phase(6, "ABILITIES & LOOT", "screenshots/07_abilities_loot.png", m_window->width(), m_window->height());
            }
            // Frame 150: Pause Game (In-Game Esc Menu)
            else if (m_auto_test_frame == 150) {
                m_state = GameState::Paused;
                m_window->set_cursor_locked(false);
                if (m_pause_menu) m_pause_menu->set_active_tab(PauseTab::Mission);
            }
            // Frame 154: Capture Esc / Pause Menu Modal (Tab 1 Telemetry)
            else if (m_auto_test_frame == 154) {
                VisualTestHarness::instance().record_phase(7, "ESC MENU", "screenshots/08_esc_menu.png", m_window->width(), m_window->height());
                if (m_pause_menu) m_pause_menu->set_active_tab(PauseTab::AudioSettings);
            }
            // Frame 156: Capture Esc / Pause Menu Modal (Tab 2 Audio & Rig)
            else if (m_auto_test_frame == 156) {
                capture_screenshot_png("screenshots/08b_esc_audio_tab.png", m_window->width(), m_window->height());
                if (m_pause_menu) m_pause_menu->set_active_tab(PauseTab::ControlsBriefing);
            }
            // Frame 158: Capture Esc / Pause Menu Modal (Tab 3 Controls & Directives)
            else if (m_auto_test_frame == 158) {
                capture_screenshot_png("screenshots/08c_esc_controls_tab.png", m_window->width(), m_window->height());
            }
            // Frame 160: Resume gameplay from pause and deploy extraction beacon
            else if (m_auto_test_frame == 160) {
                m_state = GameState::Gameplay;
                m_window->set_cursor_locked(true);
                m_extraction->deploy_beacon(m_player->position());
                m_surveying.reset(); // Clear sonar X-ray so beacon screenshot is clean
            }
            // Frame 174: Capture Extraction Beacon Holdout
            else if (m_auto_test_frame == 174) {
                VisualTestHarness::instance().record_phase(8, "EXTRACTION BEACON", "screenshots/09_extraction_beacon.png", m_window->width(), m_window->height());
            }
            // Frame 178: Testing: Force escape evacuation -> triggers complete safe evac into Debrief
            else if (m_auto_test_frame == 178) {
                VF_LOG_INFO("AutoPlayTest", "Testing: Forcing escape evacuation to evaluate debrief results.");
                m_extraction->force_escape();
            }
            // Frame 188: Capture Mission Debrief Screen
            else if (m_auto_test_frame == 188) {
                VisualTestHarness::instance().record_phase(9, "MISSION DEBRIEF", "screenshots/10_mission_debrief.png", m_window->width(), m_window->height());
            }
            // Frame 198: Finalize Visual Test Harness & Exit Cleanly
            else if (m_auto_test_frame >= 198) {
                VisualTestHarness::instance().finalize();
                VF_LOG_INFO("AutoPlayTest", "Extensive visual test completed! All 10 screens & montage captured.");
                break;
            }
        }

        // ── Automated Dedicated Void Stalker Visual Test Suite (--test-enemy) ──
        if (m_config.test_enemy) {
            m_auto_test_frame++;

            // Ensure player view is stable and looking toward stalker (+Z is yaw=90)
            if (m_auto_test_frame == 1) {
                m_player->set_position(glm::vec3(16.0f, 22.0f, 16.0f));
                m_player->clamp_to_surface(*m_world);
                m_player->set_look_angles(90.0f, -4.0f);
                m_renderer->headlamp().enabled = false;
                m_stalkers.reset();
                m_stalkers.spawn_stalker(m_player->position() + glm::vec3(0.0f, -0.1f, 3.6f));
                if (!m_stalkers.stalkers().empty()) {
                    auto& s = m_stalkers.stalkers_mut()[0];
                    s.state = StalkerState::Stalking;
                    s.target_pos = m_player->position();
                    s.yaw = 3.14159f; // Facing player (-Z)
                }
                VF_LOG_INFO("EnemyTest", "Staged Void Stalker in deep shadow for Slot 0.");
            }
            // Frame 12: Slot 0 "01_SHADOW_STALK"
            else if (m_auto_test_frame == 12) {
                VisualTestHarness::instance().record_enemy_phase(0, "SHADOW STALK", "screenshots/enemy_01_shadow_stalk.png", m_window->width(), m_window->height());
                // Switch headlamp ON for reveal
                m_renderer->headlamp().enabled = true;
                if (!m_stalkers.stalkers().empty()) {
                    auto& s = m_stalkers.stalkers_mut()[0];
                    s.position = m_player->position() + glm::vec3(0.0f, -0.1f, 2.9f);
                    s.yaw = 3.14159f;
                }
                m_player->set_look_angles(90.0f, -4.0f);
                VF_LOG_INFO("EnemyTest", "Headlamp ON: Revealing predatory chitin details for Slot 1.");
            }
            // Frame 24: Slot 1 "02_HEADLAMP_REVEAL"
            else if (m_auto_test_frame == 24) {
                VisualTestHarness::instance().record_enemy_phase(1, "HEADLAMP REVEAL", "screenshots/enemy_02_headlamp_reveal.png", m_window->width(), m_window->height());
                // Shift to Circling Flank
                if (!m_stalkers.stalkers().empty()) {
                    auto& s = m_stalkers.stalkers_mut()[0];
                    s.state = StalkerState::Circling;
                    s.position = m_player->position() + glm::vec3(1.8f, -0.1f, 2.5f);
                    s.target_pos = m_player->position();
                    s.yaw = 3.8f;
                }
                m_player->set_look_angles(55.0f, -4.0f); // Turn to flank
                VF_LOG_INFO("EnemyTest", "Stalker circling flank for Slot 2.");
            }
            // Frame 38: Slot 2 "03_CIRCLING_FLANK"
            else if (m_auto_test_frame == 38) {
                VisualTestHarness::instance().record_enemy_phase(2, "CIRCLING FLANK", "screenshots/enemy_03_circling_flank.png", m_window->width(), m_window->height());
                // Shift to Lunging Attack
                if (!m_stalkers.stalkers().empty()) {
                    auto& s = m_stalkers.stalkers_mut()[0];
                    s.state = StalkerState::Lunging;
                    s.position = m_player->position() + glm::vec3(0.0f, 0.15f, 1.8f);
                    s.target_pos = m_player->position();
                    s.yaw = 3.14159f;
                    s.just_lunged = true;
                }
                m_player->set_look_angles(90.0f, -2.0f);
                VF_LOG_INFO("EnemyTest", "Stalker lunging attack for Slot 3.");
            }
            // Frame 50: Slot 3 "04_AGGRESSIVE_LUNGE"
            else if (m_auto_test_frame == 50) {
                VisualTestHarness::instance().record_enemy_phase(3, "AGGRESSIVE LUNGE", "screenshots/enemy_04_aggressive_lunge.png", m_window->width(), m_window->height());
                // Cast Sonar Pulse -> triggers stun
                on_sonar_cast(m_player->position());
                if (!m_stalkers.stalkers().empty()) {
                    auto& s = m_stalkers.stalkers_mut()[0];
                    s.state = StalkerState::Stunned;
                    s.position = m_player->position() + glm::vec3(0.0f, -0.1f, 2.3f);
                    s.yaw = 3.14159f;
                    s.stun_timer = 3.0f;
                }
                m_player->set_look_angles(90.0f, -4.0f);
                VF_LOG_INFO("EnemyTest", "Sonar shockwave applied, stalker stunned for Slot 4.");
            }
            // Frame 62: Slot 4 "05_SONAR_STUN"
            else if (m_auto_test_frame == 62) {
                VisualTestHarness::instance().record_enemy_phase(4, "SONAR STUN", "screenshots/enemy_05_sonar_stun.png", m_window->width(), m_window->height());
                // Damage stalker and transition to Fleeing
                if (!m_stalkers.stalkers().empty()) {
                    auto& s = m_stalkers.stalkers_mut()[0];
                    s.hp = 8.0f;
                    s.state = StalkerState::Fleeing;
                    s.position = m_player->position() + glm::vec3(0.5f, -0.1f, 3.6f);
                    s.target_pos = m_player->position();
                    s.yaw = 0.0f; // Turned around, fleeing away down the corridor
                }
                m_player->set_look_angles(85.0f, -4.0f);
                VF_LOG_INFO("EnemyTest", "Stalker damaged & retreating for Slot 5.");
            }
            // Frame 74: Slot 5 "06_RETREAT_FLEEING"
            else if (m_auto_test_frame == 74) {
                VisualTestHarness::instance().record_enemy_phase(5, "RETREAT FLEEING", "screenshots/enemy_06_retreat_fleeing.png", m_window->width(), m_window->height());
            }
            // Frame 82: Finalize dedicated enemy visual test harness & exit
            else if (m_auto_test_frame >= 82) {
                VisualTestHarness::instance().finalize_enemy_test();
                VF_LOG_INFO("EnemyTest", "Dedicated Void Stalker visual test harness finalized! All 6 phases & montage generated.");
                break;
            }
        }

        // ── Automated Character & Enemy 3D Model Showcase Capture (--capture-models) ──
        if (m_config.capture_models) {
            m_auto_test_frame++;

            // Ensure stable camera and pristine studio lighting
            if (m_auto_test_frame == 1) {
                m_player->set_position(glm::vec3(16.0f, 22.0f, 16.0f));
                m_player->clamp_to_surface(*m_world);
                m_player->set_look_angles(90.0f, -3.0f); // Looking towards +Z
                m_renderer->headlamp().enabled = true;
                m_renderer->headlamp().intensity = 2.8f;
                m_renderer->clear_point_lights();
                // Three-point studio lighting
                m_renderer->add_point_light({m_player->position() + glm::vec3(1.4f, 1.8f, 1.6f), glm::vec3(1.0f, 0.95f, 0.90f), 18.0f, 2.0f});
                m_renderer->add_point_light({m_player->position() + glm::vec3(-1.6f, 0.9f, 1.4f), glm::vec3(0.4f, 0.65f, 0.95f), 14.0f, 1.2f});
                m_renderer->add_point_light({m_player->position() + glm::vec3(0.0f, 2.4f, 4.0f), glm::vec3(0.85f, 0.45f, 1.0f), 20.0f, 2.2f});

                // Stage 0: Void Stalker
                m_stalkers.reset();
                m_burrowers.reset();
                m_showcase_render_delver = false;
                m_stalkers.spawn_stalker(m_player->position() + glm::vec3(0.0f, -0.22f, 2.8f));
                if (!m_stalkers.stalkers().empty()) {
                    auto& s = m_stalkers.stalkers_mut()[0];
                    s.state = StalkerState::Circling;
                    s.yaw = 3.40f; // 3/4 front dynamic angle
                }
                VF_LOG_INFO("ModelShowcase", "Staged Void Stalker for Slot 0.");
            }
            // Frame 14: Capture Slot 0 "ENEMY: VOID STALKER"
            else if (m_auto_test_frame == 14) {
                VisualTestHarness::instance().record_model_phase(0, "VOID STALKER", "docs/models/enemy_void_stalker.png", m_window->width(), m_window->height());
                // Transition to Stage 1: Seismic Burrower
                m_stalkers.reset();
                m_burrowers.reset();
                m_burrowers.spawn_burrower(m_player->position() + glm::vec3(0.0f, -0.35f, 4.4f));
                if (!m_burrowers.burrowers().empty()) {
                    auto& b = m_burrowers.burrowers_mut()[0];
                    b.state = BurrowerState::Breaching;
                    b.yaw = 2.80f; // Angled 3/4 toward camera
                    b.pitch = -0.06f;
                    b.cutter_angle = 1.25f;
                }
                VF_LOG_INFO("ModelShowcase", "Staged Seismic Burrower for Slot 1.");
            }
            // Frame 28: Capture Slot 1 "ENEMY: SEISMIC BURROWER"
            else if (m_auto_test_frame == 28) {
                VisualTestHarness::instance().record_model_phase(1, "SEISMIC BURROWER", "docs/models/enemy_seismic_burrower.png", m_window->width(), m_window->height());
                // Transition to Stage 2: Mining Drill Rig Viewmodel
                m_burrowers.reset();
                m_stalkers.reset();
                m_showcase_render_delver = false;
                m_player->set_active_tool(ToolSlot::MiningDrill);
                VF_LOG_INFO("ModelShowcase", "Staged Modular Mining Drill Rig for Slot 2.");
            }
            // Frame 42: Capture Slot 2 "MINING DRILL RIG"
            else if (m_auto_test_frame == 42) {
                VisualTestHarness::instance().record_model_phase(2, "MINING DRILL RIG", "docs/models/system_viewmodel_drill.png", m_window->width(), m_window->height());
                // Transition to Stage 3: Delver Kaelen (Demolitionist)
                m_showcase_render_delver = true;
                m_showcase_delver_class = CharacterClass::Demolitionist;
                m_showcase_delver_pos = m_player->position() + glm::vec3(0.0f, -0.42f, 2.2f);
                m_showcase_delver_yaw = 3.25f;
                VF_LOG_INFO("ModelShowcase", "Staged Delver Kaelen (Demolitionist) for Slot 3.");
            }
            // Frame 56: Capture Slot 3 "KAELEN (DEMOLITIONIST)"
            else if (m_auto_test_frame == 56) {
                VisualTestHarness::instance().record_model_phase(3, "KAELEN (DEMOLITIONIST)", "docs/models/character_demolitionist_kaelen.png", m_window->width(), m_window->height());
                // Transition to Stage 4: Delver Rhodes (Vanguard)
                m_showcase_render_delver = true;
                m_showcase_delver_class = CharacterClass::Vanguard;
                m_showcase_delver_pos = m_player->position() + glm::vec3(0.0f, -0.42f, 2.2f);
                m_showcase_delver_yaw = 3.25f;
                VF_LOG_INFO("ModelShowcase", "Staged Delver Rhodes (Vanguard) for Slot 4.");
            }
            // Frame 70: Capture Slot 4 "RHODES (VANGUARD)"
            else if (m_auto_test_frame == 70) {
                VisualTestHarness::instance().record_model_phase(4, "RHODES (VANGUARD)", "docs/models/character_vanguard_rhodes.png", m_window->width(), m_window->height());
                // Transition to Stage 5: Delver Vesper (Scout)
                m_showcase_render_delver = true;
                m_showcase_delver_class = CharacterClass::Scout;
                m_showcase_delver_pos = m_player->position() + glm::vec3(0.0f, -0.42f, 2.2f);
                m_showcase_delver_yaw = 3.25f;
                VF_LOG_INFO("ModelShowcase", "Staged Delver Vesper (Scout) for Slot 5.");
            }
            // Frame 84: Capture Slot 5 "VESPER (SCOUT)"
            else if (m_auto_test_frame == 84) {
                VisualTestHarness::instance().record_model_phase(5, "VESPER (SCOUT)", "docs/models/character_scout_vesper.png", m_window->width(), m_window->height());
            }
            // Frame 92: Finalize model showcase & exit
            else if (m_auto_test_frame >= 92) {
                VisualTestHarness::instance().finalize_model_showcase();
                VF_LOG_INFO("ModelShowcase", "Dedicated 3D Model Showcase finalized! All 6 models & master contact sheet generated in docs/models/.");
                break;
            }
        }

        // ── Automated Level Shapes & Architectural 3D Showcase Capture (--capture-level-shapes) ──
        if (m_config.capture_level_shapes) {
            m_auto_test_frame++;

            struct ShapeStageConfig {
                int slot;
                const char* label;
                const char* filename;
                int sector;
                bool is_corridor;
                RoomShapeType type;
                glm::vec3 cam_pos;
                float yaw;
                float pitch;
            };

            static const ShapeStageConfig s_shape_stages[16] = {
                {0, "SPAWN STAGING CAVERN", "docs/level_design/shapes/01_spawn_staging_cavern.png", 1, false, RoomShapeType::SpawnStagingCavern, glm::vec3(30.0f, 10.5f, 30.0f), 45.0f, -18.0f},
                {1, "MINING PILLAR HALL", "docs/level_design/shapes/02_mining_pillar_hall.png", 1, false, RoomShapeType::MiningPillarHall, glm::vec3(29.5f, 13.0f, 29.5f), 45.0f, -22.0f},
                {2, "CRYSTALLINE GEODE", "docs/level_design/shapes/03_crystalline_geode.png", 1, false, RoomShapeType::CrystallineGeode, glm::vec3(30.5f, 11.0f, 30.5f), 45.0f, -16.0f},
                {3, "TERRACED QUARRY", "docs/level_design/shapes/04_terraced_quarry.png", 1, false, RoomShapeType::TerracedQuarry, glm::vec3(29.5f, 14.5f, 29.5f), 45.0f, -28.0f},
                {4, "INDUSTRIAL VAULT BUNKER", "docs/level_design/shapes/05_industrial_vault_bunker.png", 2, false, RoomShapeType::IndustrialVaultBunker, glm::vec3(30.0f, 11.5f, 30.0f), 45.0f, -18.0f},
                {5, "FAULT LINE CREVASSE", "docs/level_design/shapes/06_fault_line_crevasse.png", 2, false, RoomShapeType::FaultLineCrevasse, glm::vec3(30.0f, 12.0f, 30.0f), 45.0f, -24.0f},
                {6, "ABYSSAL VERTICAL CHASM", "docs/level_design/shapes/07_abyssal_vertical_chasm.png", 3, false, RoomShapeType::AbyssalVerticalChasm, glm::vec3(30.0f, 19.0f, 30.0f), 45.0f, -32.0f},
                {7, "RADIOACTIVE CORE SANCTUARY", "docs/level_design/shapes/08_radioactive_core_sanctuary.png", 3, false, RoomShapeType::RadioactiveCoreSanctuary, glm::vec3(30.0f, 11.0f, 30.0f), 45.0f, -18.0f},
                {8, "EXTRACTION LANDING BAY", "docs/level_design/shapes/09_extraction_landing_bay.png", 1, false, RoomShapeType::ExtractionLandingBay, glm::vec3(30.0f, 11.0f, 30.0f), 45.0f, -18.0f},
                {9, "MAGMA CALDERA LAKE", "docs/level_design/shapes/10_magma_caldera_lake.png", 2, false, RoomShapeType::MagmaCalderaLake, glm::vec3(30.0f, 11.5f, 30.0f), 45.0f, -22.0f},
                {10, "SPIKE TRENCH ARENA", "docs/level_design/shapes/11_spike_trench_arena.png", 2, false, RoomShapeType::SpikeTrenchArena, glm::vec3(30.0f, 12.0f, 30.0f), 45.0f, -24.0f},
                {11, "VOID SINGULARITY RIFT", "docs/level_design/shapes/12_void_singularity_rift.png", 3, false, RoomShapeType::VoidSingularityRift, glm::vec3(30.0f, 15.0f, 30.0f), 45.0f, -26.0f},
                {12, "FUNGOID BIO GROTTO", "docs/level_design/shapes/13_fungoid_bio_grotto.png", 1, false, RoomShapeType::FungoidBioGrotto, glm::vec3(30.0f, 12.5f, 30.0f), 45.0f, -20.0f},
                {13, "LASER DEFENSE FOUNDRY", "docs/level_design/shapes/14_laser_defense_foundry.png", 2, false, RoomShapeType::LaserDefenseFoundry, glm::vec3(30.0f, 13.0f, 30.0f), 45.0f, -22.0f},
                {14, "CRUMBLING ARCH CANYON", "docs/level_design/shapes/15_crumbling_arch_canyon.png", 2, false, RoomShapeType::CrumblingArchCanyon, glm::vec3(30.0f, 13.5f, 30.0f), 45.0f, -24.0f},
                {15, "CORRIDOR BULKHEAD VAULT", "docs/level_design/shapes/16_corridor_bulkhead_vault.png", 2, true, RoomShapeType::IndustrialVaultBunker, glm::vec3(20.5f, 6.0f, 36.0f), 0.0f, -2.0f}
            };

            int current_slot = (m_auto_test_frame - 1) / 6;
            int frame_in_slot = (m_auto_test_frame - 1) % 6;

            if (current_slot >= 0 && current_slot < 16) {
                const auto& cfg = s_shape_stages[current_slot];

                if (frame_in_slot == 0) {
                    m_selected_level = cfg.sector;
                    if (m_renderer) {
                        m_renderer->set_sector(cfg.sector);
                    }
                    if (cfg.is_corridor) {
                        auto gen = std::make_unique<LevelGenerator>(cfg.sector, 1337);
                        gen->create_corridor_test_layout();
                        m_world->set_level_generator(std::move(gen));
                    } else {
                        auto gen = std::make_unique<LevelGenerator>(cfg.sector, 1337);
                        gen->create_single_room_test_layout(cfg.type);
                        m_world->set_level_generator(std::move(gen));
                    }
                    m_world->force_mesh_all_sync();
                    m_stalkers.reset();
                    m_burrowers.reset();
                    m_debris.clear();
                    m_plasma_bolts.clear();
                    m_showcase_render_delver = false;

                    m_player->set_position(cfg.cam_pos);
                    m_player->set_look_angles(cfg.yaw, cfg.pitch);

                    if (m_renderer) {
                        m_renderer->headlamp().enabled = true;
                        m_renderer->headlamp().intensity = 2.4f;
                    }

                    VF_LOG_INFO("LevelShapes", "Staged Shape [" << (current_slot + 1) << "/16]: " << cfg.label);
                } else if (frame_in_slot == 4) {
                    VisualTestHarness::instance().record_shape_phase(
                        cfg.slot,
                        cfg.label,
                        cfg.filename,
                        m_window->width(),
                        m_window->height()
                    );
                }
            } else if (m_auto_test_frame >= 16 * 6 + 2) {
                VisualTestHarness::instance().finalize_shapes_showcase();
                VF_LOG_INFO("LevelShapes", "Level Design & Shapes Showcase finalized! All 16 shapes & master contact sheet generated in docs/level_design/.");
                break;
            }
        }

        m_window->swap_buffers();
    }
}

} // namespace Voidfall
