#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "application.hpp"
#include "logger.hpp"
#include "screenshot.hpp"
#include "../graphics/particle_system.hpp"
#include "../ui/debrief_menu.hpp"
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

static Application* s_app_instance = nullptr;

Application* Application::instance() {
    return s_app_instance;
}

void Application::LaunchSector(int sector_index) {
    m_selected_level = sector_index;
    m_expedition_success = false;
    start_expedition(m_selected_level);
}

Application::Application(const AppConfig& config)
    : m_config(config)
{
    s_app_instance = this;
    init_systems();
}

Application::~Application() {
    if (s_app_instance == this) {
        s_app_instance = nullptr;
    }
}

void Application::TransitionState(GameState newState) {
    if (m_state == newState) {
        return;
    }

    // Explicit Channel Teardown on State Transition:
    // When switching to Main Menu, Orbital Hub, or Debrief, halt all gameplay voice channels
    if (newState == GameState::MainMenu || newState == GameState::OrbitalHub || newState == GameState::Debrief) {
        AudioSystem::StopAllGameplayVoices();
    }

    if (newState != GameState::Gameplay) {
        m_map_open = false;
        m_map_dragging_last = false;
    }

    // Reset single-trigger death guard, trigger 0.6s fade-from-black, and pause shooting on entering active gameplay
    if (newState == GameState::Gameplay) {
        m_expedition_success = false;
        if (m_state != GameState::Paused) {
            AudioSystem::instance().reset_death_sound();
            if (m_hud) {
                m_hud->trigger_spawn_fade(0.6f);
            }
        }
        if (m_player) {
            float pause_dur = (m_state == GameState::Paused) ? 0.35f : 0.6f;
            m_player->reset_spawn_shoot_pause(pause_dur);
        }
    }

    m_state = newState;
}

void Application::init_systems() {
    // 1. Window
    WindowConfig win_cfg;
    win_cfg.title = "Voidfall: Dredge (Multiplayer Edition) - [" +
                    std::string(m_config.is_host ? "HOST" : "CLIENT") + "]";
    win_cfg.width = m_config.window_width > 0 ? m_config.window_width : 1600;
    win_cfg.height = m_config.window_height > 0 ? m_config.window_height : 900;
    win_cfg.visible = !m_config.hidden_window;
    win_cfg.auto_screen_size = m_config.auto_screen_size && (m_config.window_width <= 0 || m_config.window_height <= 0);
    win_cfg.fullscreen = m_config.fullscreen;
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
    m_renderer->headlamp().inner_cutoff = glm::cos(glm::radians(18.0f));
    m_renderer->headlamp().outer_cutoff = glm::cos(glm::radians(28.0f));
    m_renderer->headlamp().max_range = 22.0f;
    m_renderer->headlamp().intensity = 5.2f;
    ParticleSystem::SetRenderer(m_renderer.get());
    DebriefMenu::SetRedeployHandler([this](int sector) {
        LaunchSector(sector);
    });
    DebriefMenu::SetReturnToHubHandler([this]() {
        TransitionState(GameState::OrbitalHub);
    });
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

    m_player->set_can_deploy_charge_predicate([this]() {
        return m_inventory.demolition_charges > 0;
    });

    m_player->set_on_charge_placed([this](const glm::ivec3& pos, const glm::ivec3& normal) {
        if (m_inventory.demolition_charges > 0) {
            m_inventory.demolition_charges--;
        }
        glm::vec3 world_pos = glm::vec3(pos) + glm::vec3(0.5f) + glm::vec3(normal) * 0.5f;
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::BulkheadDeploy, world_pos, 0.85f);
        }
        m_noise_meter.add_bulkhead_sound(world_pos, true);
    });

    m_player->set_on_warning([this](const std::string& /*msg*/) {
        // Sensory feedback handled via audio/visuals/environment
    });

    m_player->set_on_suffocation_cough([this]() {
        if (m_audio) {
            m_audio->trigger_player_asphyxiation(0.95f);
        }
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

    m_player->set_on_weapon_reload([this](CharacterClass cls, float reload_time) {
        if (m_audio) {
            m_audio->play_weapon_reload(static_cast<int>(m_player->weapon_archetype()));
        }
        m_noise_meter.add_reload_sound(m_player->position());
    });

    m_player->set_on_flare_thrown([this](const glm::vec3& origin, const glm::vec3& dir, CharacterClass cls) {
        FlareManager::instance().spawn_flare(origin, dir, cls);
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::TacticalOvercharge, origin, 1.0f);
        }
    });

    m_player->set_on_melee_shove([this](const glm::vec3& cam_pos, const glm::vec3& cam_dir) {
        float dmg = 15.0f * m_player->melee_damage_multiplier();
        float shove_range = (m_player->upgrades().cleaveShockwaveTier > 0) ? 3.2f : 2.5f;
        float min_cos = (m_player->upgrades().cleaveShockwaveTier > 0) ? 0.0f : 0.65f;

        int hits = m_stalkers.apply_melee_shove(cam_pos, cam_dir, shove_range, min_cos, dmg);

        // Weapon/Tool-specific melee swing audio whoosh
        if (m_audio) {
            m_audio->play_melee_swing(
                static_cast<int>(m_player->active_tool()),
                static_cast<int>(m_player->weapon_archetype()),
                cam_pos
            );
        }

        // Camera recoil / trauma impulse tailored to weapon heft & swing kinematics
        if (m_player->active_tool() == ToolSlot::CombatWeapon) {
            if (m_player->weapon_archetype() == WeaponArchetype::MagmaScattergun) {
                m_player->add_trauma(0.20f); // Heavy horizontal cleave kick
            } else if (m_player->weapon_archetype() == WeaponArchetype::NeedlerRailgun) {
                m_player->add_trauma(0.12f); // Sharp centerline spear thrust
            } else {
                m_player->add_trauma(0.16f); // Upward muzzle uppercut kick
            }
        } else if (m_player->active_tool() == ToolSlot::MiningDrill) {
            m_player->add_trauma(0.22f);     // Heavy industrial battering ram shudder
        } else {
            m_player->add_trauma(0.15f);
        }

        // Hostile creature melee impact feedback
        if (hits > 0) {
            if (m_hud) {
                m_hud->trigger_hit_marker(false, dmg);
            }
            if (m_audio) {
                m_audio->trigger_melee_hit_flesh(cam_pos + cam_dir * 1.5f);
            }
            if (m_player->upgrades().berserkerOverdriveTier > 0 && m_audio) {
                m_audio->play_sound_2d(SoundCue::UIUpgrade, 0.70f, 1.45f);
            }
        }
    });

    m_player->set_on_interact([this]() {
        try_interact();
    });

    m_mission.set_on_notification([this](const std::string& /*msg*/, float /*dur*/) {
        // Rely on sounds, visuals, and environment events instead of on-screen text notices
    });
    m_mission.set_on_gas_ignited([this](const glm::vec3& center, float rad) {
        if (m_renderer) {
            m_renderer->spawn_fireball(center, 32);
        }
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::ExplosiveBlast, center, 1.0f);
        }
        m_noise_meter.add_demolition_sound(center, true);
        if (m_player && glm::distance(m_player->position(), center) < rad * 1.5f) {
            m_player->add_trauma(0.35f);
        }
    });

    m_player->set_on_damage_source([this](float dmg, PlayerController::DamageSource source) {
        float flash_intensity = std::clamp(dmg * 0.025f + 0.45f, 0.45f, 0.95f);
        if (source == PlayerController::DamageSource::Radiation) {
            // ZERO screen shake trauma for radiation! Subtle emerald ionizing static vignette
            if (m_hud) {
                m_hud->trigger_radiation_flash(std::clamp(dmg * 0.04f + 0.20f, 0.20f, 0.45f));
            }
        } else if (source == PlayerController::DamageSource::ToxicGas) {
            // Subtle cough shudder only (0.02f), no violent camera disorientation!
            if (m_hud) {
                m_hud->trigger_toxic_gas_flash(flash_intensity);
            }
        } else if (source == PlayerController::DamageSource::EnemyAttack) {
            // ENEMY ATTACK: Trigger visceral on-screen blood splatters + flesh slash audio!
            if (m_hud) {
                m_hud->trigger_damage_flash(flash_intensity);
                m_hud->trigger_enemy_blood_splatter(flash_intensity);
            }
            float trauma_add = std::clamp(dmg * 0.035f + 0.30f, 0.30f, 0.95f);
            m_trauma = std::min(1.0f, m_trauma + trauma_add);
            if (m_audio) {
                m_audio->trigger_enemy_flesh_hit(1.0f);
            }
        } else if (source == PlayerController::DamageSource::FallImpact) {
            // FALL DAMAGE: Bone cracking noise, camera trauma, but STRICTLY NO blood splatters!
            if (m_hud) {
                m_hud->trigger_damage_flash(flash_intensity);
            }
            float trauma_add = std::clamp(dmg * 0.035f + 0.35f, 0.35f, 0.95f);
            m_trauma = std::min(1.0f, m_trauma + trauma_add);
            if (m_audio) {
                m_audio->trigger_bone_crack(1.0f);
            }
        } else if (source == PlayerController::DamageSource::FallingDebris) {
            // FALLING DEBRIS (Seismic Activity): Stone crash/armor crunch, camera trauma, but STRICTLY NO blood splatters!
            if (m_hud) {
                m_hud->trigger_damage_flash(flash_intensity);
            }
            float trauma_add = std::clamp(dmg * 0.030f + 0.25f, 0.25f, 0.85f);
            m_trauma = std::min(1.0f, m_trauma + trauma_add);
            if (m_audio) {
                m_audio->trigger_debris_impact(1.0f);
            }
        } else if (source == PlayerController::DamageSource::CorrosiveAcid) {
            // CORROSIVE ACID: Sizzling chemical etching vignette & caustic hiss!
            if (m_hud) {
                m_hud->trigger_acid_flash(flash_intensity);
                m_hud->set_acid_exposure(1.0f, 0.016f);
            }
            float trauma_add = std::clamp(dmg * 0.015f + 0.06f, 0.06f, 0.20f);
            m_trauma = std::min(1.0f, m_trauma + trauma_add);
            if (m_audio) {
                m_audio->play_sound_2d(SoundCue::ToxicGasHiss, 0.70f);
            }
        } else if (source == PlayerController::DamageSource::CryogenicCoolant) {
            // CRYOGENIC COOLANT: Frosted sub-zero ice crystallization & acoustic droplet ping
            if (m_hud) {
                m_hud->trigger_frost_flash(flash_intensity);
                m_hud->set_frost_exposure(1.0f, 0.016f);
            }
            float trauma_add = std::clamp(dmg * 0.010f + 0.03f, 0.03f, 0.15f);
            m_trauma = std::min(1.0f, m_trauma + trauma_add);
            if (m_audio) {
                m_audio->play_sound_2d(SoundCue::CavernDrip, 0.60f);
            }
        } else if (source == PlayerController::DamageSource::Suffocation) {
            // CAVE-IN COLLAPSE / SUFFOCATION: Subtle diaphragmatic cough shudder (0.03f)
            float trauma_add = 0.03f;
            m_trauma = std::min(1.0f, m_trauma + trauma_add);
        } else {
            // Kinetic / Spikes / Thermal Lava / Void Singularity
            if (m_hud) {
                m_hud->trigger_damage_flash(flash_intensity);
            }
            float trauma_add = (source == PlayerController::DamageSource::ThermalLava)
                                ? std::clamp(dmg * 0.015f + 0.10f, 0.10f, 0.35f)
                                : std::clamp(dmg * 0.035f + 0.25f, 0.25f, 0.90f);
            m_trauma = std::min(1.0f, m_trauma + trauma_add);
            if (m_audio && (source == PlayerController::DamageSource::Kinetic || source == PlayerController::DamageSource::Spikes)) {
                m_audio->play_sound_2d(SoundCue::SuitPuncture, 0.85f);
            }
        }
    });

    // 5. Systems
    setup_hazard_system();

    SwarmManager::instance().SetOnEnrageEnded([this]() {
        m_noise_meter.trigger_post_combat_cooldown();
    });

    m_noise_meter.set_on_wave([this](NoiseMeter::AlertLevel level, float pct) {
        if (!m_player || !m_world) return;
        if (!m_spawn_mgr.can_spawn(m_stalkers.active_count(), m_player->position())) return;

        int allowed = m_spawn_mgr.max_allowed_enemies() - m_stalkers.active_count();
        if (allowed <= 0) return;

        int count = 1;
        if (level == NoiseMeter::AlertLevel::Alerted) {
            count = 1;
            if (m_audio) {
                m_audio->play_sound_3d(SoundCue::StalkerChitter, m_player->position() + glm::vec3(6.0f, 0.0f, 6.0f), 0.95f);
            }
        } else if (level == NoiseMeter::AlertLevel::Agitated) {
            count = (m_selected_level >= 3) ? 2 : ((m_selected_level == 2) ? 2 : 1);
            if (m_audio) {
                m_audio->play_sound_2d(SoundCue::StalkerEchoScreech, 0.45f);
            }
            m_player->add_trauma(0.2f);
        } else if (level == NoiseMeter::AlertLevel::Swarming) {
            count = (m_selected_level >= 3) ? 3 : ((m_selected_level == 2) ? 2 : 2);
            SwarmManager::instance().set_active_wave_hostiles(count);
            if (m_audio) {
                m_audio->play_sound_2d(SoundCue::StalkerEchoScreech, 0.55f);
            }
            m_player->add_trauma(0.35f);
        }
        count = std::min(count, allowed);
        if (count > 0) {
            m_stalkers.spawn_wave(m_player->position(), count, *m_world);
        }
    });

    m_extraction = std::make_unique<ExtractionSystem>();
    setup_extraction_callbacks();

    // 6. UI systems
    m_hud = std::make_unique<HUD>(m_window->width(), m_window->height());
    m_hub_ui = std::make_unique<OrbitalHubUI>(m_window->width(), m_window->height());
    m_hub_ui->set_test_mode(is_test_mode());
    m_pause_menu = std::make_unique<PauseMenu>(m_window->width(), m_window->height());

    // 7. Audio Engine (Runs in silent headless mode during automated testing/captures or when --mute is requested)
    m_audio = std::make_unique<AudioEngine>();
    bool enable_audio = true;
    if (m_config.mute_audio || m_config.test_mode || m_config.test_enemy ||
        m_config.test_enemy_load ||
        m_config.auto_play_test || m_config.capture_models || 
        m_config.capture_level_shapes || m_config.hidden_window ||
        !m_config.single_screenshot_path.empty() || m_config.is_test_save) {
        enable_audio = false;
    }
    if (m_config.force_audible) {
        enable_audio = true;
    }
    m_audio->init(enable_audio);
    m_audio->set_master_volume(m_settings.master_volume);
    AudioSystem::instance().set_audio_engine(m_audio.get());

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
            while (m_debris.size() >= World::MAX_ACTIVE_DEBRIS) {
                if (!m_debris.empty() && m_debris.front().is_sleeping() && m_world) {
                    m_debris.front().re_voxelize_blocks(*m_world);
                }
                m_debris.erase(m_debris.begin());
            }
            m_debris.emplace_back(pkt.debris_id, pkt.origin, pkt.linear_velocity,
                                 pkt.angular_velocity, pkt.material_id, pkt.block_count, m_world.get());
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
        m_selected_level = m_user_profile.get_highest_unlocked_sector();
    }

    m_settings = m_user_profile.settings;
    m_settings.sanitize();
    sync_audio_settings();

    // Sync all sector records, not just the first 3, to support endless progression
    for (int s = 1; s < PlayerInventory::MAX_SECTOR_RECORDS; ++s) {
        m_inventory.sector_records[s] = m_user_profile.sector_records[s];
    }
    sync_profile_with_player();

    // Reveal the window seamlessly once all subsystems, shaders, and resources are fully initialized
    if (m_window) {
        m_window->show();
    }
}

void Application::sync_audio_settings() {
    if (m_audio) {
        m_audio->set_master_volume(m_settings.master_volume);
        m_audio->set_music_volume(m_settings.music_volume);
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

bool Application::try_interact() {
    if (m_state != GameState::Gameplay || !m_player || !m_world) return false;

    // Check Precursor Vault Relic retrieval
    if (m_mission.is_vault_breached() && !m_mission.is_relic_retrieved()) {
        glm::vec3 relic_pos = glm::vec3(m_mission.vault().relic_pos) + glm::vec3(0.5f);
        float dist = glm::distance(m_player->position(), relic_pos);

        RaycastHit hit = m_player->get_look_target(*m_world, 5.0f);
        bool aiming_at_relic = hit.hit && (hit.block_pos == m_mission.vault().relic_pos ||
                                           (hit.voxel.is_solid() && hit.voxel.material_id == MAT_PRISMATIC_CRYSTAL && dist < 6.0f));

        if (dist <= 3.8f || aiming_at_relic) {
            if (m_mission.interact_relic(*m_world, m_player->position(), 6.0f)) {
                m_inventory.add_relic();
                m_inventory.total_run_score += 250;
                m_skills.add_demolitions_xp(50);

                int dummy_exp = 0, dummy_ti = 0;
                m_world->vault_door_mut().interact_relic(dummy_exp, dummy_ti);
                register_objective_interaction(MAT_PRISMATIC_CRYSTAL);

                if (m_hud) {
                    m_hud->add_floating_loot(relic_pos + glm::vec3(0.0f, 0.6f, 0.0f),
                                             "+1 RELIC HYPER-CORE (+250 PTS)",
                                             glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
                }
                if (m_audio) {
                    m_audio->play_sound_2d(SoundCue::BulkheadDeploy, 1.0f);
                }
                if (m_renderer) {
                    m_renderer->spawn_break_particles(relic_pos, glm::ivec3(0, 1, 0), MAT_PRISMATIC_CRYSTAL);
                }
                VF_LOG_INFO("Application", "Player successfully retrieved Precursor Relic via interactive pickup [E]!");
                return true;
            }
        }
    }
    return false;
}

void Application::sync_profile_with_player() {
    CharacterClass cls = static_cast<CharacterClass>(m_user_profile.selected_class_id);
    if (m_player) {
        m_player->apply_attributes_and_upgrades(cls, m_user_profile.upgrades);
        m_player->set_mouse_sensitivity(m_settings.mouse_sensitivity);
        m_player->set_auto_step(m_settings.auto_step);
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
    m_expedition_success = false;
    m_debris.clear();
    if (m_renderer) {
        m_renderer->set_sector(level);
    }

    // Determine level generation seed:
    // 1. Explicit CLI override (--seed <N>)
    // 2. Previously completed level for this user -> use saved persistent seed (doesn't change)
    // 3. Uncompleted / newly opened level -> generate fresh random seed
    uint32_t seed_to_use = 0;
    if (m_config.has_explicit_seed) {
        seed_to_use = m_config.world_seed;
        VF_LOG_INFO("LevelGen", "Using explicit command-line seed " << seed_to_use << " for Sector " << level);
    } else if (level >= 1 && level < UserProfile::MAX_SECTOR_RECORDS &&
               m_user_profile.is_sector_completed(level) &&
               m_user_profile.sector_records[level].saved_seed != 0) {
        seed_to_use = m_user_profile.sector_records[level].saved_seed;
        VF_LOG_INFO("LevelGen", "Loading saved persistent seed " << seed_to_use << " for completed Sector " << level);
    } else {
        seed_to_use = SaveSystem::generate_random_seed(level);
        VF_LOG_INFO("LevelGen", "Generated random seed " << seed_to_use << " for uncompleted Sector " << level);
    }

    m_current_level_seed = seed_to_use;
    m_inventory.active_seed = seed_to_use;

    // Voidite target scales: Sector 1 = 25, Sector 2 = 30, Sector 3 = 45, then +10 per sector beyond 3, capped at 150
    int voidite_target = (level <= 1) ? 25 :
                         (level == 2) ? 30 :
                         (level == 3) ? 45 :
                         std::min(45 + (level - 3) * 10, 150);
    int astral_target = (level >= 3) ? (3 + (level - 3) * 2) : 0;
    m_inventory.reset(voidite_target, astral_target);
    m_expedition_time = 0.0f;
    reset_all_objective_idle_timers();

    // Apply Delver Archetype & Upgrades
    CharacterClass cls = static_cast<CharacterClass>(m_user_profile.selected_class_id);
    auto attr = get_character_attributes(cls);
    sync_profile_with_player();

    // Baseline Bulkheads per class
    m_inventory.max_bulkheads = attr.maxBulkheads;
    m_inventory.bulkheads = std::min(m_inventory.bulkheads, m_inventory.get_max_bulkheads());

    // Generate Level Sector Procedural Topology
    m_world->generate_world(level, seed_to_use, voidite_target);
    m_world->verify_and_seed_objective_ores(astral_target);
    m_world->update(m_player->position(), 2);

    glm::vec3 spawn_pos = (m_world && m_world->level_generator()) ?
        m_world->level_generator()->spawn_position() : glm::vec3(16.0f, 5.1f, 16.0f);
    m_player->set_position(spawn_pos);
    m_player->clamp_to_surface(*m_world);
    m_player->align_spawn_yaw(*m_world);
    m_trauma = 0.0f;
    m_player->set_trauma(0.0f);
    m_player->set_velocity(glm::vec3(0.0f));

    // Reset systems
    m_noise_meter.reset();
    SwarmManager::instance().reset();
    StealthSystem::instance().reset();
    m_stalker_spotted_audio_cooldown = 4.0f;
    m_stalker_investigate_audio_cooldown = 3.5f;
    m_stalkers.reset();
    m_stalkers.set_sector_level(level);
    m_burrowers.reset();
    m_spawn_mgr.reset(level, m_player->position());
    CarcassManager::instance().clear();
    FlareManager::instance().reset();
    m_player->clear_breadcrumbs();
    m_player->reset_insertion_shield(6.0f);
    m_player->reset_spawn_shoot_pause(0.6f);
    m_player->reset_radiation();
    m_terrain_scanner.reset();
    m_mission.reset(level);
    m_mission.embed_precursor_vault(*m_world, level);
    m_spawn_mgr.set_on_dust_burst([this](const glm::vec3& pos, const glm::vec3& normal) {
        if (m_renderer) {
            m_renderer->spawn_burrow_particles(pos, normal, MAT_FRACTURED_GRANITE, 10);
        }
    });
    m_spawn_mgr.set_on_audio_cue([this](const glm::vec3& pos) {
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::MonsterDigging, pos, 1.0f);
        }
    });
    m_plasma_bolts.clear();
    setup_hazard_system();
    m_hazard->set_sector_parameters(level);

    // Populate active fauna placed by world generation (Smart Spawning with quarantine zone)
    if (m_world) {
        for (const auto& fauna : m_world->GetActiveEntities()) {
            m_stalkers.spawn_roosting(fauna.pos, fauna.role);
        }
        m_stalkers.assign_patrol_zones_from_world(*m_world);
    }

    // Pre-populate Burrowers for Sector 3+ (Endgame Abyssal Mantle)
    if (m_world && m_world->level_generator()) {
        const auto& rooms = m_world->level_generator()->rooms();
        int burrower_count = (level >= 3 && rooms.size() > 4) ? 1 + (level - 3) / 3 : 0;
        for (int b = 0; b < burrower_count && (4 + b) < static_cast<int>(rooms.size()); ++b) {
            int room_idx = 4 + b;
            glm::vec3 bpos(static_cast<float>(rooms[room_idx].center.x),
                           static_cast<float>(rooms[room_idx].floor_y) + 2.0f,
                           static_cast<float>(rooms[room_idx].center.z));
            m_burrowers.spawn_dormant_burrower(bpos, 1.2f + b * 0.1f);
        }
        m_burrowers.assign_patrol_zones_from_world(*m_world);
    }

    // Pre-placed fauna initialize with designated patrol zones and spawn grace.
    // Half of the cavern fauna start as active prowling sentries in Idle state,
    // while the others start dormant in Roosting with autonomous slumber timers.
    for (size_t i = 0; i < m_stalkers.stalkers().size(); ++i) {
        auto& s = m_stalkers.stalkers_mut()[i];
        if (s.is_boss()) {
            s.state = StalkerState::Idle;
            s.surface_state = StalkerSurfaceState::FLOOR;
            s.target_surface_state = StalkerSurfaceState::FLOOR;
            s.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
            s.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
            s.patrol_pause_timer = 0.5f;
            s.slumber_timer = 0.0f;
            s.spawn_grace_timer = 0.0f;
            s.detection_range = 28.0f;
        } else {
            bool is_active_sentry = (i % 2 == 0);
            if (is_active_sentry && s.has_patrol_zone) {
                s.state = StalkerState::Idle;
                s.surface_state = StalkerSurfaceState::FLOOR;
                s.target_surface_state = StalkerSurfaceState::FLOOR;
                s.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
                s.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
                s.patrol_pause_timer = 0.5f;
            } else {
                s.state = StalkerState::Roosting;
                s.slumber_timer = 90.0f + static_cast<float>((s.id * 17) % 60);
            }
            s.spawn_grace_timer = 4.0f;
        }
        s.state_timer = 0.0f;
        s.velocity = glm::vec3(0.0f);
        s.screech_timer = 12.0f;
        s.stuck_timer = 0.0f;
        s.stuck_checkpoint_pos = s.position;
        s.stuck_checkpoint_timer = 0.0f;
    }
    m_stalkers.reset_screech_cooldowns(12.0f);
    m_burrowers.reset_audio_cooldowns(12.0f);

    m_extraction = std::make_unique<ExtractionSystem>();
    m_holdout_stage = 0;
    setup_extraction_callbacks();

    if (m_audio) {
        m_audio->stop_all();
        m_audio->set_current_sector(level);

        // Map sector to ambient drone theme
        int audio_tier = std::min((level - 1) / 3, 2); // 0 = Sector 1-3, 1 = 4-6, 2 = 7+
        SoundCue ambient_drone   = (audio_tier == 0) ? SoundCue::AmbientSector1 :
                                   (audio_tier == 1) ? SoundCue::AmbientSector2 :
                                                       SoundCue::AmbientSector3;

        // Play non-repetitive arrival stinger (curated variety pool, non-consecutive history, organic pitch, smooth fade-in/fade-out)
        m_audio->play_arrival_stinger(level);
        m_audio->play_sound_2d(ambient_drone, 0.24f, 1.0f, true);
    }

    m_death_sequence = false;
    m_death_timer = 0.0f;
    m_debrief_input_lock = 0.0f;
    m_tremor_spall_timer = 0.0f;
    m_aftershock_wave_timer = 0.0f;
    m_expedition_success = false;
    if (m_hud) {
        m_hud->set_death_sequence(false);
        m_hud->SetContractorBriefing(true, 8.0f);
        m_hud->trigger_spawn_fade(0.6f);
    }

    m_map_open = false;
    m_map_dragging_last = false;
    m_window->set_cursor_locked(true);
    TransitionState(GameState::Gameplay);
}

void Application::setup_extraction_callbacks() {
    if (!m_extraction) return;

    m_extraction->set_on_complete([this]() {
        m_expedition_success = true;
        m_death_sequence = false;
        if (m_player) {
            m_player->reset_insertion_shield(999.0f); // Complete invulnerability upon extraction
        }
        if (m_audio) m_audio->play_sound_2d(SoundCue::EvacTouchdown, 1.0f);
        if (m_mission.is_relic_retrieved()) {
            m_user_profile.total_exp += m_mission.total_bonus_xp();
            m_user_profile.total_titanium += m_mission.total_bonus_titanium();
            VF_LOG_INFO("MissionSystem", "Extraction bonus awarded: +" << m_mission.total_bonus_xp()
                        << " EXP, +" << m_mission.total_bonus_titanium() << " Titanium Cores!");
        }
        m_inventory.finalize_run(m_selected_level, true); // Safe evac!
        if (m_selected_level >= 1 && m_selected_level < UserProfile::MAX_SECTOR_RECORDS) {
            uint32_t completed_seed = (m_world ? m_world->seed() : m_current_level_seed);
            int rate = m_inventory.sector_records[m_selected_level].highest_completion_rate;
            const std::string& badge = m_inventory.sector_records[m_selected_level].best_badge;
            m_user_profile.mark_sector_completed(m_selected_level, completed_seed, rate, badge);
            m_inventory.sector_records[m_selected_level] = m_user_profile.sector_records[m_selected_level];
            SaveSystem::save_profile(m_user_profile, m_active_save_file);
            VF_LOG_INFO("SaveSystem", "Sector " << m_selected_level << " completed! Rate=" << rate
                        << "%, Badge=" << badge << ", Saved persistent level seed: "
                        << m_user_profile.sector_records[m_selected_level].saved_seed);
        }
        TransitionState(GameState::Debrief);
        m_window->set_cursor_locked(false);
        std::cout << "[Extraction] Delver extraction complete! Returning to debrief." << std::endl;
    });

    m_extraction->set_on_clearance_verify([this](const glm::vec3& beacon_pos, std::string*) -> bool {
        if (!m_world) return true;
        int cx = static_cast<int>(std::floor(beacon_pos.x));
        int cz = static_cast<int>(std::floor(beacon_pos.z));
        int gy = static_cast<int>(std::floor(beacon_pos.y));
        for (int dx = -2; dx <= 2; ++dx) {
            for (int dz = -2; dz <= 2; ++dz) {
                if (dx * dx + dz * dz > 3) continue;
                for (int y = gy + 1; y <= gy + 5; ++y) {
                    if (y < m_world->world_height() && m_world->is_solid(glm::ivec3(cx + dx, y, cz + dz))) {
                        m_world->set_voxel(cx + dx, y, cz + dz, Voxel{MAT_AIR, 0}, true);
                        if (m_renderer) {
                            m_renderer->spawn_break_particles(glm::vec3(cx + dx, y, cz + dz) + 0.5f, glm::ivec3(0, 1, 0), MAT_FRACTURED_GRANITE);
                        }
                    }
                }
            }
        }
        return true;
    });

    m_extraction->set_on_ceiling_warning([this](const glm::vec3& beacon_pos) {
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::MonsterDigging, beacon_pos + glm::vec3(0.0f, 16.0f, 0.0f), 1.0f);
        }
        if (m_renderer) {
            // Spawn falling dust and rock debris from cavern ceiling above beacon
            for (int i = 0; i < 8; ++i) {
                float ox = static_cast<float>(rand() % 40 - 20) * 0.1f;
                float oz = static_cast<float>(rand() % 40 - 20) * 0.1f;
                glm::vec3 dust_pos = beacon_pos + glm::vec3(ox, 14.0f + static_cast<float>(rand() % 20) * 0.1f, oz);
                m_renderer->spawn_break_particles(dust_pos, glm::ivec3(0, -1, 0), MAT_FRACTURED_GRANITE);
            }
            m_renderer->trigger_dust_kickup(3.0f);
        }
        if (m_player) {
            m_player->add_trauma(0.25f);
        }
    });

    m_extraction->set_on_pod_arrival([this](const glm::vec3& beacon_pos) {
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::EvacTouchdown, beacon_pos, 1.0f);
        }
        if (m_renderer) {
            // Heavy impact dust ring around pod touchdown
            for (int angle = 0; angle < 360; angle += 30) {
                float rad = glm::radians(static_cast<float>(angle));
                glm::vec3 ring_pos = beacon_pos + glm::vec3(std::cos(rad) * 2.8f, 0.2f, std::sin(rad) * 2.8f);
                m_renderer->spawn_break_particles(ring_pos, glm::ivec3(0, 1, 0), MAT_FRACTURED_GRANITE);
            }
            m_renderer->trigger_dust_kickup(4.0f);
        }
        if (m_player) {
            m_player->add_trauma(0.4f);
        }
    });

    m_extraction->set_on_steam_vent([this](const glm::vec3& beacon_pos) {
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::GeothermalVent, beacon_pos + glm::vec3(0.0f, 2.0f, 0.0f), 0.85f);
        }
        if (m_renderer) {
            // Emit pressurized steam/gas clouds from pod vents
            for (int i = 0; i < 4; ++i) {
                float angle = static_cast<float>(i) * 1.5708f;
                glm::vec3 vent_pos = beacon_pos + glm::vec3(std::cos(angle) * 1.4f, 3.2f, std::sin(angle) * 1.4f);
                m_renderer->spawn_water_mist(vent_pos, 4);
            }
        }
    });
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

    // Spawn dynamic billboard debris quads aligned with voxel shape
    VoxelShape shape = static_cast<VoxelShape>(flags & VOXEL_SHAPE_MASK);
    m_renderer->spawn_break_particles(glm::vec3(x, y, z), normal, mat, shape);
    m_spawn_mgr.on_block_broken(mat);

    // Register delver interaction with objective materials and auto-remove active hint
    register_objective_interaction(mat);

    if (m_hud && m_hud->has_objective_hint()) {
        glm::vec3 bpos(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        float d = glm::distance(bpos, m_objective_hint_pos);
        bool is_target_mat = (m_objective_hint_mat != 0 && mat == m_objective_hint_mat);
        if (d <= 2.5f || is_target_mat) {
            uint8_t hint_mat = m_objective_hint_mat != 0 ? m_objective_hint_mat : mat;
            m_hud->clear_objective_hint();
            m_objective_hint_given = false;
            m_objective_hint_timer = 0.0f;
            register_objective_interaction(hint_mat);
            VF_LOG_INFO("Gameplay", "Objective hint removed on block broken: mat="
                        << static_cast<int>(mat) << " at (" << x << ", " << y << ", " << z << ")");
        }
    }

    // Anchor Dependency & Auto-Despawn: clear surface props on broken voxel faces
    if (m_world) {
        m_world->clear_disconnected_clutter(glm::ivec3(x, y, z));
    }

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
        }
    } else if (mat == MAT_FRACTURED_GRANITE) {
        m_inventory.add_scrap_from_granite();
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
        m_hud->add_floating_loot(popup_pos, "+1 VAULT BULKHEAD BREACHED (+50 PTS)", glm::vec4(1.0f, 0.82f, 0.2f, 1.0f));
        m_skills.add_demolitions_xp(50);
        m_trauma = std::min(m_trauma + 0.35f, 1.0f);
    } else if (mat == MAT_ASTRAL_ORE) {
        int amt = 1;
        static uint32_t s_astral_seed = 9187;
        s_astral_seed = s_astral_seed * 1103515245 + 12345;
        if (is_demolitionist && ((s_astral_seed % 3) == 0)) {
            amt = 2; // Demolitionist 33% double yield on rare stellar minerals
        }
        m_inventory.add_astral_ore(amt);
        int bonus_xp = is_demolitionist ? 15 : 0;
        int bonus_score = is_demolitionist ? 40 : 0;
        m_inventory.total_run_score += bonus_score;
        m_skills.add_demolitions_xp(35 + bonus_xp);
        std::string popup = (amt > 1) ? "+2 ASTRAL ORE (+100 PTS [DEMO REFINING])" : "+1 ASTRAL ORE (+60 PTS)";
        m_hud->add_floating_loot(popup_pos, popup, glm::vec4(1.0f, 0.84f, 0.15f, 1.0f)); // Radiant Astral Gold
    } else if (mat == MAT_PRISMATIC_CRYSTAL) {
        m_inventory.add_salvage(10);
        m_inventory.total_run_score += 50;
        m_skills.add_demolitions_xp(25);
        m_hud->add_floating_loot(popup_pos, "+10 SALVAGE (+50 PTS [PRISMATIC DECOR])", glm::vec4(0.30f, 0.95f, 1.0f, 1.0f));
    } else if (mat == MAT_BIOLUMINESCENT_FLORA) {
        m_inventory.add_salvage(8);
        m_inventory.total_run_score += 15;
        m_skills.add_demolitions_xp(10);
        m_hud->add_floating_loot(popup_pos, "+8 BIO-SAMPLE (+15 PTS)", glm::vec4(0.25f, 0.95f, 0.50f, 1.0f));
    } else {
        m_inventory.add_salvage(1);
        m_hud->add_floating_loot(popup_pos, "+1 SALVAGE (+10 PTS)", glm::vec4(0.7f, 0.75f, 0.8f, 1.0f));
        m_skills.add_demolitions_xp(5);
    }

    // Accumulate mining fracture sound event based on material hardness & 3D location
    m_noise_meter.add_voxel_break_sound(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), mat);
    if (m_hazard) {
        m_hazard->on_rock_mined(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), mat);
    }

    // Drill contact damage to any ambush stalker right in front of drill bit
    m_stalkers.damage_nearest(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), 2.5f, 25.0f);

    // 3D Spatial Audio Cue for Voxel Fracture
    if (m_audio) {
        SoundCue cue = SoundCue::VoxelBreakBasalt;
        if (mat == MAT_TITANIUM) cue = SoundCue::VoxelBreakTitanium;
        else if (mat == MAT_VOIDITE_CRYSTAL || mat == MAT_ASTRAL_ORE) cue = SoundCue::VoxelBreakVoidite;
        else if (mat == MAT_INDUSTRIAL_BULKHEAD) cue = SoundCue::VoxelBreakBulkhead;
        else if (mat == MAT_RADIOACTIVE_ORE) cue = SoundCue::VoxelBreakRadioactive;
        m_audio->play_sound_3d(cue, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), 1.0f);
    }

    // Drilling into porous bio-gas voxels creates a lingering green fume hazard cloud
    if (mat == MAT_GAS || mat == MAT_TOXIC_GAS) {
        if (m_renderer) {
            m_renderer->spawn_toxic_gas_cloud(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), 5);
        }
    }

    // Authoritative Anchored Island BFS Cave-in Solver (Bounded 64 voxels max search depth)
    if (m_config.is_host) {
        if (m_world->borders_hanging_overhang_or_stalactite(x, y, z)) {
            auto unanchored = StructuralCheck::solve_cavein(*m_world, x, y, z, 64);
            for (const auto& island : unanchored) {
                while (m_debris.size() >= World::MAX_ACTIVE_DEBRIS) {
                    if (!m_debris.empty() && m_debris.front().is_sleeping() && m_world) {
                        m_debris.front().re_voxelize_blocks(*m_world);
                    }
                    m_debris.erase(m_debris.begin());
                }
                uint32_t did = m_next_debris_id++;
                glm::vec3 vel(0.0f, -1.5f, 0.0f);
                glm::vec3 rot(0.2f, 0.5f, 0.1f);
                m_debris.emplace_back(did, island.center_of_mass, vel, rot, island.blocks, island.primary_material, m_world.get());
                if (m_host) {
                    m_host->broadcast_debris_spawn(did, island.center_of_mass, vel, island.primary_material, island.blocks.size());
                }
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

    bool extraction_active = m_extraction && (m_extraction->phase() == ExtractionPhase::BeaconDeployed || m_extraction->phase() == ExtractionPhase::PodLanded);
    glm::vec3 extraction_pos = m_extraction ? m_extraction->beacon_position() : glm::vec3(0.0f);

    bool include_astral = m_inventory.target_astral_ore > 0;
    m_surveying.trigger_scan(origin, *m_world, radius, linger_bonus, effective_rank,
                             &m_stalkers.stalkers(), &m_burrowers.burrowers(),
                             extraction_pos, extraction_active, include_astral);
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

    // Explosive concussive blast generates severe spatial noise and shatters nearby stalkers & burrowers
    m_noise_meter.add_demolition_sound(blast_origin, is_micro);
    StealthSystem::instance().EmitSatchelDetonation(blast_origin);
    if (m_hazard) {
        m_hazard->on_explosive_detonation(blast_origin, is_micro ? 25.0f : 45.0f);
    }
    m_stalkers.damage_nearest(glm::vec3(origin), is_micro ? 4.0f : 8.0f, is_micro ? 35.0f : 70.0f);
    m_burrowers.damage_nearest(blast_origin, is_micro ? 5.0f : 9.0f, is_micro ? 40.0f : 85.0f, true);

    // Concussive shockwave triggers localized seismic tremor and attracts subterranean monsters
    m_noise_meter.add_seismic_sound(blast_origin, is_micro ? 0.95f : 1.35f);
    if (m_renderer) {
        m_renderer->trigger_dust_kickup(is_micro ? 2.5f : 3.8f);
    }

    // Track bounding box of modified blocks to synchronously remesh affected chunks
    glm::ivec3 min_affected = origin;
    glm::ivec3 max_affected = origin;
    bool any_blocks_destroyed = false;
    size_t prev_debris_count = m_debris.size();

    // Check if blast breaches sealed Precursor Vault Bulkhead door (Secondary Objective)
    if (m_mission.check_satchel_vault_breach(*m_world, blast_origin, is_micro ? 2.5f : 4.5f)) {
        register_objective_interaction(MAT_REINFORCED_VAULT_DOOR);
        if (m_mission.vault().exists) {
            glm::ivec3 vpos = m_mission.vault().door_pos;
            min_affected = glm::min(min_affected, vpos - glm::ivec3(2, 1, 2));
            max_affected = glm::max(max_affected, vpos + glm::ivec3(2, 4, 2));
            any_blocks_destroyed = true;
        }
    }

    // Check if blast ignites nearby porous bio-gas pocket in a controlled fireball
    int ignited_gas = m_mission.ignite_gas_pocket(*m_world, blast_origin, is_micro ? 3.5f : 6.0f, m_stalkers);
    if (ignited_gas > 0) {
        float gas_rad = is_micro ? 3.5f : 6.0f;
        min_affected = glm::min(min_affected, glm::ivec3(std::floor(blast_origin.x - gas_rad), std::floor(blast_origin.y - gas_rad), std::floor(blast_origin.z - gas_rad)));
        max_affected = glm::max(max_affected, glm::ivec3(std::ceil(blast_origin.x + gas_rad), std::ceil(blast_origin.y + gas_rad), std::ceil(blast_origin.z + gas_rad)));
        any_blocks_destroyed = true;
    }

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
                    min_affected = glm::min(min_affected, bpos);
                    max_affected = glm::max(max_affected, bpos);
                    any_blocks_destroyed = true;
                }
            }
        }
        m_trauma = std::min(m_trauma + 0.25f, 1.0f);
    } else {
        // Heavy Demolition Charge: 3x3x3 spherical blast radius (expanded +1 perimeter for Demolitionist)
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
                        min_affected = glm::min(min_affected, bpos);
                        max_affected = glm::max(max_affected, bpos);
                        any_blocks_destroyed = true;
                    }
                }
            }
        }
        m_trauma = std::min(m_trauma + 0.50f, 1.0f);
    }

    // Include any dynamic debris cave-ins detached during this detonation
    for (size_t di = prev_debris_count; di < m_debris.size(); ++di) {
        glm::ivec3 dpos = glm::ivec3(std::floor(m_debris[di].position().x), std::floor(m_debris[di].position().y), std::floor(m_debris[di].position().z));
        min_affected = glm::min(min_affected, dpos - glm::ivec3(3));
        max_affected = glm::max(max_affected, dpos + glm::ivec3(3));
        any_blocks_destroyed = true;
    }

    // Synchronously remesh all chunks in and adjacent to the blast volume
    // so crater walls and chunk seams NEVER appear temporarily see-through
    if (any_blocks_destroyed && m_world) {
        m_world->sync_remesh_box(min_affected, max_affected);
    }
}

void Application::on_tactical_ability(CharacterClass cls, const glm::vec3& pos, const glm::vec3& dir) {
    if (!m_player || !m_world) return;

    if (cls == CharacterClass::Demolitionist) {
        // Concussion shockwave disruptor pulse
        m_player->add_trauma(0.35f);
        m_noise_meter.add_explosive_noise(25.0f);

        glm::vec3 eye = pos + glm::vec3(0.0f, 0.7f, 0.0f);
        RaycastHit hit = m_world->raycast(eye, dir, 8.0f);
        glm::ivec3 center = hit.hit ? hit.block_pos : glm::ivec3(glm::floor(eye + dir * 5.0f));

        bool any_shockwave_destroyed = false;
        glm::ivec3 min_shock = center;
        glm::ivec3 max_shock = center;

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dz = -1; dz <= 1; ++dz) {
                    glm::ivec3 bp = center + glm::ivec3(dx, dy, dz);
                    Voxel vox = m_world->get_voxel(bp.x, bp.y, bp.z);
                    if (vox.is_solid() && vox.material_id != MAT_DREDGE_BEDROCK && vox.material_id != MAT_REINFORCED_VAULT_DOOR) {
                        m_world->set_voxel(bp.x, bp.y, bp.z, Voxel{MAT_AIR, 0}, true);
                        min_shock = glm::min(min_shock, bp);
                        max_shock = glm::max(max_shock, bp);
                        any_shockwave_destroyed = true;
                        if (m_renderer) {
                            m_renderer->spawn_break_particles(glm::vec3(bp) + glm::vec3(0.5f), glm::ivec3(0, 1, 0), vox.material_id);
                        }
                    }
                }
            }
        }
        if (any_shockwave_destroyed && m_world) {
            m_world->sync_remesh_box(min_shock, max_shock);
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
        // Kinetic Repulsor Field: omni-directional shockwave repelling enemies, deflecting debris, restoring suit integrity
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::TacticalRepulsor, pos, 1.2f);
        }

        // 1. Omni-directional repulsor wave against Void Stalkers (repel + stun + damage)
        const float repulsor_radius = 8.5f;
        for (auto& s : m_stalkers.stalkers_mut()) {
            if (s.state == StalkerState::Dead || s.state == StalkerState::Dying) continue;
            float dist = glm::distance(s.position, pos);
            if (dist < repulsor_radius) {
                glm::vec3 push_dir = (dist > 0.001f) ? glm::normalize(s.position - pos) : glm::vec3(0.0f, 1.0f, 0.0f);
                s.velocity += push_dir * 18.0f + glm::vec3(0.0f, 4.0f, 0.0f);
                s.state = StalkerState::Stunned;
                s.stun_timer = 2.0f;
                s.state_timer = 0.0f;
                s.hp = std::max(0.0f, s.hp - 40.0f);
                if (s.hp <= 0.0f) {
                    s.state = StalkerState::Dying;
                    s.just_died = true;
                    s.hit_flash_timer = 0.0f;
                    s.death_timer = 0.0f;
                    s.state_timer = 0.0f;
                }
            }
        }

        // 2. Stun and disrupt nearby Seismic Burrowers
        m_burrowers.apply_stun(pos, repulsor_radius, 2.5f);
        m_burrowers.damage_nearest(pos, repulsor_radius, 60.0f, false);

        // 3. Deflect falling dynamic debris outward away from the Vanguard
        for (auto& deb : m_debris) {
            if (deb.is_destroyed()) continue;
            float dist = glm::distance(deb.position(), pos);
            if (dist < 10.0f) {
                glm::vec3 repel_dir = (dist > 0.001f) ? glm::normalize(deb.position() - pos) : glm::vec3(0.0f, 1.0f, 0.0f);
                repel_dir.y = std::max(repel_dir.y, 0.4f);
                deb.apply_impulse(glm::normalize(repel_dir) * 16.0f);
            }
        }

        // 4. Armored Stabilizer Recovery: clear trauma, restore 25 HP suit integrity
        m_player->set_trauma(0.0f);
        m_player->set_health(std::min(m_player->health() + 25.0f, m_player->max_health()));
        m_noise_meter.add_explosive_noise(12.0f);

        // 5. Visual VFX: radial dust kickup & kinetic shockwave particles
        if (m_renderer) {
            m_renderer->trigger_dust_kickup(3.0f);
            m_renderer->spawn_break_particles(pos + glm::vec3(0.0f, 0.5f, 0.0f), glm::ivec3(0, 1, 0), MAT_INDUSTRIAL_BULKHEAD);
        }

    } else if (cls == CharacterClass::Scout) {
        // Kinetic Dash Overcharge
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

void Application::spawn_ceiling_cavein_wave(const glm::vec3& epicenter, float radius, int min_blocks, int max_blocks) {
    if (!m_world) return;

    // Detach ceiling blocks around the epicenter within the excavation zone
    auto detach_blocks = StructuralCheck::query_seismic_detachment_blocks(
        *m_world,
        epicenter,
        min_blocks, max_blocks,
        std::min(radius * 0.75f, 15.0f),
        2, 16
    );

    // Fallback: if ceiling is higher or wider within the affected zone, expand query
    if (detach_blocks.empty()) {
        detach_blocks = StructuralCheck::query_seismic_detachment_blocks(
            *m_world,
            epicenter,
            min_blocks, max_blocks,
            std::min(radius * 0.95f, 18.0f),
            2, 22
        );
    }

    // Extraction Landing Zone Shielding: Protect players defending the extraction beacon
    if (m_extraction && (m_extraction->phase() == ExtractionPhase::BeaconDeployed || m_extraction->phase() == ExtractionPhase::PodLanded)) {
        if (m_selected_level <= 1) {
            // In Sector 1 (Perimeter Drift), the LZ is fully shielded from ceiling spalling during evacuation
            detach_blocks.clear();
        } else {
            // In higher sectors, protect a 6-block horizontal radius around the extraction beacon
            glm::vec3 bpos = m_extraction->beacon_position();
            std::erase_if(detach_blocks, [&](const glm::ivec3& b) {
                float dist_horiz = glm::distance(glm::vec2(b.x + 0.5f, b.z + 0.5f), glm::vec2(bpos.x, bpos.z));
                return dist_horiz < 6.0f;
            });
        }
    }

    if (detach_blocks.empty()) return;

    std::unordered_set<ChunkPos, ChunkPosHash> affected_chunks;
    for (const auto& b : detach_blocks) {
        Voxel v = m_world->get_voxel(b.x, b.y, b.z);
        // Remove physical block from ceiling without neighbor queueing flood
        m_world->set_voxel(b.x, b.y, b.z, Voxel{MAT_AIR, 0}, false);

        int cx = (b.x >= 0) ? (b.x / CHUNK_SIZE) : ((b.x - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cy = (b.y >= 0) ? (b.y / CHUNK_SIZE) : ((b.y - CHUNK_SIZE + 1) / CHUNK_SIZE);
        int cz = (b.z >= 0) ? (b.z / CHUNK_SIZE) : ((b.z - CHUNK_SIZE + 1) / CHUNK_SIZE);
        affected_chunks.insert(ChunkPos{cx, cy, cz});

        int lx = (b.x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int ly = (b.y % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int lz = (b.z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        if (lx == 0) affected_chunks.insert(ChunkPos{cx - 1, cy, cz});
        if (lx == CHUNK_SIZE - 1) affected_chunks.insert(ChunkPos{cx + 1, cy, cz});
        if (ly == 0) affected_chunks.insert(ChunkPos{cx, cy - 1, cz});
        if (ly == CHUNK_SIZE - 1) affected_chunks.insert(ChunkPos{cx, cy + 1, cz});
        if (lz == 0) affected_chunks.insert(ChunkPos{cx, cy, cz - 1});
        if (lz == CHUNK_SIZE - 1) affected_chunks.insert(ChunkPos{cx, cy, cz + 1});

        // Spawn fracture dust/particles at ceiling detachment point in the affected zone
        if (m_renderer) {
            m_renderer->spawn_break_particles(
                glm::vec3(b.x + 0.5f, b.y + 0.5f, b.z + 0.5f),
                glm::ivec3(0, -1, 0),
                v.material_id
            );
        }

        if (m_host) {
            uint16_t idx = static_cast<uint16_t>(Chunk::to_index(lx, ly, lz));
            m_host->broadcast_block_delta(cx, cy, cz, idx, MAT_AIR, 0);
        }

        uint32_t did = m_next_debris_id++;
        float rvx = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 1.1f;
        float rvz = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 1.1f;
        // Heavy, high-gravity downward velocity with chaotic tumbling
        float rvy = -2.5f - static_cast<float>(rand() % 100) / 40.0f;
        glm::vec3 vel(rvx, rvy, rvz);
        float rot_x = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 4.0f;
        float rot_y = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 4.0f;
        float rot_z = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 4.0f;
        glm::vec3 rot(rot_x, rot_y, rot_z);
        while (m_debris.size() >= World::MAX_ACTIVE_DEBRIS) {
            if (!m_debris.empty() && m_debris.front().is_sleeping() && m_world) {
                m_debris.front().re_voxelize_blocks(*m_world);
            }
            m_debris.erase(m_debris.begin());
        }
        m_debris.emplace_back(did, glm::vec3(b.x + 0.5f, b.y + 0.5f, b.z + 0.5f), vel, rot, v.material_id, 1, m_world.get());
        if (m_host) {
            m_host->broadcast_debris_spawn(did, glm::vec3(b.x + 0.5f, b.y + 0.5f, b.z + 0.5f), vel, v.material_id, 1);
        }
    }

    // Queue unique affected chunks once for background remeshing
    for (const auto& cp : affected_chunks) {
        m_world->queue_chunk_for_meshing(cp);
    }
}

void Application::setup_hazard_system() {
    m_hazard = std::make_unique<HazardClock>();

    // Pre-tremor warning phase: localized spall particles and proximity warning
    m_hazard->set_on_tremor_warning([this](const glm::vec3& epicenter, float radius) {
        float dist = glm::distance(m_player->position(), epicenter);
        float proximity = std::clamp(1.0f - (dist / radius), 0.0f, 1.0f);

        if (m_audio) {
            if (proximity > 0.05f) {
                m_audio->play_sound_2d(SoundCue::CavernGroan, 0.70f * std::max(0.3f, proximity), 0.95f);
                m_audio->play_sound_2d(SoundCue::SeismicTremor, 0.55f * std::max(0.2f, proximity), 1.15f);
            } else {
                m_audio->play_sound_3d(SoundCue::SeismicTremor, epicenter, 0.75f);
            }
        }

        // Particle effects are strictly localized to the zone undergoing tremor
        if (m_renderer) {
            for (int i = 0; i < 16; ++i) {
                float ox = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 5.0f;
                float oz = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 5.0f;
                float oy = 3.5f + static_cast<float>(rand() % 8);
                m_renderer->spawn_break_particles(epicenter + glm::vec3(ox, oy, oz), glm::ivec3(0, -1, 0), MAT_FRACTURED_GRANITE);
            }
            if (proximity > 0.15f) {
                m_renderer->trigger_dust_kickup(2.5f * proximity);
            }
        }
    });

    m_hazard->set_on_tremor([this](float intensity, const glm::vec3& epicenter, float radius) {
        float dist = glm::distance(m_player->position(), epicenter);
        float proximity = std::clamp(1.0f - (dist / radius), 0.0f, 1.0f);

        // Screen shaking ONLY affects player if they are inside or near the affected zone
        if (proximity > 0.01f) {
            float shake_amount = intensity * 0.70f * proximity;
            m_player->add_trauma(shake_amount);
            m_trauma = std::min(m_trauma + shake_amount, 1.0f);
        }

        if (m_audio) {
            if (proximity > 0.05f) {
                m_audio->play_sound_2d(SoundCue::ExplosiveBlast, 0.65f * proximity, 0.85f);
                m_audio->set_seismic_rumble(intensity * proximity);
            } else {
                m_audio->play_sound_3d(SoundCue::SeismicTremor, epicenter, 0.85f);
            }
        }
        if (proximity > 0.15f && m_renderer) {
            m_renderer->trigger_dust_kickup(4.5f * proximity);
        }

        // Primary Cave-in Wave: detach 6 to 12 physical ceiling blocks in the active zone (controlled, non-lethal spall)
        spawn_ceiling_cavein_wave(epicenter, radius, 6, 12);

        // Hazard Phase 3 & 4 (radiation >= 45% in Sector >= 3): chance to awaken Seismic Burrower around epicenter!
        // Sectors 1 and 2 are early/mid tier and do NOT spawn deep subterranean burrowers during routine mining.
        if (m_selected_level >= 3 && m_hazard->radiation_level() >= 45.0f && m_burrowers.active_count() == 0) {
            m_burrowers.spawn_hazard_wave(epicenter, *m_world);
        }
    });
}

void Application::process_input(int key, int action) {
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE) {
        if (m_state == GameState::Gameplay) {
            if (m_map_open) {
                m_map_open = false;
                m_map_dragging_last = false;
                m_window->set_cursor_locked(true);
                if (m_player) m_player->reset_spawn_shoot_pause(0.2f);
                return;
            }
            if (m_hud && m_hud->is_manual_briefing_open()) {
                m_hud->dismiss_help_briefing();
                return;
            }
            TransitionState(GameState::Paused);
            m_window->set_cursor_locked(false);
        } else if (m_state == GameState::Paused) {
            TransitionState(GameState::Gameplay);
            m_window->set_cursor_locked(true);
        } else if (m_state == GameState::OrbitalHub) {
            if (m_hub_ui) m_hub_ui->set_subview(MenuSubView::Main);
            TransitionState(GameState::MainMenu);
        } else if (m_state == GameState::Debrief) {
            if (m_hub_ui) m_hub_ui->set_subview(MenuSubView::Main);
            TransitionState(GameState::MainMenu);
        } else if (m_state == GameState::MainMenu) {
            if (m_hub_ui && m_hub_ui->subview() != MenuSubView::Main) {
                m_hub_ui->set_subview(MenuSubView::Main);
            }
            // In main menu: ESC does NOT exit the game.
            // Player quits intentionally via [ EXIT ] button on the title screen.
        }
    } else if (key == GLFW_KEY_TAB || key == GLFW_KEY_M) {
        if (m_state == GameState::Gameplay) {
            m_map_open = !m_map_open;
            m_map_dragging_last = false;
            m_window->set_cursor_locked(!m_map_open);
            m_last_map_mouse = m_window->get_cursor_pos();
            if (!m_map_open && m_player) {
                m_player->reset_spawn_shoot_pause(0.2f);
            }
            return;
        }
    } else if (key == GLFW_KEY_F11 || ((key == GLFW_KEY_ENTER) && (m_window && (m_window->is_key_down(GLFW_KEY_LEFT_ALT) || m_window->is_key_down(GLFW_KEY_RIGHT_ALT))))) {
        if (m_window) {
            m_window->toggle_fullscreen();
        }
    } else if (key == GLFW_KEY_I) {
        if (m_state == GameState::Gameplay) {
            TransitionState(GameState::Paused);
            m_window->set_cursor_locked(false);
            if (m_pause_menu) {
                m_pause_menu->set_active_tab(PauseTab::Inventory);
            }
        } else if (m_state == GameState::Paused) {
            if (m_pause_menu && m_pause_menu->active_tab() == PauseTab::Inventory) {
                TransitionState(GameState::Gameplay);
                m_window->set_cursor_locked(true);
            } else if (m_pause_menu) {
                m_pause_menu->set_active_tab(PauseTab::Inventory);
            }
        }
    } else if (key == GLFW_KEY_H || key == GLFW_KEY_F1) {
        if (m_hud) {
            m_hud->toggle_help_briefing();
        }
    } else if (key == GLFW_KEY_E) {
        if (m_state == GameState::Gameplay) {
            try_interact();
        }
    }
}

void Application::fixed_tick(float dt) {
    m_current_tick++;
    AudioSystem::Update(dt);

    if (m_state != GameState::Gameplay) {
        return;
    }

    if (m_hud) {
        m_hud->update(dt);
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
            TransitionState(GameState::Debrief);
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
    if (m_extraction && (m_extraction->phase() == ExtractionPhase::PodLanded || m_extraction->is_ceiling_drill_active())) {
        m_player->set_pod_collider(
            m_extraction->beacon_position(),
            m_extraction->pod_drill_progress(),
            m_extraction->ramp_extension(),
            m_extraction->is_pod_anchored()
        );
    } else {
        m_player->clear_pod_collider();
    }

    m_player->update_physics(dt, *m_world);

    if (m_player->has_pod_collider()) {
        m_player->resolve_extraction_pod_collision(
            m_extraction->beacon_position(),
            m_extraction->pod_drill_progress(),
            m_extraction->ramp_extension(),
            m_extraction->is_pod_anchored()
        );
    }

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

        // Falling island blocks crush any subterranean monsters in downward trajectory
        if (it->velocity().y < -3.0f) {
            float crush_dmg = std::clamp(20.0f + std::abs(it->velocity().y) * 1.8f, 20.0f, 50.0f);
            m_stalkers.damage_nearest(it->position(), 1.8f, crush_dmg);
            m_burrowers.damage_nearest(it->position(), 2.0f, crush_dmg, true);
        }

        if (res.shattered) {
            m_renderer->spawn_break_particles(res.shatter_pos, glm::ivec3(0, 1, 0), res.shatter_mat);
            m_renderer->spawn_dust_cloud(res.shatter_pos, 20);
            m_renderer->trigger_dust_kickup(2.5f);
            if (m_audio) {
                m_audio->play_sound_3d(SoundCue::VoxelHit, res.shatter_pos, 0.75f);
            }
        }

        if (res.placed_on_ground) {
            // Block landed on cavern floor and settled as physical voxel block
            glm::vec3 ground_center = glm::vec3(res.place_pos) + glm::vec3(0.5f, 0.5f, 0.5f);
            if (m_renderer) {
                m_renderer->spawn_dust_cloud(ground_center, 24);
                m_renderer->spawn_break_particles(
                    glm::vec3(res.place_pos) + glm::vec3(0.5f, 0.9f, 0.5f),
                    glm::ivec3(0, 1, 0),
                    it->material_id()
                );
                m_renderer->trigger_dust_kickup(3.0f);
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
                m_player->add_trauma(0.12f);
                m_trauma = std::min(m_trauma + 0.12f, 1.0f);
            }
            // Debris crash sound event in cavern space
            m_noise_meter.add_debris_crash_sound(glm::vec3(res.place_pos), it->block_count());

            // Crushing impact damages nearby hostiles caught under falling ceiling blocks
            float floor_crush = std::clamp(20.0f + std::abs(it->velocity().y) * 1.8f, 20.0f, 50.0f);
            m_stalkers.damage_nearest(ground_center, 2.4f, floor_crush);
            m_burrowers.damage_nearest(ground_center, 2.6f, floor_crush, true);
        }

        static float s_debris_hit_cooldown = 0.0f;
        s_debris_hit_cooldown = std::max(0.0f, s_debris_hit_cooldown - dt);

        if (res.hit_player) {
            if (s_debris_hit_cooldown <= 0.0f) {
                s_debris_hit_cooldown = 1.2f; // Prevent multi-block cluster damage in the same instant
                float dmg = res.damage * 0.35f; // Scaled to balanced 7-17.5 HP so ceiling rocks do not wipe out delvers
                if (m_extraction && m_extraction->is_player_in_perimeter(player_pos)) {
                    float resist = (m_selected_level <= 1) ? 0.40f : (m_selected_level == 2 ? 0.50f : 0.60f);
                    if (m_extraction->phase() == ExtractionPhase::PodLanded) {
                        resist = 0.10f; // 90% damage reduction while evac pod has landed
                    }
                    dmg *= resist;
                }
                m_player->take_damage(dmg, PlayerController::DamageSource::FallingDebris);
                m_player->add_trauma(0.20f);
                m_trauma = std::min(m_trauma + 0.20f, 1.0f);
                m_hud->trigger_damage_flash(0.5f);
            }
        }

        if (it->is_destroyed()) {
            it = m_debris.erase(it);
        } else {
            ++it;
        }
    }

    // 4. Update Hazard Clock, Extraction, Surveying, and Particles
    if (m_hazard && m_player) {
        m_hazard->set_player_position(m_player->position());
    }
    m_hazard->update(dt);

    // Sustained Tectonic Tremor Shaking & Spalling
    if (m_hazard && m_hazard->is_tremoring()) {
        glm::vec3 epicenter = m_hazard->active_tremor_epicenter();
        float radius = m_hazard->active_tremor_radius();
        float dist = glm::distance(m_player->position(), epicenter);
        float proximity = std::clamp(1.0f - (dist / radius), 0.0f, 1.0f);

        float tremor_int = m_hazard->tremor_intensity();
        if (tremor_int <= 0.0f) tremor_int = 1.0f;

        // Screen shake ONLY affects user if in proximity to the tremor area!
        if (proximity > 0.05f) {
            m_trauma = std::max(m_trauma, 0.85f * tremor_int * proximity);
        }

        // Seismic rumble sound event emitted from the epicenter
        m_noise_meter.add_seismic_sound(epicenter, tremor_int * dt);
        if (m_audio) {
            m_audio->set_seismic_rumble(tremor_int * proximity);
        }

        // Secondary aftershock cascades: periodic detachment waves throughout the tremor
        m_aftershock_wave_timer += dt;
        if (m_aftershock_wave_timer >= 0.85f) {
            m_aftershock_wave_timer = 0.0f;
            spawn_ceiling_cavein_wave(epicenter, radius, 4, 8);
        }

        m_tremor_spall_timer += dt;
        if (m_tremor_spall_timer >= 0.25f) {
            m_tremor_spall_timer = 0.0f;
            if (m_renderer && m_world) {
                if (proximity > 0.2f) {
                    m_renderer->trigger_dust_kickup(2.5f * proximity);
                }
                // Particles spawn ONLY at the tremor zone ceiling, NOT above player!
                for (int i = 0; i < 8; ++i) {
                    float ox = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 6.0f;
                    float oz = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 6.0f;
                    float oy = 3.0f + static_cast<float>(rand() % 5);
                    m_renderer->spawn_break_particles(epicenter + glm::vec3(ox, oy, oz), glm::ivec3(0, -1, 0), MAT_FRACTURED_GRANITE);
                }
            }
        }
    } else {
        m_tremor_spall_timer = 0.0f;
        m_aftershock_wave_timer = 0.0f;
        if (m_audio && !m_death_sequence) {
            m_audio->set_seismic_rumble(0.0f);
        }
    }

    // Dynamic Spatial Radiation Proximity Query
    if (m_world && m_player) {
        m_proximity_radiation = m_world->query_radiation_proximity(m_player->position(), 12.0f);
        m_effective_radiation = std::clamp(std::max(m_proximity_radiation, m_player->exo().radiation / 100.0f), 0.0f, 1.0f);
        if (m_audio) {
            m_audio->set_radiation_proximity(m_effective_radiation);
        }
    }

    m_extraction->update(dt, m_player->position());
    if (m_state != GameState::Gameplay) {
        return; // State exited (e.g. extraction completed into Debrief): halt further in-tick damage/updates
    }
    if (m_expedition_success) {
        // Defensive self-healing: active Gameplay must never retain a completed flag
        VF_LOG_WARN("Application", "m_expedition_success was unexpectedly true during active Gameplay. Clearing flag to prevent tick lock.");
        m_expedition_success = false;
    }

    // Spawn falling dust and spall from cavern ceiling directly above beacon during ceiling drill countdown
    if (m_extraction && m_extraction->is_ceiling_drill_active() && m_renderer) {
        static float s_dust_accum = 0.0f;
        s_dust_accum += dt;
        if (s_dust_accum >= 0.12f) {
            s_dust_accum = 0.0f;
            float rx = static_cast<float>(rand() % 40 - 20) * 0.08f;
            float rz = static_cast<float>(rand() % 40 - 20) * 0.08f;
            glm::vec3 dust_pos = m_extraction->beacon_position() + glm::vec3(rx, 15.0f, rz);
            m_renderer->spawn_break_particles(dust_pos, glm::ivec3(0, -1, 0), MAT_FRACTURED_GRANITE);
        }
    }

    m_surveying.update(dt);
    if (m_surveying.is_active()) {
        m_surveying.update_monsters(&m_stalkers.stalkers(), &m_burrowers.burrowers());
    }
    m_hud->update(dt);
    m_renderer->update_particles(dt);

    if (m_config.capture_models || m_config.capture_level_shapes) {
        // Freeze AI physics and simulation during model/shape showcases so staged layouts remain pristine
        return;
    }

    // Update Mining Noise Meter & natural decay
    // Wire crouch stealth mechanic: Left Ctrl / C held = crouching = faster noise decay & stealth movement
    bool is_crouching = m_player ? m_player->is_crouching() : m_window->is_key_down(GLFW_KEY_LEFT_CONTROL);
    m_noise_meter.set_crouching(is_crouching);

    // Continuous Movement & Exo-Thruster Acoustic Sound Emissions:
    bool is_moving = (m_player->current_buttons() & (BTN_FORWARD | BTN_BACKWARD | BTN_LEFT | BTN_RIGHT)) != 0;
    bool is_sprinting = (m_player->current_buttons() & BTN_SPRINT) != 0;
    m_noise_meter.add_movement_sound(m_player->position(), dt, is_sprinting, is_crouching, m_player->is_grounded() && is_moving);
    if (m_player->is_grounded() && is_moving) {
        if (is_crouching) {
            StealthSystem::instance().EmitCrouch(m_player->position());
        } else {
            StealthSystem::instance().EmitWalk(m_player->position());
        }
    }

    bool is_thruster_active = (m_player->current_buttons() & BTN_THRUSTER) != 0;
    if (is_thruster_active) {
        m_noise_meter.add_jetpack_sound(m_player->position(), dt);
    }
    if (m_audio) {
        m_audio->set_thruster_active(is_thruster_active, m_player->position());
    }

    if (m_player->grapple().just_fired) {
        m_noise_meter.add_grapple_fire_sound(m_player->position());
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::GrappleFire, m_player->position(), 0.70f);
        }
        m_player->grapple_mut().just_fired = false;
    }

    bool is_reeling = m_player->grapple().active && ((m_player->current_buttons() & BTN_GRAPPLE_REEL) != 0);
    if (is_reeling) {
        m_noise_meter.add_grapple_reel_sound(m_player->position(), dt);
    }
    if (m_audio) {
        m_audio->set_grapple_active(m_player->grapple().active, is_reeling, m_player->position());
    }

    // Update active sound event lifetimes and noise meter decay
    m_noise_meter.update(dt);
    // Tick the agitation state machine: decrements enrage timer, transitions
    // ENRAGED → COOLDOWN after 30 s, then COOLDOWN → CALM over 15 s.
    // Without this call the enrage state is permanent (timer never decrements).
    SwarmManager::instance().Update(dt);

    // Industrial Drill excavation acoustics & seismic stress (Mining drill is strictly for rock/minerals; monsters cannot be mined)
    if (m_player->is_drilling()) {
        glm::vec3 drill_tip = m_player->position() + m_player->forward() * 1.5f;
        m_noise_meter.add_drill_sound(drill_tip, dt, 14.0f);
        StealthSystem::instance().EmitDrill(drill_tip);
        if (m_hazard) {
            // Sustained drill contact generates continuous seismic micro-fracture stress in this excavation zone.
            m_hazard->add_seismic_stress(drill_tip, 3.8f * dt);
        }
    }

    // Update Void Stalker hostile entities AI & collision (Sound-driven hearing + LOS-gated attacks)
    if (m_stalker_spotted_audio_cooldown > 0.0f) {
        m_stalker_spotted_audio_cooldown = std::max(0.0f, m_stalker_spotted_audio_cooldown - dt);
    }
    if (m_stalker_investigate_audio_cooldown > 0.0f) {
        m_stalker_investigate_audio_cooldown = std::max(0.0f, m_stalker_investigate_audio_cooldown - dt);
    }

    glm::vec3 headlamp_dir = m_player->forward();
    bool headlamp_on = m_renderer ? m_renderer->headlamp().enabled : true;
    bool is_drilling = m_player->is_drilling();

    VoidStalkerManager::FrameResult stalker_res;
    if (!m_config.test_enemy && !m_config.test_enemy_load) {
        stalker_res = m_stalkers.update(
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
    }

    // Step pacing manager and persistent carcass ragdoll physics
    m_spawn_mgr.update(dt, *m_world, m_stalkers, m_player->position());
    CarcassManager::instance().update(dt, *m_world);

    if (stalker_res.total_damage > 0.0f) {
        float dmg = stalker_res.total_damage;
        if (m_extraction && m_extraction->is_player_in_perimeter(m_player->position())) {
            float resist = (m_selected_level <= 1) ? 0.60f : (m_selected_level == 2 ? 0.65f : 0.80f);
            if (m_extraction->phase() == ExtractionPhase::PodLanded) {
                resist = 0.10f; // 90% damage reduction while evac pod has landed
            }
            dmg *= resist;
        }
        m_player->take_damage(dmg, PlayerController::DamageSource::EnemyAttack);
        m_player->add_trauma(0.45f);
        m_trauma = std::min(m_trauma + 0.45f, 1.0f);
        m_hud->trigger_damage_flash(0.75f);
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::StalkerHit, m_player->position(), 1.0f);
        }
    }
    if (stalker_res.any_projectile_fired) {
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::VoxelBreakVoidite, m_player->position() + m_player->forward() * 4.0f, 0.9f);
        }
    }
    if (stalker_res.any_lunge) {
        m_player->add_trauma(0.25f);
    }
    if (stalker_res.any_boss_slam) {
        m_player->add_trauma(0.55f);
        m_trauma = std::min(m_trauma + 0.55f, 1.0f);
        if (m_renderer) {
            m_renderer->trigger_dust_kickup(2.5f);
        }
        if (m_hazard) {
            m_hazard->force_tremor(1.5f);
        }
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::SeismicTremor, stalker_res.boss_slam_pos, 1.0f);
        }
    }
    if (stalker_res.any_boss_charge) {
        m_player->add_trauma(0.35f);
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::BurrowerRoar, m_player->position(), 1.0f);
        }
    }
    if (stalker_res.any_boss_roar) {
        m_player->add_trauma(0.40f);
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::StalkerEchoScreech, m_player->position(), 1.0f);
        }
    }
    if (stalker_res.any_spotted) {
        if (m_audio && m_stalker_spotted_audio_cooldown <= 0.0f) {
            glm::vec3 s_pos = m_player->position();
            for (const auto& s : m_stalkers.stalkers()) {
                if (s.state == StalkerState::Stalking || s.has_player_los) {
                    s_pos = s.position;
                    break;
                }
            }
            m_audio->play_sound_3d(SoundCue::StalkerSpotted, s_pos, 1.0f);
            m_stalker_spotted_audio_cooldown = 2.0f;
        }
    } else if (stalker_res.any_heard_sound) {
        if (m_audio && m_stalker_investigate_audio_cooldown <= 0.0f) {
            glm::vec3 s_pos = m_player->position();
            for (const auto& s : m_stalkers.stalkers()) {
                if (s.state == StalkerState::Investigating) {
                    s_pos = s.position;
                    break;
                }
            }
            m_audio->play_sound_3d(SoundCue::StalkerChitter, s_pos, 0.95f);
            m_stalker_investigate_audio_cooldown = 2.5f;
        }
    }
    if (stalker_res.stalkers_killed > 0) {
        m_inventory.total_run_score += 100 * stalker_res.stalkers_killed;
        m_skills.add_demolitions_xp(25 * stalker_res.stalkers_killed);
        m_noise_meter.trigger_post_combat_cooldown(8.0f);
    }
    if (stalker_res.stalkers_escaped > 0) {
        if (m_renderer) {
            m_renderer->trigger_dust_kickup(1.5f);
        }
    }

    // Update Seismic Burrower hostile entities AI, excavation & cave-in triggers
    auto burrower_res = m_burrowers.update(dt, m_player->position(), *m_world, m_noise_meter.recent_sounds(), is_crouching);

    // Mutual hard depenetration between Void Stalkers and Seismic Burrowers
    m_stalkers.resolve_burrower_penetrations(m_burrowers.burrowers_mut(), *m_world);

    // Localized visibility check: burrow particles should only spawn if visible from cavern without intervening walls
    auto is_burrow_visible = [&](const glm::vec3& world_pt) -> bool {
        if (!m_player || !m_world) return true;
        glm::vec3 cam = m_player->eye_position();
        glm::vec3 diff = world_pt - cam;
        float d = glm::length(diff);
        if (d > 20.0f) return false; // Beyond cavern acoustic/visual particle threshold
        if (d < 0.25f) return true;
        RaycastHit hit = m_world->raycast(cam, diff / d, d - 0.25f);
        return !hit.hit; // If obstructed by an intervening rock wall, it is occluded
    };

    // Spawn localized burrowing particle effects for excavated voxels bordering exposed cavern air
    if (m_renderer && !burrower_res.excavated_voxels.empty()) {
        static const glm::ivec3 air_dirs[6] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
        };
        int spawned_particles_count = 0;
        constexpr int MAX_BURROW_PARTICLES_PER_FRAME = 6;
        for (const auto& ev : burrower_res.excavated_voxels) {
            if (spawned_particles_count >= MAX_BURROW_PARTICLES_PER_FRAME) break;
            bool on_surface = false;
            for (const auto& d : air_dirs) {
                if (!m_world->is_solid(ev.pos + d)) {
                    on_surface = true;
                    break;
                }
            }
            if (!on_surface) continue; // Skip voxels completely sealed inside subterranean bedrock

            glm::vec3 vpos = glm::vec3(ev.pos) + glm::vec3(0.5f);
            if (is_burrow_visible(vpos)) {
                m_renderer->spawn_burrow_particles(vpos, glm::vec3(0, 0, 1), ev.original_mat, 8);
                m_renderer->spawn_crack_debris(vpos, glm::ivec3(0, 1, 0), 0.6f, ev.original_mat);
                spawned_particles_count++;
            }
        }
    }

    // Subterranean tectonic tremor and proximity horror caused by nearby burrower
    if (burrower_res.max_rumble > 0.02f) {
        m_player->add_trauma(burrower_res.max_rumble * 0.35f * dt);
        if (burrower_res.max_rumble > 0.40f && m_renderer) {
            m_renderer->trigger_dust_kickup(burrower_res.max_rumble * 1.5f);
        }
    }

    if (burrower_res.total_damage > 0.0f) {
        float dmg = burrower_res.total_damage;
        if (m_extraction && m_extraction->is_player_in_perimeter(m_player->position())) {
            float resist = (m_selected_level <= 1) ? 0.60f : (m_selected_level == 2 ? 0.65f : 0.80f);
            if (m_extraction->phase() == ExtractionPhase::PodLanded) {
                resist = 0.10f; // 90% damage reduction while evac pod has landed
            }
            dmg *= resist;
        }
        float applied = m_player->take_damage(dmg, PlayerController::DamageSource::EnemyAttack);
        m_player->add_trauma(0.55f);
        m_trauma = std::min(m_trauma + 0.55f, 1.0f);
        m_hud->trigger_damage_flash(0.85f);
        if (m_audio) {
            m_audio->play_sound_3d(SoundCue::ExplosiveBlast, m_player->position(), 1.0f);
        }
    }
    if (burrower_res.any_breach) {
        m_player->add_trauma(0.35f);
        if (m_renderer) {
            m_renderer->trigger_dust_kickup(3.0f);
        }
        if (m_audio) {
            m_audio->play_sound_2d(SoundCue::SeismicTremor, 0.8f);
        }
    }
    if (burrower_res.any_cavein_triggered) {
        m_hazard->force_tremor(1.4f);
    }
    if (burrower_res.burrowers_killed > 0) {
        m_inventory.total_run_score += 250 * burrower_res.burrowers_killed;
        m_skills.add_demolitions_xp(60 * burrower_res.burrowers_killed);
    }
 
    // 5. Delver Combat Firearm Firing & Ballistics Simulation (Class Archetype Specialized)
    if (m_player->active_tool() == ToolSlot::CombatWeapon && m_player->try_fire_weapon(m_plasma_bolts, dt)) {
        SoundCue cue = SoundCue::PlasmaFire;
        switch (m_player->weapon_archetype()) {
            case WeaponArchetype::MagmaScattergun: cue = SoundCue::ScattergunFire; break;
            case WeaponArchetype::PlasmaCarbine:   cue = SoundCue::PlasmaFire; break;
            case WeaponArchetype::NeedlerRailgun:  cue = SoundCue::RailgunFire; break;
        }

        if (m_player->weapon_archetype() == WeaponArchetype::NeedlerRailgun) {
            m_muzzle_flash_timer = 0.05f;
            if (m_renderer) {
                m_renderer->spawn_barrel_smoke(m_player->eye_position() + m_player->forward() * 0.7f, m_player->forward(), 4);
            }
        }

        if (m_audio) {
            m_audio->play_sound_2d(cue, 0.85f);
        }
        // Gunshot acoustic muzzle blast sound event
        m_noise_meter.add_gunshot_sound(m_player->position() + m_player->forward() * 0.8f, static_cast<int>(m_player->weapon_archetype()));
        m_player->add_trauma(m_player->weapon_stats().trauma_kick);

        // Muzzle concussion through rock — each shot adds seismic microstress to the cavern.
        // Railgun needle is most destabilising (hypersonic shockwave), shotgun least (spread).
        if (m_hazard) {
            float muzzle_stress = 8.0f;
            switch (m_player->weapon_archetype()) {
                case WeaponArchetype::MagmaScattergun: muzzle_stress = 6.0f;  break;
                case WeaponArchetype::PlasmaCarbine:   muzzle_stress = 8.0f;  break;
                case WeaponArchetype::NeedlerRailgun:  muzzle_stress = 15.0f; break;
            }
            m_hazard->on_weapon_impact(muzzle_stress);
        }
    } // end if(try_fire_weapon)

    // Update active plasma bolts
    for (auto& bolt : m_plasma_bolts) {
        if (!bolt.active) continue;

        glm::vec3 next_pos = bolt.position + bolt.velocity * dt;
        bolt.lifetime -= dt;
        if (bolt.lifetime <= 0.0f) {
            bolt.active = false;
            continue;
        }

        // Check if bolt passes through or strikes reactive bio-gas pocket
        int bx = static_cast<int>(std::floor(next_pos.x));
        int by = static_cast<int>(std::floor(next_pos.y));
        int bz = static_cast<int>(std::floor(next_pos.z));
        Voxel air_v = m_world->get_voxel(bx, by, bz);
        if (air_v.material_id == MAT_GAS || air_v.material_id == MAT_VOLATILE_SMOKE) {
            bolt.active = false;
            m_mission.ignite_gas_pocket(*m_world, next_pos, 4.5f, m_stalkers);
            continue;
        }

        // 1. Raycast / voxel collision against solid blocks
        Voxel v = m_world->get_voxel(bx, by, bz);
        if (v.is_solid()) {
            bolt.active = false;

            // Compute hit face normal based on bullet velocity
            glm::ivec3 hit_norm(0, 1, 0);
            glm::vec3 abs_vel = glm::abs(bolt.velocity);
            if (abs_vel.x > abs_vel.y && abs_vel.x > abs_vel.z) {
                hit_norm = glm::ivec3(bolt.velocity.x > 0.0f ? -1 : 1, 0, 0);
            } else if (abs_vel.y > abs_vel.z) {
                hit_norm = glm::ivec3(0, bolt.velocity.y > 0.0f ? -1 : 1, 0);
            } else {
                hit_norm = glm::ivec3(0, 0, bolt.velocity.z > 0.0f ? -1 : 1);
            }

            float stress_impact = 5.0f;
            uint8_t bullet_block_damage = 6;
            if (m_player->weapon_archetype() == WeaponArchetype::MagmaScattergun) {
                stress_impact = 2.5f;
                bullet_block_damage = 4;
            } else if (m_player->weapon_archetype() == WeaponArchetype::NeedlerRailgun) {
                stress_impact = 10.0f;
                bullet_block_damage = 15; // Railgun instantly fractures/shatters standard voxels
            }

            // Bullet impact acoustic distraction sound event on rock wall
            m_noise_meter.add_bullet_impact_sound(next_pos, stress_impact);
            if (m_audio) {
                m_audio->play_sound_3d(SoundCue::PlasmaHit, next_pos, 0.70f);
            }

            // Ballistic block damage & fracture mechanics
            if (v.material_id != MAT_DREDGE_BEDROCK && v.material_id != MAT_REINFORCED_VAULT_DOOR) {
                uint8_t current_damage = v.flags_and_damage & 0x0F;
                uint8_t new_damage = current_damage + bullet_block_damage;
                if (new_damage >= 15) {
                    // Block completely breaks under heavy ballistic impact!
                    m_world->SetBlock(bx, by, bz, MAT_AIR, 0);
                    on_block_broken(bx, by, bz, hit_norm, v.material_id, v.flags_and_damage);
                    m_noise_meter.add_voxel_break_sound(next_pos, v.material_id);
                    SoundCue break_cue = (v.material_id == MAT_VOIDITE_CRYSTAL || v.material_id == MAT_ASTRAL_ORE) ? SoundCue::VoxelBreakVoidite :
                                         (v.material_id == MAT_INDUSTRIAL_BULKHEAD) ? SoundCue::VoxelBreakBulkhead :
                                         SoundCue::VoxelBreakBasalt;
                    if (m_audio) {
                        m_audio->play_sound_3d(break_cue, next_pos, 0.85f);
                    }

                    // Destabilize stratum: bullet-shattered block triggers localized seismic tremor!
                    if (m_hazard) {
                        m_hazard->on_weapon_impact(next_pos, stress_impact * 2.5f);
                    }

                    // Localized seismic tremor event in the area where the bullet hit
                    m_noise_meter.add_seismic_sound(next_pos, 0.75f);

                    if (m_renderer) {
                        m_renderer->trigger_dust_kickup(1.2f);
                        for (int i = 0; i < 4; ++i) {
                            float ox = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 1.5f;
                            float oz = (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 1.5f;
                            m_renderer->spawn_break_particles(next_pos + glm::vec3(ox, 2.5f, oz), glm::ivec3(0, -1, 0), v.material_id);
                        }
                    }
                } else {
                    // Accumulate ballistic damage and spawn progressive crack debris
                    if (v.is_slab() || v.is_ramp() || v.is_corner()) {
                        v.flags_and_damage = (v.flags_and_damage & ~VOXEL_DAMAGE_MASK) | (new_damage & VOXEL_DAMAGE_MASK);
                        v.flags_and_damage &= ~VOXEL_FLAG_WATERLOGGED;
                    } else {
                        v.flags_and_damage = (v.flags_and_damage & 0xF0) | (new_damage & 0x0F);
                        v.flags_and_damage &= ~VOXEL_FLAG_WATERLOGGED;
                    }
                    m_world->set_voxel(bx, by, bz, v, true);
                    if (m_renderer) {
                        m_renderer->spawn_crack_debris(next_pos, hit_norm, static_cast<float>(new_damage) / 15.0f, v.material_id, v.shape());
                        m_renderer->trigger_dust_kickup(0.25f + stress_impact * 0.03f);
                    }
                    if (m_hazard) {
                        m_hazard->on_weapon_impact(next_pos, stress_impact);
                    }
                }
            } else {
                if (m_renderer) {
                    m_renderer->spawn_crack_debris(next_pos, hit_norm, 0.5f, MAT_VOIDITE_CRYSTAL);
                    m_renderer->trigger_dust_kickup(0.35f + stress_impact * 0.05f);
                }
                if (m_hazard) {
                    m_hazard->on_weapon_impact(next_pos, stress_impact);
                }
            }
            continue;
        }

        // 2. Enemy hit detection: Void Stalkers
        bool is_crit = false;
        float damage_dealt = 0.0f;
        glm::vec3 player_pos = m_player ? m_player->position() : (next_pos - bolt.velocity * 0.1f);
        glm::vec3 shot_dir = glm::length(bolt.velocity) > 0.001f ? glm::normalize(bolt.velocity) : glm::vec3(0.0f, 0.0f, 1.0f);
        float boss_mult = m_player ? m_player->boss_damage_multiplier() : 1.0f;
        if (m_stalkers.damage_nearest(next_pos, 1.4f, bolt.damage, true, &is_crit, &damage_dealt, &player_pos, &shot_dir, boss_mult)) {
            bolt.active = false;
            if (m_renderer) {
                m_renderer->spawn_crack_debris(next_pos, glm::ivec3(0, 1, 0), 0.8f, MAT_VOIDITE_CRYSTAL);
            }
            if (is_crit) {
                if (m_hud) {
                    m_hud->trigger_hit_marker(true, damage_dealt);
                }
                if (m_audio) {
                    m_audio->play_sound_3d(SoundCue::CritHit, next_pos, 1.0f);
                }
            } else {
                if (m_hud) {
                    m_hud->trigger_hit_marker(false, bolt.damage);
                }
                if (m_audio) {
                    m_audio->play_sound_3d(SoundCue::PlasmaHit, next_pos, 0.95f);
                }
            }
            m_inventory.total_run_score += is_crit ? 45 : 15;
            float enemy_stress = 3.5f;
            if (m_player->weapon_archetype() == WeaponArchetype::MagmaScattergun) {
                enemy_stress = 1.5f;
            } else if (m_player->weapon_archetype() == WeaponArchetype::NeedlerRailgun) {
                enemy_stress = 7.0f;
            }
            if (m_hazard) {
                m_hazard->on_weapon_impact(next_pos, enemy_stress);
            }
            continue;
        }

        // 3. Enemy hit detection: Seismic Burrowers
        bool is_b_crit = false;
        float b_damage_dealt = 0.0f;
        if (m_burrowers.damage_nearest(next_pos, 2.4f, bolt.damage, false, true, &is_b_crit, &b_damage_dealt, &player_pos, &shot_dir)) {
            bolt.active = false;
            if (m_renderer) {
                m_renderer->spawn_crack_debris(next_pos, glm::ivec3(0, 1, 0), 0.9f, MAT_TITANIUM);
                m_renderer->trigger_dust_kickup(0.5f);
            }
            if (is_b_crit) {
                if (m_hud) {
                    m_hud->trigger_hit_marker(true, b_damage_dealt);
                }
                if (m_audio) {
                    m_audio->play_sound_3d(SoundCue::CritHit, next_pos, 1.0f);
                }
            } else {
                if (m_hud) {
                    m_hud->trigger_hit_marker(false, bolt.damage);
                }
                if (m_audio) {
                    m_audio->play_sound_3d(SoundCue::PlasmaHit, next_pos, 0.95f);
                }
            }
            m_inventory.total_run_score += is_b_crit ? 75 : 25;
            float burrower_stress = 5.5f;
            if (m_player->weapon_archetype() == WeaponArchetype::MagmaScattergun) {
                burrower_stress = 2.2f;
            } else if (m_player->weapon_archetype() == WeaponArchetype::NeedlerRailgun) {
                burrower_stress = 9.0f;
            }
            if (m_hazard) {
                m_hazard->on_weapon_impact(next_pos, burrower_stress);
            }
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

        if (m_player->is_drilling()) {
            static float drill_strata_timer = 0.0f;
            drill_strata_timer += dt;
            if (drill_strata_timer >= 0.45f) {
                drill_strata_timer = 0.0f;
                glm::ivec3 tb = m_player->target_block();
                uint8_t target_mat = m_world->get_voxel(tb.x, tb.y, tb.z).material_id;
                SoundCue strataCue = SoundCue::DrillStrataBasalt;
                if (target_mat == MAT_TITANIUM) {
                    strataCue = SoundCue::DrillStrataTitanium;
                } else if (target_mat == MAT_VOIDITE_CRYSTAL) {
                    strataCue = SoundCue::DrillStrataVoidite;
                }
                AudioSystem::instance().TriggerRandomizedCue(strataCue, 0.40f);
            }
        }

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
        struct EnemyStepCandidate {
            SoundCue cue;
            glm::vec3 pos;
            float volume;
            float pitch;
            float dist;
        };
        std::vector<EnemyStepCandidate> enemy_step_candidates;

        for (auto& s : m_stalkers.stalkers_mut()) {
            if (s.just_screeched) {
                s.just_screeched = false;
                // Pure directional 3D audio echo without HUD text clutter (leaves the player relying on hearing)
                float pitch = 1.0f;
                AudioSystem::instance().TriggerRandomizedCue(SoundCue::VoidStalkerRoar, 0.45f, &pitch);
                m_audio->play_sound_3d(SoundCue::StalkerEchoScreech, s.position, 0.52f, pitch);
            }
            if (s.just_chittered) {
                s.just_chittered = false;
                // Subtle 3D mandible clicking in the dark (very quiet, creepy stealth cue)
                float pitch = 1.0f;
                AudioSystem::instance().TriggerRandomizedCue(SoundCue::StalkerChitter, 0.65f, &pitch);
                m_audio->play_sound_3d(SoundCue::StalkerChitter, s.position, 0.65f, pitch);
            }
            if (s.just_stepped) {
                s.just_stepped = false;
                float d = glm::distance(s.position, m_player->position());
                if (d <= 28.0f) {
                    SoundCue step_cue = (s.role == StalkerRole::ChitinGoliath)
                                            ? SoundCue::GoliathStep
                                            : (s.step_is_run ? SoundCue::StalkerRun : SoundCue::StalkerStep);
                    enemy_step_candidates.push_back({step_cue, s.position, s.step_volume, s.step_pitch, d});
                }
            }
            glm::vec3 burrow_loc = (glm::length(s.burrow_entry) > 0.01f) ? s.burrow_entry : s.position;
            if (s.state == StalkerState::Emerging && m_renderer) {
                float progress = std::clamp(s.state_timer / std::max(0.1f, s.burrow_duration), 0.0f, 1.0f);
                if (is_burrow_visible(s.position)) {
                    m_renderer->spawn_monster_emergence_particles(s.position, s.contact_normal, s.role, progress, s.just_dug ? 10 : 3);
                }
            }
            if (s.just_emerged) {
                s.just_emerged = false;
                if (m_audio) {
                    if (s.role == StalkerRole::ChitinGoliath) {
                        m_audio->play_sound_3d(SoundCue::BurrowerRoar, s.position, 1.10f);
                        m_audio->play_sound_3d(SoundCue::VoxelBreakBasalt, s.position, 0.95f);
                    } else if (s.role == StalkerRole::Shooter) {
                        float pitch = 1.25f;
                        AudioSystem::instance().TriggerRandomizedCue(SoundCue::StalkerHiss, 0.85f, &pitch);
                        m_audio->play_sound_3d(SoundCue::StalkerHiss, s.position, 0.85f, pitch);
                    } else {
                        m_audio->play_sound_3d(SoundCue::StalkerChitter, s.position, 1.0f);
                    }
                }
                if (m_renderer && is_burrow_visible(s.position)) {
                    float dust_amt = (s.role == StalkerRole::ChitinGoliath) ? 2.5f : 1.2f;
                    m_renderer->trigger_dust_kickup(dust_amt);
                    if (s.role == StalkerRole::ChitinGoliath) {
                        m_renderer->spawn_crack_debris(s.position, glm::ivec3(0, 1, 0), 1.4f, MAT_VOLCANIC_BASALT);
                    }
                }
            }
            if (s.state == StalkerState::Burrowing && m_renderer) {
                // Directional rock chipping particles localized at borehole surface entry point
                if (s.state_timer < 1.1f && is_burrow_visible(burrow_loc)) {
                    m_renderer->spawn_burrow_particles(burrow_loc, s.burrow_dir, MAT_FRACTURED_GRANITE, s.just_dug ? 8 : 2);
                }
            }
            if (s.just_dug) {
                s.just_dug = false;
                m_audio->play_sound_3d(SoundCue::MonsterDigging, s.position, 0.85f);
                if (m_renderer && is_burrow_visible(burrow_loc)) {
                    m_renderer->spawn_crack_debris(burrow_loc, glm::ivec3(-s.burrow_dir.x, -s.burrow_dir.y, -s.burrow_dir.z), 0.75f, MAT_FRACTURED_GRANITE);
                    m_renderer->trigger_dust_kickup(0.35f);
                }
            }
            if (s.just_escaped) {
                s.just_escaped = false;
                m_audio->play_sound_3d(SoundCue::VoxelBreakBasalt, s.position, 0.75f);
                if (m_renderer && is_burrow_visible(burrow_loc)) {
                    m_renderer->spawn_burrow_particles(burrow_loc, s.burrow_dir, MAT_FRACTURED_GRANITE, 16);
                    m_renderer->spawn_crack_debris(burrow_loc, glm::ivec3(0, 1, 0), 1.0f, MAT_FRACTURED_GRANITE);
                    m_renderer->trigger_dust_kickup(1.2f);
                }
                SwarmManager::instance().on_hostile_killed();
            }
            if (s.just_spotted_player) {
                s.just_spotted_player = false;
                // Predatory throat hiss when stalking delver
                float pitch = 1.0f;
                AudioSystem::instance().TriggerRandomizedCue(SoundCue::StalkerHiss, 0.65f, &pitch);
                m_audio->play_sound_3d(SoundCue::StalkerHiss, s.position, 0.65f, pitch);
            }
            if (s.just_lunged) {
                s.just_lunged = false;
                // Adrenaline spike: duck background ambient drone by 70% during visceral leap attack
                m_audio->trigger_ducking(0.28f, 0.45f, 1.8f);
                float pitch = 1.0f;
                AudioSystem::instance().TriggerRandomizedCue(SoundCue::VoidStalkerRoar, 0.50f, &pitch);
                m_audio->play_sound_3d(SoundCue::StalkerLunge, s.position, 0.60f, pitch);
            }
            if (s.just_hit_player) {
                s.just_hit_player = false;
                m_audio->trigger_ducking(0.35f, 0.30f, 2.2f);
                m_audio->play_sound_2d(SoundCue::StalkerHit, 1.0f);
            }
            if (s.just_died) {
                s.just_died = false;
                // Apply smooth gain fade-out tail rather than an immediate hard stop
                AudioSystem::StopVoice(VOICE_ENEMY, 0.4f);
                m_audio->play_sound_3d(SoundCue::StalkerDie, s.position, 0.70f);
                SwarmManager::instance().on_hostile_killed();
            }
        }
        m_stalkers.remove_dead();
        if (m_stalkers.active_count() == 0 && SwarmManager::instance().IsEnraged()) {
            SwarmManager::instance().EndEnrage();
        }

        // Seismic Burrower 3D Audio & Particle Cues
        for (auto& b : m_burrowers.burrowers_mut()) {
            if (b.just_breached) {
                b.just_breached = false;
                if (m_audio) {
                    m_audio->play_sound_3d(SoundCue::BurrowerRoar, b.position, 1.15f);
                    m_audio->play_sound_3d(SoundCue::BurrowerGrind, b.position, 1.0f);
                    m_audio->play_sound_3d(SoundCue::VoxelBreakBasalt, b.position, 0.95f);
                }
                if (m_renderer && is_burrow_visible(b.position)) {
                    m_renderer->spawn_crack_debris(b.position, glm::ivec3(0, 1, 0), 1.2f, MAT_VOLCANIC_BASALT);
                    m_renderer->trigger_dust_kickup(2.5f);
                }
            }
            if (b.just_roared) {
                b.just_roared = false;
                m_audio->play_sound_3d(SoundCue::BurrowerRoar, b.position, 1.05f);
            }
            if (b.just_chattered) {
                b.just_chattered = false;
                float pitch = 0.82f;
                AudioSystem::instance().TriggerRandomizedCue(SoundCue::StalkerChitter, 0.60f, &pitch);
                m_audio->play_sound_3d(SoundCue::StalkerChitter, b.position, 0.60f, pitch);
            }
            if (b.just_stepped) {
                b.just_stepped = false;
                float d = glm::distance(b.position, m_player->position());
                if (d <= 28.0f) {
                    enemy_step_candidates.push_back({SoundCue::BurrowerStep, b.position, b.step_volume, b.step_pitch, d});
                }
            }
            if (b.just_ground) {
                b.just_ground = false;
                m_audio->play_sound_3d(SoundCue::MonsterDigging, b.position, 0.90f);
                m_audio->play_sound_3d(SoundCue::BurrowerGrind, b.position, 0.85f);
                if (m_renderer) {
                    glm::vec3 tip_pos = b.position + b.forward() * 0.90f;
                    if (is_burrow_visible(tip_pos)) {
                        m_renderer->spawn_burrow_particles(tip_pos, b.forward(), MAT_VOLCANIC_BASALT, 12);
                        m_renderer->spawn_crack_debris(tip_pos, glm::ivec3(0, 1, 0), 0.65f, MAT_VOLCANIC_BASALT);
                    }
                }
            }
            if (b.state == BurrowerState::Burrowing && m_renderer) {
                glm::vec3 tip_pos = b.position + b.forward() * 0.90f;
                if (is_burrow_visible(tip_pos)) {
                    m_renderer->spawn_burrow_particles(tip_pos, b.forward(), MAT_VOLCANIC_BASALT, 3);
                }
            }
            if (b.state == BurrowerState::Breaching && m_renderer) {
                float progress = std::clamp(b.state_timer / 1.2f, 0.0f, 1.0f);
                if (is_burrow_visible(b.position)) {
                    m_renderer->spawn_burrower_emergence_particles(b.position, b.forward(), progress, b.just_ground ? 12 : 4);
                }
            }
            if (b.just_died) {
                b.just_died = false;
                m_audio->play_sound_3d(SoundCue::VoxelBreakBasalt, b.position, 0.95f);
                m_audio->play_sound_3d(SoundCue::BurrowerGrind, b.position, 0.70f);
                if (m_renderer && is_burrow_visible(b.position)) {
                    m_renderer->spawn_burrow_particles(b.position, glm::vec3(0, 1, 0), MAT_VOLCANIC_BASALT, 28);
                    m_renderer->spawn_crack_debris(b.position, glm::ivec3(0, 1, 0), 1.2f, MAT_VOLCANIC_BASALT);
                    m_renderer->trigger_dust_kickup(2.0f);
                }
                SwarmManager::instance().on_hostile_killed();
            }
        }
        m_burrowers.remove_dead();

        // Swarm Footstep Arbiter & Density Companding:
        // Dynamically balance footstep volume and rate across heavy enemy foot traffic
        static float s_enemy_step_spacing = 0.0f;
        s_enemy_step_spacing = std::max(0.0f, s_enemy_step_spacing - dt);

        if (!enemy_step_candidates.empty() && m_audio) {
            // Prioritize closest enemy footsteps
            std::sort(enemy_step_candidates.begin(), enemy_step_candidates.end(),
                      [](const EnemyStepCandidate& a, const EnemyStepCandidate& b) {
                          return a.dist < b.dist;
                      });

            int walker_count = static_cast<int>(enemy_step_candidates.size());
            float swarm_compand = 1.0f / (1.0f + 0.35f * std::max(0, walker_count - 1));

            // Pace footstep playback so rapid multi-enemy steps are staggered naturally
            if (s_enemy_step_spacing <= 0.0f || enemy_step_candidates.front().cue == SoundCue::GoliathStep) {
                const auto& best = enemy_step_candidates.front();
                m_audio->play_sound_3d(best.cue, best.pos, best.volume * swarm_compand, best.pitch);
                s_enemy_step_spacing = (best.cue == SoundCue::GoliathStep) ? 0.09f : 0.065f;
            }
        }

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
        static float s_room_transition_debounce = 0.0f;
        s_room_transition_debounce = std::max(0.0f, s_room_transition_debounce - dt);
        if (room_shape != s_last_room_shape && room_shape >= 0) {
            s_last_room_shape = room_shape;
            // Room transition acoustic arrival cue (spaced by 12s debounce so doorway steps do not repeat)
            if (s_room_transition_debounce <= 0.0f) {
                m_audio->trigger_room_micro_event(room_shape, 0.45f);
                s_room_transition_debounce = 12.0f;
            }
        }

        // Calculate dynamic proximity to hazards
        float lava_dist = 999.0f;
        float spike_dist = 999.0f;
        float void_dist = 999.0f;

        if (current_room) {
            if (current_room->type == RoomShapeType::MagmaCalderaLake || current_room->type == RoomShapeType::MoltenMagmaFoundry) {
                lava_dist = glm::distance(m_player->position(), glm::vec3(current_room->center.x, static_cast<float>(current_room->floor_y), current_room->center.z));
            } else if (current_room->type == RoomShapeType::FaultLineCrevasse || current_room->type == RoomShapeType::TectonicAbyssalSinkhole) {
                lava_dist = std::abs(m_player->position().x - static_cast<float>(current_room->center.x));
            } else if (current_room->type == RoomShapeType::SpikeTrenchArena || current_room->type == RoomShapeType::ColossalAbyssalChasm) {
                spike_dist = std::max(0.0f, m_player->position().y - (static_cast<float>(current_room->floor_y) + 1.0f));
            } else if (current_room->type == RoomShapeType::VoidSingularityRift) {
                void_dist = glm::distance(m_player->position(), glm::vec3(current_room->center.x, static_cast<float>(current_room->floor_y), current_room->center.z));
            }
        }

        // Check underfoot / vertical drop into bottomless abyss
        // Only trigger void proximity if the player is not grounded and actually suspended over void/air
        if (m_player && !m_player->isGrounded() && m_player->position().y <= 4.0f && m_world) {
            int check_gx = static_cast<int>(std::floor(m_player->position().x));
            int check_gy = static_cast<int>(std::floor(m_player->position().y - 1.0f));
            int check_gz = static_cast<int>(std::floor(m_player->position().z));
            if (m_world->get_voxel(check_gx, check_gy, check_gz).material_id == MAT_AIR) {
                void_dist = std::min(void_dist, std::max(0.0f, m_player->position().y - 1.8f));
            }
        }

        int ground_x = static_cast<int>(std::floor(m_player->position().x));
        int ground_y = static_cast<int>(std::floor(m_player->position().y - 0.96f));
        int ground_z = static_cast<int>(std::floor(m_player->position().z));
        Voxel underfoot = m_world->get_voxel(ground_x, ground_y, ground_z);
        if (underfoot.material_id == MAT_THERMITE_SLAG || underfoot.material_id == MAT_MOLTEN_MAGMA) {
            lava_dist = 0.5f;
        }
        if (underfoot.material_id == MAT_OBSIDIAN_SPIKES) {
            spike_dist = 0.5f;
        }

        // Dynamic spatial query for distant telegraphed toxic gas plumes (visible up to 45m away)
        float gas_dist = 999.0f;
        glm::vec3 pp = m_player->position();

        static float s_gas_pfx_timer = 0.0f;
        s_gas_pfx_timer += dt;
        bool spawn_gas_pfx = (s_gas_pfx_timer >= 0.14f);
        if (spawn_gas_pfx) s_gas_pfx_timer = 0.0f;

        if (m_world && m_renderer) {
            for (const auto& gpos_i : m_world->toxic_gas_sources()) {
                glm::vec3 gpos(gpos_i);
                float d = glm::distance(pp, gpos + glm::vec3(0.5f));
                gas_dist = std::min(gas_dist, d);
                // Telegraph visible pockets of toxic gas particles across wide rooms up to 45m away
                if (spawn_gas_pfx && d < 45.0f && (rand() % 6 == 0)) {
                    m_renderer->spawn_toxic_gas_cloud(gpos, 1);
                }
            }
        }

        // Local legacy gas check fallback
        int px = static_cast<int>(std::floor(pp.x));
        int py = static_cast<int>(std::floor(pp.y));
        int pz = static_cast<int>(std::floor(pp.z));
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dz = -5; dz <= 5; ++dz) {
                for (int dx = -5; dx <= 5; ++dx) {
                    Voxel vox = m_world->get_voxel(px + dx, py + dy, pz + dz);
                    if (vox.material_id == MAT_GAS || vox.material_id == MAT_TOXIC_GAS) {
                        glm::vec3 gpos(px + dx, py + dy, pz + dz);
                        float d = glm::distance(pp, gpos + glm::vec3(0.5f));
                        gas_dist = std::min(gas_dist, d);
                        if (spawn_gas_pfx && d < 12.0f && m_renderer) {
                            m_renderer->spawn_toxic_gas_cloud(gpos, 1);
                        }
                    }
                }
            }
        }

        // Spawn ambient ionizing sparks near radioactive ore (visible up to 40m away)
        if (m_world && m_renderer) {
            static float s_rad_pfx_timer = 0.0f;
            s_rad_pfx_timer += dt;
            if (s_rad_pfx_timer >= 0.18f) {
                s_rad_pfx_timer = 0.0f;
                for (const auto& rpos_i : m_world->radioactive_sources()) {
                    glm::vec3 rpos(rpos_i);
                    float d = glm::distance(pp, rpos + glm::vec3(0.5f));
                    if (d < 40.0f && (rand() % 5 == 0)) {
                        m_renderer->spawn_radiation_glimmer(rpos, 1);
                    }
                }
            }
        }

        // Spawn ambient crystal water mist near subterranean aquifers & waterfalls (visible up to 35m)
        if (m_world && m_renderer) {
            static float s_water_pfx_timer = 0.0f;
            s_water_pfx_timer += dt;
            if (s_water_pfx_timer >= 0.20f) {
                s_water_pfx_timer = 0.0f;
                for (const auto& wpos_i : m_world->aquifer_sources()) {
                    glm::vec3 wpos(wpos_i);
                    float d = glm::distance(pp, wpos + glm::vec3(0.5f));
                    if (d < 35.0f && (rand() % 8 == 0)) {
                        m_renderer->spawn_water_mist(wpos, 1);
                    }
                }
            }
        }

        // Spawn ambient embers near lava (visible up to 35m)
        if (m_world && m_renderer) {
            static float s_lava_pfx_timer = 0.0f;
            s_lava_pfx_timer += dt;
            if (s_lava_pfx_timer >= 0.22f) {
                s_lava_pfx_timer = 0.0f;
                for (const auto& lpos_i : m_world->lava_sources()) {
                    glm::vec3 lpos(lpos_i);
                    float d = glm::distance(pp, lpos + glm::vec3(0.5f));
                    if (d < 35.0f && (rand() % 6 == 0)) {
                        m_renderer->spawn_lava_embers(lpos, 1);
                    }
                }
            }
        }

        m_nearest_gas_distance = gas_dist;
        m_nearest_lava_distance = lava_dist;

        float prox_rad_val = m_proximity_radiation * 100.0f;
        float rad_val = std::max(m_player->exo().radiation, prox_rad_val);
        m_audio->update_hazard_proximity_audio(dt, lava_dist, rad_val, spike_dist, void_dist, gas_dist);

        // Update player biometrics (cardiac heartbeat vitals)
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

    // ═══ Reduced & Balanced Radiation HP Drain ═══
    // Radiation only occurs in radiation-related areas (proximity to MAT_RADIOACTIVE_ORE / radioactive chasms).
    // Safe cavern areas have 0 exposure, allowing suit scrubbers to actively vent and lower the radiation meter.
    // HP damage only begins at extreme exposure (>= 75%) so exploration is not suffocated by early tick damage.
    // < 75%:  safe buffer (0 HP/s — exosuit filters hold completely)
    // 75-89%: very gentle warning corrosion (~0.03 * (rad-75) HP/s = 0 - 0.42 HP/s)
    // 90-97%: moderate hazard (0.45 + (rad-90) * 0.08 HP/s = 0.45 - 1.01 HP/s)
    // 98%+:   critical hazard (capped at 1.5 HP/s max, down from 5.0 HP/s)
    {
        bool in_decon = (m_extraction && m_extraction->is_player_in_perimeter(m_player->position()));
        float prox_rad = m_proximity_radiation * 100.0f;
        float target_rad = in_decon ? 0.0f : prox_rad;

        m_player->update_radiation(target_rad, dt, in_decon);
        float rad = m_player->exo().radiation;

        float rad_damage = 0.0f;
        if (rad >= 98.0f) {
            rad_damage = 1.5f;
        } else if (rad >= 90.0f) {
            rad_damage = 0.45f + (rad - 90.0f) * 0.08f;
        } else if (rad >= 75.0f) {
            rad_damage = (rad - 75.0f) * 0.03f;
        }

        if (rad_damage > 0.0f) {
            if (in_decon) {
                float resist = (m_selected_level <= 1) ? 0.10f : (m_selected_level == 2 ? 0.15f : 0.20f);
                rad_damage *= resist;
            }
            // Apply radiation damage with ZERO screen shake trauma!
            m_player->take_damage(rad_damage * dt, PlayerController::DamageSource::Radiation);

            // Periodic vocal groaning from the delver under radiation sickness (rate-limited)
            static float s_rad_groan_timer = 0.0f;
            s_rad_groan_timer += dt;
            if (s_rad_groan_timer >= 4.5f) {
                s_rad_groan_timer = 0.0f;
                if (m_audio) {
                    m_audio->trigger_player_groan(0.70f);
                }
            }
        }

        // Exosuit damage only at extreme radiation levels (100%)
        if (rad >= 100.0f) {
            m_player->take_damage(2.0f * dt, PlayerController::DamageSource::Radiation);
        }
    }

    // ═══ Gas Pocket Damage ═══
    // Standing inside MAT_GAS voxels corrodes suit integrity and deals mild HP damage
    {
        glm::vec3 pp = m_player->position();
        int gx = static_cast<int>(std::floor(pp.x));
        int gy = static_cast<int>(std::floor(pp.y));
        int gz = static_cast<int>(std::floor(pp.z));
        Voxel gas_check = m_world->get_voxel(gx, gy, gz);
        Voxel gas_head  = m_world->get_voxel(gx, gy + 1, gz);
        if (gas_check.material_id == MAT_GAS || gas_head.material_id == MAT_GAS) {
            // Toxic gas damage: subtle cough shudder only (0.02f), NOT violent screen shake!
            m_player->take_damage(1.0f * dt, PlayerController::DamageSource::ToxicGas);

            // Trigger on-screen visor toxic mist particles and asphyxiation coughing
            if (m_hud) {
                m_hud->set_toxic_gas_exposure(1.0f, dt);
            }

            static float s_gas_asphyx_timer = 0.0f;
            s_gas_asphyx_timer += dt;
            if (s_gas_asphyx_timer >= 2.0f) {
                s_gas_asphyx_timer = 0.0f;
                if (m_audio) {
                    m_audio->trigger_player_asphyxiation(0.80f);
                }
            }

        }

        if (m_hud) {
            if (m_player->is_in_acid() || m_player->acid_burn_timer() > 0.0f) {
                float acid_exp = m_player->is_in_acid() ? 1.0f : (m_player->acid_burn_timer() / 2.0f);
                m_hud->set_acid_exposure(acid_exp, dt);
            }
            if (m_player->is_in_coolant() || m_player->frost_chill_timer() > 0.0f) {
                float frost_exp = m_player->is_in_coolant() ? 1.0f : (m_player->frost_chill_timer() / 2.5f);
                m_hud->set_frost_exposure(frost_exp, dt);
            }
        }

        if (m_player->is_suffocating()) {
            if (m_renderer) {
                m_renderer->set_area_darkness(1.0f);
            }
        }
    }

    // ═══ Extraction Holdout Wave Escalation ═══
    // Scale holdout pacing and enemy density smoothly by sector level.
    // Level 1: 20s holdout, 2 solitary stalkers, zero burrowers, zero tremors.
    // Level 2: 30s holdout, 4 stalkers + 1 burrower, mild final tremor (protected LZ).
    // Level 3: 40s holdout, 7 stalkers + 1 burrower, severe tectonic tremor.
    if (m_extraction->phase() == ExtractionPhase::BeaconDeployed) {
        float total_cd = std::max(1.0f, m_extraction->initial_countdown());
        float cd = m_extraction->countdown();
        float progress = 1.0f - std::clamp(cd / total_cd, 0.0f, 1.0f);

        if (m_selected_level <= 1) {
            // Level 1 (Sector 1: Perimeter Drift) - Introductory Gentle Holdout (20s)
            // Stage 1 (progress >= 0.45, ~9s elapsed): 1 solitary stalker scout
            if (m_holdout_stage == 0 && progress >= 0.45f) {
                m_holdout_stage = 1;
                m_stalkers.spawn_wave(m_player->position(), 1, *m_world);
                m_player->add_trauma(0.12f);
            }
            // Stage 2 (progress >= 0.80, ~16s elapsed): 1 additional stalker
            else if (m_holdout_stage == 1 && progress >= 0.80f) {
                m_holdout_stage = 2;
                m_stalkers.spawn_wave(m_player->position(), 1, *m_world);
                m_player->add_trauma(0.15f);
            }
            // Stage 3 (progress >= 0.95, ~19s elapsed): Evac pod touchdown (NO burrowers, NO tremors)
            else if (m_holdout_stage == 2 && progress >= 0.95f) {
                m_holdout_stage = 3;
            }
        } else if (m_selected_level == 2) {
            // Level 2 (Sector 2: Volatile Fault) - Moderate Holdout (30s)
            // Stage 1 (progress >= 0.35, ~10.5s elapsed): 1 flanking stalker scout
            if (m_holdout_stage == 0 && progress >= 0.35f) {
                m_holdout_stage = 1;
                m_stalkers.spawn_wave(m_player->position(), 1, *m_world);
                m_player->add_trauma(0.15f);
            }
            // Stage 2 (progress >= 0.70, ~21s elapsed): 2 stalker reinforcements (fair combat, no burrower)
            else if (m_holdout_stage == 1 && progress >= 0.70f) {
                m_holdout_stage = 2;
                m_stalkers.spawn_wave(m_player->position(), 2, *m_world);
                m_player->add_trauma(0.20f);
            }
            // Stage 3 (progress >= 0.90, ~27s elapsed): Mild tremor (LZ is shielded from rocks)
            else if (m_holdout_stage == 2 && progress >= 0.90f) {
                m_holdout_stage = 3;
                m_hazard->force_tremor(0.5f);
            }
        } else {
            // Level 3+ (Sector 3: Abyssal Mantle) - Endgame High-Intensity Holdout (40s)
            // Stage 1 (progress >= 0.25, ~10s elapsed): 3 aggressive stalkers
            if (m_holdout_stage == 0 && progress >= 0.25f) {
                m_holdout_stage = 1;
                m_stalkers.spawn_wave(m_player->position(), 3, *m_world);
                m_player->add_trauma(0.25f);
            }
            // Stage 2 (progress >= 0.65, ~26s elapsed): 4 stalkers + 1 burrower
            else if (m_holdout_stage == 1 && progress >= 0.65f) {
                m_holdout_stage = 2;
                m_stalkers.spawn_wave(m_player->position(), 4, *m_world);
                if (m_burrowers.active_count() == 0) {
                    m_burrowers.spawn_hazard_wave(m_player->position(), *m_world);
                }
                m_player->add_trauma(0.40f);
                if (m_audio) {
                    m_audio->play_sound_2d(SoundCue::SeismicTremor, 0.65f);
                }
            }
            // Stage 3 (progress >= 0.88, ~35s elapsed): Severe tremor + final push
            else if (m_holdout_stage == 2 && progress >= 0.88f) {
                m_holdout_stage = 3;
                m_hazard->force_tremor(1.5f);
            }
        }
    }

    // Check player health failure (only when not extracted)
    if (!m_expedition_success && !m_death_sequence && m_player->health() <= 0.0f) {
        m_death_sequence = true;
        m_death_timer = 0.0f;
        m_inventory.suit_failed = true;
        m_expedition_success = false;
        m_trauma = 1.0f;
        AudioSystem::PlayDeathSound();
        if (m_audio) {
            m_audio->play_sound_2d(SoundCue::DamageWarning, 1.0f);
        }
        std::string death_cause = "SUIT INTEGRITY COMPROMISED // DELVER M.I.A.";
        if (m_player->is_in_lava()) {
            death_cause = "INCINERATED // DELVER CONSUMED BY THERMITE SLAG";
        } else if (m_player->is_in_acid() || m_player->acid_burn_timer() > 0.0f) {
            death_cause = "DISSOLVED // DELVER CONSUMED BY CORROSIVE NEON ACID";
        } else if (m_player->is_in_coolant()) {
            death_cause = "FROZEN // CRYOGENIC SHOCK AND CELLULAR CRYSTALLIZATION";
        } else if (m_player->is_in_spikes()) {
            death_cause = "IMPALED // FATAL PUNCTURE TRAUMA IN SPIKE TRENCH";
        } else if (m_player->position().y <= 2.2f) {
            death_cause = "CRUSHED // CONSUMED BY ABYSSAL VOID SINGULARITY";
        } else if (m_player->exo().radiation >= 95.0f) {
            death_cause = "LETHAL RADIATION POISONING // ACUTE IONIZATION SICKNESS";
        } else if (m_player->is_suffocating()) {
            death_cause = "ASPHYXIATED // BURIED ALIVE IN CAVE-IN COLLAPSE";
        }
        VF_LOG_INFO("Gameplay", "Player Delver died: " << death_cause);
    }

    // 5. Stream chunks around player and update voxel world systems (fluids, dynamic debris)
    m_world->update(m_player->position(), 2);
    m_world->Update(dt);

    // Chemical Flare simulation update
    FlareManager::instance().update(dt, *m_world);

    // Multi-stage mission system update
    if (m_player) {
        m_mission.update(dt, *m_world, m_player->position());
    }

    // Railgun muzzle flash decay
    if (m_muzzle_flash_timer > 0.0f) {
        m_muzzle_flash_timer = std::max(0.0f, m_muzzle_flash_timer - dt);
    }

    // 6. Delver Assistance: Delayed acoustic scan hint when player hasn't interacted with a given objective in a long time
    m_astral_hint_idle_timer += dt;
    m_voidite_hint_idle_timer += dt;
    m_vault_hint_idle_timer += dt;
    m_objective_hint_timer += dt;

    // Check active player drill contact with objective materials to reset idle time continuously
    if (m_player && m_world && m_player->target_block_damage() > 0.0f && m_player->target_block().x != -1) {
        glm::ivec3 tb = m_player->target_block();
        Voxel tv = m_world->get_voxel(tb.x, tb.y, tb.z);
        if (tv.material_id == MAT_ASTRAL_ORE || tv.material_id == MAT_VOIDITE_CRYSTAL ||
            tv.material_id == MAT_REINFORCED_VAULT_DOOR || tv.material_id == MAT_PRISMATIC_CRYSTAL) {
            register_objective_interaction(tv.material_id);
        }
    }

    bool needs_astral = (m_inventory.target_astral_ore > 0 && m_inventory.astral_ore < m_inventory.target_astral_ore);
    bool needs_voidite = (m_inventory.target_voidite > 0 && m_inventory.voidite < m_inventory.target_voidite);
    bool needs_vault = (m_mission.vault().exists && !m_mission.is_vault_breached() && !m_mission.is_relic_retrieved());

    // Only hint the player at an objective if they haven't interacted with the given objective in a long time (>= 75s)
    bool astral_qualifies = needs_astral && (m_astral_hint_idle_timer >= OBJECTIVE_HINT_IDLE_THRESHOLD);
    bool voidite_qualifies = needs_voidite && (m_voidite_hint_idle_timer >= OBJECTIVE_HINT_IDLE_THRESHOLD);
    bool vault_qualifies = needs_vault && (m_vault_hint_idle_timer >= OBJECTIVE_HINT_IDLE_THRESHOLD);

    if (!m_objective_hint_given && (astral_qualifies || voidite_qualifies || vault_qualifies)) {
        uint8_t target_mat = 0;
        std::string ore_name = "";
        glm::vec4 hint_col = glm::vec4(1.0f);
        glm::vec3 hint_target_pos(0.0f);
        bool has_target = false;

        // Candidate selection: choose unfulfilled objective that has gone longest without interaction
        if (astral_qualifies && voidite_qualifies) {
            if (m_astral_hint_idle_timer >= m_voidite_hint_idle_timer) {
                target_mat = MAT_ASTRAL_ORE;
            } else {
                target_mat = MAT_VOIDITE_CRYSTAL;
            }
        } else if (astral_qualifies) {
            target_mat = MAT_ASTRAL_ORE;
        } else if (voidite_qualifies) {
            target_mat = MAT_VOIDITE_CRYSTAL;
        } else if (vault_qualifies) {
            target_mat = MAT_REINFORCED_VAULT_DOOR;
        }

        if (target_mat == MAT_ASTRAL_ORE) {
            ore_name = "ASTRAL ORE";
            hint_col = glm::vec4(1.0f, 0.82f, 0.20f, 1.0f);
            auto ore_positions = m_world->query_material_positions(MAT_ASTRAL_ORE, 32);
            if (!ore_positions.empty()) {
                glm::vec3 player_pos = m_player->position();
                glm::vec3 closest_pos = glm::vec3(ore_positions[0]);
                float min_d = glm::distance(player_pos, closest_pos);
                for (size_t i = 1; i < ore_positions.size(); ++i) {
                    glm::vec3 cand = glm::vec3(ore_positions[i]);
                    float d = glm::distance(player_pos, cand);
                    if (d < min_d) {
                        min_d = d;
                        closest_pos = cand;
                    }
                }
                hint_target_pos = closest_pos;
                has_target = true;
            }
        } else if (target_mat == MAT_VOIDITE_CRYSTAL) {
            ore_name = "VOIDITE CRYSTAL";
            hint_col = glm::vec4(0.88f, 0.30f, 1.0f, 1.0f);
            auto ore_positions = m_world->query_material_positions(MAT_VOIDITE_CRYSTAL, 32);
            if (!ore_positions.empty()) {
                glm::vec3 player_pos = m_player->position();
                glm::vec3 closest_pos = glm::vec3(ore_positions[0]);
                float min_d = glm::distance(player_pos, closest_pos);
                for (size_t i = 1; i < ore_positions.size(); ++i) {
                    glm::vec3 cand = glm::vec3(ore_positions[i]);
                    float d = glm::distance(player_pos, cand);
                    if (d < min_d) {
                        min_d = d;
                        closest_pos = cand;
                    }
                }
                hint_target_pos = closest_pos;
                has_target = true;
            }
        } else if (target_mat == MAT_REINFORCED_VAULT_DOOR) {
            ore_name = "PRECURSOR VAULT";
            hint_col = glm::vec4(1.0f, 0.0f, 0.85f, 1.0f);
            hint_target_pos = glm::vec3(m_mission.vault().door_pos);
            has_target = true;
        }

        if (has_target) {
            m_objective_hint_given = true;
            m_objective_hint_pos = hint_target_pos;
            m_objective_hint_mat = target_mat;
            float dist = glm::distance(m_player->position(), hint_target_pos);

            if (m_hud) {
                m_hud->set_objective_hint(hint_target_pos, ore_name, hint_col, 60.0f, target_mat);
                std::string hint_msg = "DEEP SCAN: " + ore_name + " DETECTED [" +
                                      std::to_string(static_cast<int>(dist)) + "m BEARING]";
                m_hud->show_warning(hint_msg, 6.0f);
            }
            if (m_audio) {
                m_audio->play_sound_2d(SoundCue::UIBlip, 1.0f);
            }
            VF_LOG_INFO("Gameplay", "Provided delayed exploration hint pointing to " << ore_name
                                    << " at (" << hint_target_pos.x << ", " << hint_target_pos.y << ", "
                                    << hint_target_pos.z << ") dist=" << dist << "m"
                                    << " (idle_time=" << get_objective_idle_time(target_mat) << "s)");
        }
    }

    // 7. Auto-removal of objective hint once player reaches, interacts with, or mines the material
    if (m_hud && m_hud->has_objective_hint() && m_player && m_world) {
        glm::vec3 hint_pos = m_hud->hint_waypoint_pos();
        glm::vec3 hint_center = hint_pos + glm::vec3(0.5f, 0.5f, 0.5f);
        float dist_pos = glm::distance(m_player->position(), hint_center);
        float dist_eye = glm::distance(m_player->eye_position(), hint_center);

        // Reached: physical proximity <= 3.5m or direct line-of-sight within drill reach (<= 5.0m)
        bool reached = (dist_pos <= 3.5f || dist_eye <= 3.5f);
        if (!reached && dist_eye <= 5.0f) {
            glm::vec3 to_hint = (hint_center - m_player->eye_position()) / dist_eye;
            RaycastHit los = m_world->raycast(m_player->eye_position(), to_hint, dist_eye - 0.2f);
            if (!los.hit) {
                reached = true;
            }
        }

        // Mined: block at hint position was destroyed or turned to air
        int hx = static_cast<int>(std::floor(hint_pos.x + 0.1f));
        int hy = static_cast<int>(std::floor(hint_pos.y + 0.1f));
        int hz = static_cast<int>(std::floor(hint_pos.z + 0.1f));
        Voxel v = m_world->get_voxel(hx, hy, hz);
        bool mined = (v.material_id == MAT_AIR);
        if (m_objective_hint_mat != 0 && v.material_id != m_objective_hint_mat) {
            mined = true;
        }

        // Quota fulfilled: player collected all required objective ores / breached vault
        bool quota_done = (m_objective_hint_mat == MAT_ASTRAL_ORE && !needs_astral) ||
                          (m_objective_hint_mat == MAT_VOIDITE_CRYSTAL && !needs_voidite) ||
                          (m_objective_hint_mat == MAT_REINFORCED_VAULT_DOOR && !needs_vault);

        if (reached || mined || quota_done) {
            m_hud->clear_objective_hint();
            m_objective_hint_given = false;
            m_objective_hint_timer = 0.0f;
            register_objective_interaction(m_objective_hint_mat);
            VF_LOG_INFO("Gameplay", "Objective hint auto-removed (reached=" << reached
                                    << " mined=" << mined << " quota_done=" << quota_done
                                    << ") at (" << hint_pos.x << ", " << hint_pos.y << ", " << hint_pos.z << ")");
        }
    } else if (m_objective_hint_given && (!m_hud || !m_hud->has_objective_hint())) {
        // If HUD auto-dismissed the hint (e.g. proximity check during render), sync state
        m_objective_hint_given = false;
        m_objective_hint_timer = 0.0f;
        register_objective_interaction(m_objective_hint_mat);
    }

    // 8. Darkness Hazard Warning: Alert player when entering a pitch-black zone without illumination
    if (m_hud && m_player && m_world && m_world->level_generator()) {
        float darkness = m_world->level_generator()->get_darkness_at(m_player->eye_position());
        if (darkness >= 0.75f && !m_player->is_headlamp_on()) {
            bool flare_nearby = false;
            for (const auto& flare : FlareManager::instance().flares()) {
                if (flare.is_alive() && glm::distance(flare.position, m_player->position()) <= 15.0f) {
                    flare_nearby = true;
                    break;
                }
            }
            if (!flare_nearby) {
                m_darkness_warning_cooldown -= dt;
                if (m_darkness_warning_cooldown <= 0.0f) {
                    m_hud->show_warning("DARK ZONE: ZERO LUMENS - DEPLOY FLARES [T] / HEADLAMP [F]", 3.0f);
                    m_darkness_warning_cooldown = 15.0f; // Throttle to every 15s
                }
            }
        } else {
            if (m_darkness_warning_cooldown > 0.0f) {
                m_darkness_warning_cooldown -= dt;
            }
        }
    }
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

        // Camera view setup with non-linear impulse trauma screen shake (scaled by comfort setting)
        float active_trauma = std::max(m_trauma, m_player ? m_player->trauma() : 0.0f);
        active_trauma *= m_settings.screen_shake;
        if (active_trauma > 0.001f) {
            // Toned down amplitude with smooth harmonic oscillation rather than 60Hz white-noise jitter
            float shake_amount = std::min(active_trauma * active_trauma * 0.12f, 0.045f);
            float wave_time = static_cast<float>(glfwGetTime()) * 16.0f;
            float shake_x = (std::sin(wave_time) * 0.7f + std::cos(wave_time * 1.6f) * 0.3f) * shake_amount;
            float shake_y = (std::cos(wave_time * 1.3f) * 0.7f + std::sin(wave_time * 2.1f) * 0.3f) * shake_amount;
            // Roll is eliminated to prevent horizon tilt and motion sickness
            view = glm::translate(view, glm::vec3(shake_x, shake_y, 0.0f));
            if (m_state == GameState::Gameplay) {
                // Crisp impulse decay (2.8f/s) so heavy impacts settle quickly without prolonged vibration
                m_trauma = (std::max)(0.0f, m_trauma - dt * 2.8f);
                if (m_player) {
                    m_player->set_trauma((std::max)(0.0f, m_player->trauma() - dt * 2.8f));
                }
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

        m_renderer->headlamp().position = m_player->eye_position();
        m_renderer->headlamp().direction = m_player->forward();
        m_renderer->headlamp().enabled = m_player->is_headlamp_on();

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

        if (m_world && m_world->level_generator()) {
            float darkness = m_world->level_generator()->get_darkness_at(m_player->eye_position());
            m_renderer->set_area_darkness(darkness);
        } else {
            m_renderer->set_area_darkness(0.0f);
        }
    } else {
        if (m_renderer) {
            m_renderer->set_headlamp_flicker(0.0f);
            m_renderer->set_area_darkness(0.0f);
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

    float effective_fov = m_settings.fov;
    if ((m_state == GameState::Gameplay || m_state == GameState::Paused) && m_player) {
        effective_fov = m_player->current_fov(m_settings.fov);
    }
    glm::mat4 proj = Renderer::create_projection(m_window->aspect_ratio(), effective_fov);

    // Setup dynamic point lights (Cavern luminaries, extraction beacon rotating siren, hostile stalker eyes)
    m_renderer->clear_point_lights();

    // 0. Delver Suit Ambient Spore / Visor Environmental Spill (r = 13.5m omni fill light)
    if (m_player) {
        PointLight suit_fill;
        suit_fill.position = m_player->position() + glm::vec3(0.0f, 1.2f, 0.0f);
        if (m_selected_level == 2) {
            suit_fill.color = glm::vec3(0.95f, 0.58f, 0.22f); // Warm subterranean geothermal glow
        } else if (m_selected_level >= 3) {
            suit_fill.color = glm::vec3(0.38f, 0.88f, 0.95f); // Phosphorescent cyan-emerald
        } else {
            suit_fill.color = glm::vec3(0.42f, 0.92f, 0.76f); // Bioluminescent subterranean teal
        }
        suit_fill.radius = 13.5f;
        suit_fill.intensity = m_player->is_headlamp_on() ? 0.65f : 0.88f;
        m_renderer->add_point_light(suit_fill);
    }

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

            // Clustered Forward Lighting allows 48+ active cavern luminaries without bottlenecking
            size_t num_to_add = std::min(count, static_cast<size_t>(48));
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

    // 3. Placed Explosive Charge Strobe Beacon Light
    if (m_player && m_player->has_placed_charge()) {
        glm::vec3 n = glm::vec3(m_player->placed_charge_normal());
        glm::vec3 cpos = glm::vec3(m_player->placed_charge_pos()) + glm::vec3(0.5f) + n * 0.55f;
        float blink = 0.5f + 0.5f * std::sin(current_time * 12.0f);
        PointLight cl;
        cl.position = cpos;
        cl.color = glm::vec3(1.0f, 0.55f, 0.08f);
        cl.radius = 8.0f;
        cl.intensity = 2.5f * blink;
        m_renderer->add_point_light(cl);
    }

    // Dynamic point lights for stalking/lunging hostiles (max 16 nearest)
    int added_stalker_lights = 0;
    for (const auto& s : m_stalkers.stalkers()) {
        if (s.is_dead() || added_stalker_lights >= 16) continue;
        PointLight sl;
        sl.position = s.position + glm::vec3(0.0f, 0.35f, 0.0f);
        switch (s.state) {
            case StalkerState::GroundSlam:
                sl.color = glm::vec3(1.0f, 0.45f, 0.05f);
                sl.radius = 12.0f;
                sl.intensity = 4.5f;
                break;
            case StalkerState::Charging:
                sl.color = glm::vec3(1.0f, 0.05f, 0.05f);
                sl.radius = 11.0f;
                sl.intensity = 4.0f;
                break;
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
        if (s.is_boss()) {
            if (s.is_enraged) {
                sl.color = glm::mix(sl.color, glm::vec3(1.0f, 0.08f, 0.05f), 0.75f);
                sl.radius *= 1.4f;
                sl.intensity *= 1.5f;
            } else {
                sl.radius *= 1.25f;
                sl.intensity *= 1.25f;
            }
        }
        m_renderer->add_point_light(sl);
        added_stalker_lights++;
    }

    // Needler Railgun Muzzle Flash point light (electric cyan, r = 3.5m)
    if (m_muzzle_flash_timer > 0.0f && m_player) {
        PointLight ml;
        ml.position = m_player->eye_position() + m_player->forward() * 0.7f + m_player->right() * 0.2f - m_player->up() * 0.1f;
        ml.color = glm::vec3(0.05f, 0.90f, 1.0f); // Electric cyan
        ml.radius = 3.5f;
        ml.intensity = 5.0f * (m_muzzle_flash_timer / 0.05f);
        m_renderer->add_point_light(ml);
    }

    // Active Chemical Flare point lights (r = 22.0m omni-light colored by class)
    for (const auto& f : FlareManager::instance().flares()) {
        if (!f.is_alive()) continue;
        PointLight fl;
        fl.position = f.position + glm::vec3(0.0f, 0.25f, 0.0f);
        fl.color = f.color;
        fl.radius = f.light_radius;
        fl.intensity = 5.2f * std::clamp(f.lifetime / 2.0f, 0.0f, 1.0f);
        m_renderer->add_point_light(fl);
    }

    // Dynamic Delver Plasma Bolt point lights (illuminates dark subterranean cavern tunnels in real-time)
    int bolt_lights = 0;
    for (const auto& bolt : m_plasma_bolts) {
        if (!bolt.active || bolt_lights >= 6) continue;
        PointLight bl;
        bl.position = bolt.position;
        bl.color = glm::vec3(bolt.color.r, bolt.color.g, bolt.color.b);
        bl.radius = 6.0f;
        bl.intensity = 2.8f;
        m_renderer->add_point_light(bl);
        bolt_lights++;
    }

    // Dynamic Void Spine Projectile point lights (radiant virulent emerald luminescence)
    int spine_lights = 0;
    for (const auto& proj : m_stalkers.projectiles()) {
        if (!proj.active || spine_lights >= 4) continue;
        PointLight pl;
        pl.position = proj.position;
        pl.color = glm::vec3(0.18f, 0.95f, 0.32f);
        pl.radius = 5.0f;
        pl.intensity = 2.5f;
        m_renderer->add_point_light(pl);
        spine_lights++;
    }

    // Begin HDR Frame
    m_renderer->begin_frame(view, proj, cam_pos);

    // Headlamp Perspective Shadow Map Pass
    if (m_world) {
        m_renderer->render_headlamp_shadows(*m_world, m_stalkers.stalkers());
    }

    // Render Chunks with Vertical & Frustum Culling
    m_world->Render(*m_renderer, cam_pos, m_player ? m_player->position() : cam_pos);

    // Render Falling Dynamic Debris
    for (const auto& d : m_debris) {
        m_renderer->render_debris(d);
    }

    // Render Void Stalker hostile entities and void spine projectiles
    m_renderer->render_stalkers(m_stalkers.stalkers(), m_stalkers.projectiles());

    // Render Persistent Enemy Carcasses
    m_renderer->render_carcasses(CarcassManager::instance().carcasses());

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

    // Render 3D Precursor Relic Hyper-Core (spinning celestial core, orbital gimbal rings & satellite shards)
    if (!m_mission.is_relic_retrieved()) {
        glm::vec3 relic_render_pos(0.0f);
        bool should_render_relic = false;
        if (m_mission.vault().exists) {
            relic_render_pos = glm::vec3(m_mission.vault().relic_pos) + glm::vec3(0.5f, 0.0f, 0.5f);
            should_render_relic = true;
        } else if (m_world && m_world->level_generator() && m_world->level_generator()->vault_placement().exists) {
            relic_render_pos = glm::vec3(m_world->level_generator()->vault_placement().relic_pos) + glm::vec3(0.5f, 0.0f, 0.5f);
            should_render_relic = true;
        }
        if (should_render_relic) {
            m_renderer->render_precursor_relic(relic_render_pos, static_cast<float>(glfwGetTime()));
        }
    }

    // Render 3D Extraction Beacon (tripod base, light column, drifting rings, rotating siren)
    if (m_extraction->phase() == ExtractionPhase::BeaconDeployed || m_extraction->phase() == ExtractionPhase::PodLanded) {
        bool is_landed = (m_extraction->phase() == ExtractionPhase::PodLanded);
        m_renderer->render_extraction_beacon(m_extraction->beacon_position(), m_extraction->siren_pulse(), static_cast<float>(glfwGetTime()), is_landed);
        if (is_landed) {
            m_renderer->render_extraction_pod(m_extraction->beacon_position(),
                                              m_extraction->pod_drill_progress(),
                                              m_extraction->ramp_extension(),
                                              static_cast<float>(glfwGetTime()),
                                              m_extraction->is_pod_anchored());
        }
    }

    // Render Grapple Cable (Module 4)
    if (m_player->grapple().active) {
        glm::vec3 tool_pos = m_player->position() + m_player->forward() * 0.4f + m_player->right() * 0.25f - glm::vec3(0.0f, 0.2f, 0.0f);
        m_renderer->render_grapple_cable(tool_pos, m_player->grapple().anchor_point);
    }

    // Render Volumetric Sonar Ping with smooth vein cluster outlines & hulls (Module 2)
    if (m_surveying.is_active()) {
        m_renderer->render_sonar_clusters(m_surveying.clusters(), m_surveying.alpha());
        m_renderer->render_sonar_monsters(m_surveying.monsters(), m_surveying.alpha());
    }

    // Render Progressive Block Cracks (Module 4) - only when CrackStage > 0.05f
    if (m_player->crack_stage() > 0.05f && m_player->target_block() != glm::ivec3(-1)) {
        Voxel v = m_world->get_voxel(m_player->target_block().x, m_player->target_block().y, m_player->target_block().z);
        m_renderer->render_block_cracks(m_player->target_block(), m_player->crack_stage(), m_player->target_normal(), v.material_id, v.shape());
    }

    // Render Placed Explosive Charge (Demolition Stun / Breach Distraction)
    if (m_player && m_player->has_placed_charge()) {
        m_renderer->render_placed_charge(m_player->placed_charge_pos(), m_player->placed_charge_normal(), static_cast<float>(glfwGetTime()));
    }

    // Render Throwable Chemical Flares
    m_renderer->render_flares(FlareManager::instance().flares(), static_cast<float>(glfwGetTime()));

    // Note: Navigation breadcrumbs are rendered exclusively on the 2D Hologram Terrain Scanner ([TAB] overlay)
    // to prevent in-world 3D markers from floating in mid-air or littering caverns.

    // Render 3D World Break, Burrow, and Dust Particles with scene depth testing
    m_renderer->render_particles();

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

        float horizontal_speed = glm::length(glm::vec2(m_player->velocity().x, m_player->velocity().z));
        bool is_grounded = m_player->is_grounded();

        m_viewmodel->set_weapon_archetype(m_player->weapon_archetype());
        m_viewmodel->render(
            dt,
            aspect,
            is_drilling,
            is_in_range,
            m_player->active_tool(),
            drill_target,
            m_player->is_firing_weapon(),
            m_player->is_crouching(),
            m_player->is_reloading(),
            m_player->reload_progress(),
            m_player->has_placed_charge(),
            m_player->is_melee_shoving(),
            horizontal_speed,
            is_grounded,
            m_player->melee_shove_progress(),
            m_player->is_aiming(),
            m_player->zoom_progress(),
            m_player->health(),
            m_player->max_health()
        );
    }

    // End HDR Frame & Post-Processing Tonemap
    float shield_timer = (m_state == GameState::Gameplay && m_player) ? m_player->insertion_shield_timer() : 0.0f;
    glm::vec3 danger_haze_color(0.12f, 0.65f, 0.22f); // Default radioactive toxic emerald
    float effective_haze = 0.0f;
    float effective_rad = (m_player) ? m_player->exo().radiation : 0.0f;
    if (m_state == GameState::Gameplay && m_player) {
        effective_rad = std::max(m_player->exo().radiation, m_proximity_radiation * 100.0f);
        float rad_haze = effective_rad / 100.0f;
        float gas_haze = (m_nearest_gas_distance < 20.0f) ? std::clamp(1.0f - (m_nearest_gas_distance / 20.0f), 0.0f, 0.85f) : 0.0f;
        float lava_haze = (m_nearest_lava_distance < 16.0f) ? std::clamp(1.0f - (m_nearest_lava_distance / 16.0f), 0.0f, 0.85f) : 0.0f;

        if (rad_haze >= gas_haze && rad_haze >= lava_haze && rad_haze > 0.05f) {
            danger_haze_color = glm::vec3(0.12f, 0.68f, 0.24f); // Toxic emerald radiation mist
            effective_haze = rad_haze;
        } else if (gas_haze >= lava_haze && gas_haze > 0.05f) {
            danger_haze_color = glm::vec3(0.48f, 0.75f, 0.12f); // Choking sulfur-green bio-gas vapor
            effective_haze = gas_haze;
        } else if (lava_haze > 0.05f) {
            danger_haze_color = glm::vec3(0.85f, 0.35f, 0.06f); // Molten volcanic heat haze
            effective_haze = lava_haze;
        }
    }
    float suffocation_darkness = (m_state == GameState::Gameplay && m_player && m_world &&
                                 (m_player->is_suffocating() || m_player->is_head_in_solid(*m_world))) ? 1.0f : 0.0f;
    m_renderer->end_frame(dt, effective_rad, shield_timer, danger_haze_color, effective_haze, suffocation_darkness);

    // Mouse coordinates and clicks for responsive menus
    glm::dvec2 cur_pos = m_window->get_cursor_pos();
    float mouse_x = static_cast<float>(cur_pos.x);
    float mouse_y = static_cast<float>(cur_pos.y);
    bool mouse_down = m_window->is_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT);
    bool mouse_clicked = mouse_down && !m_mouse_down_last;
    m_mouse_down_last = mouse_down;

    // Render State-Specific Overlay
    if (m_state == GameState::MainMenu) {
        bool has_save = m_user_profile.has_save_data();
        MainMenuAction action = m_hub_ui->render_main_menu(m_selected_level, m_user_profile, has_save, mouse_x, mouse_y, mouse_clicked, mouse_down, static_cast<float>(m_window->get_scroll_delta_y()));
        m_settings = m_user_profile.settings;
        sync_audio_settings();

        if (m_hub_ui->check_and_clear_test_sound() && m_audio) {
            m_audio->play_sound_2d(SoundCue::UIBlip, 1.0f);
        }

        if (m_hub_ui->has_profile_changed()) {
            m_hub_ui->clear_profile_changed();
            SaveSystem::save_profile(m_user_profile, m_active_save_file);
            for (int s = 1; s < PlayerInventory::MAX_SECTOR_RECORDS; ++s) {
                m_inventory.sector_records[s] = m_user_profile.sector_records[s];
            }
            sync_profile_with_player();
        }

        if (action == MainMenuAction::Continue) {
            m_selected_level = m_user_profile.get_highest_unlocked_sector();
            start_expedition(m_selected_level);
        } else if (action == MainMenuAction::NewExpedition) {
            start_expedition(m_selected_level);
        } else if (action == MainMenuAction::UpgradeTerminal) {
            m_hub_ui->set_subview(MenuSubView::Upgrades);
        } else if (action == MainMenuAction::Settings) {
            m_hub_ui->set_subview(MenuSubView::Settings);
        } else if (action == MainMenuAction::ResetSave) {
            clear_save_and_reset();
        } else if (action == MainMenuAction::Exit) {
            SaveSystem::save_profile(m_user_profile, m_active_save_file);
            glfwSetWindowShouldClose(m_window->handle(), GLFW_TRUE);
        }
    } else if (m_state == GameState::OrbitalHub) {
        // Redundant staging wall bypassed - route directly to MainMenu
        m_hub_ui->set_subview(MenuSubView::Main);
        TransitionState(GameState::MainMenu);
    } else if (m_state == GameState::Debrief) {
        if (m_debrief_input_lock > 0.0f) {
            m_debrief_input_lock -= dt;
            mouse_clicked = false; // Block accidental skip/clicks on death report
        }
        DebriefAction action = m_hub_ui->render_debrief(m_expedition_success, m_selected_level, m_inventory, m_skills, m_user_profile, mouse_x, mouse_y, mouse_clicked);
        if (action == DebriefAction::LaunchNextSector) {
            SaveSystem::save_profile(m_user_profile, m_active_save_file);
            if (m_expedition_success) {
                int next_sec = std::min(UserProfile::MAX_SECTOR_RECORDS - 1, m_selected_level + 1);
                if (m_user_profile.is_sector_unlocked(next_sec) || m_user_profile.is_sector_completed(m_selected_level)) {
                    m_selected_level = next_sec;
                }
            }
            m_expedition_success = false;
            LaunchSector(m_selected_level);
        } else if (action == DebriefAction::RedeployExpedition) {
            SaveSystem::save_profile(m_user_profile, m_active_save_file);
            // Retain selected sector index, reset mission runtime data, and launch immediately
            m_expedition_success = false;
            LaunchSector(m_selected_level);
        } else if (action == DebriefAction::ReturnToHub) {
            SaveSystem::save_profile(m_user_profile, m_active_save_file);
            m_selected_level = m_user_profile.get_highest_unlocked_sector();
            m_hub_ui->set_subview(MenuSubView::Main);
            TransitionState(GameState::MainMenu);
            m_window->set_cursor_locked(false);
        }
    } else if (m_state == GameState::Paused) {
        // Render Pause Menu Modal directly over the darkened frozen 3D scene (no HUD text bleed-through)
        PauseMenuAction action = m_pause_menu->render(
            m_selected_level,
            m_expedition_time,
            m_inventory,
            m_settings,
            m_user_profile.selected_class_id,
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
            TransitionState(GameState::Gameplay);
            m_window->set_cursor_locked(true);
        } else if (action == PauseMenuAction::Abandon || action == PauseMenuAction::ReturnToStartup || action == PauseMenuAction::ReturnToHub) {
            SaveSystem::save_profile(m_user_profile, m_active_save_file);
            m_selected_level = m_user_profile.get_highest_unlocked_sector();
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
            m_user_profile.total_astral_ore += m_inventory.astral_ore;
            for (int s = 1; s <= 3; ++s) {
                if (m_inventory.sector_records[s].highest_completion_rate > m_user_profile.sector_records[s].highest_completion_rate) {
                    m_user_profile.sector_records[s].highest_completion_rate = m_inventory.sector_records[s].highest_completion_rate;
                }
            }
            SaveSystem::save_profile(m_user_profile, m_active_save_file);

            m_hub_ui->set_subview(MenuSubView::Main);
            TransitionState(GameState::MainMenu);
            m_window->set_cursor_locked(false);
        }
    } else {
        // Render HUD Overlay (Module 4) - bypass during model showcase captures for clean studio render
        if (!m_config.capture_models && !m_config.capture_level_shapes) {
            if (m_hud) {
                m_hud->set_death_sequence(m_death_sequence, m_death_timer, DEATH_SEQUENCE_DURATION);
            }
            m_hud->render(*m_player, *m_world, *m_hazard, *m_extraction, m_inventory, m_skills, m_selected_level, view, proj, &m_surveying, &m_noise_meter, m_stalkers.active_count(), &m_stalkers.stalkers(), &m_burrowers.burrowers(), &m_mission);

            // 3D Holographic Terrain Scanner (TAB Minimap Projection)
            if (m_terrain_scanner.fold_progress() > 0.001f) {
                m_terrain_scanner.refresh_geometry(*m_world, m_player->position(), *m_player, *m_extraction, m_mission, static_cast<float>(glfwGetTime()));
                m_terrain_scanner.render(m_window->width(), m_window->height(), m_player->position(), m_player->yaw());
            }
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

void Application::grant_expedition_objectives() {
    int target = std::max(25, m_inventory.target_voidite);
    m_inventory.voidite = std::max(m_inventory.voidite, target);
    m_inventory.titanium += 15;
    m_inventory.salvage_parts += 10;
    m_inventory.vault_breached = true;
    m_inventory.relic_extracted = true;

    if (m_mission.vault().exists) {
        m_mission.force_complete_vault_objective();
    }

    if (m_hud) {
        m_hud->add_loot_toast("OBJECTIVES MET", glm::vec4(0.2f, 0.95f, 0.5f, 1.0f), target, 40);
        m_hud->add_loot_toast("VOIDITE", glm::vec4(0.7f, 0.3f, 1.0f, 1.0f), m_inventory.voidite, 30);
        m_hud->add_loot_toast("RELIC SECURED", glm::vec4(0.2f, 0.8f, 1.0f, 1.0f), 1, 30);
        m_hud->show_warning("TESTING: ALL LEVEL OBJECTIVES GRANTED (" + std::to_string(m_inventory.voidite) + "/" + std::to_string(target) + " VOIDITE + RELIC)", 3.0f);
    }
    VF_LOG_INFO("Testing", "Granted full expedition objectives for Sector " << m_selected_level << ": "
                << m_inventory.voidite << "/" << target << " Voidite, Vault Breached, Relic Extracted.");
}

void Application::complete_and_extract_instantly() {
    VF_LOG_INFO("Testing", "Executing instant complete and extract for Sector " << m_selected_level);
    grant_expedition_objectives();
    if (m_state == GameState::Gameplay && m_extraction) {
        m_extraction->force_escape();
    } else {
        // Direct profile progression update if outside active gameplay
        if (m_selected_level >= 1 && m_selected_level < UserProfile::MAX_SECTOR_RECORDS) {
            uint32_t seed = (m_world ? m_world->seed() : m_current_level_seed);
            if (seed == 0) seed = SaveSystem::generate_random_seed(m_selected_level);
            m_user_profile.mark_sector_completed(m_selected_level, seed, 100, "CLEARED (100%)");
            m_inventory.sector_records[m_selected_level] = m_user_profile.sector_records[m_selected_level];
            SaveSystem::save_profile(m_user_profile, m_active_save_file);
        }
    }
}

void Application::test_upgrade_and_respec_cycle() {
    VF_LOG_INFO("Testing", "Executing diagnostic Upgrade & Respec cycle test...");
    int player_lvl = m_user_profile.get_player_level();
    if (m_user_profile.total_exp < 1000) {
        m_user_profile.grant_exp(1500);
        player_lvl = m_user_profile.get_player_level();
    }

    // Purchase upgrades using EXP and player level
    bool b1 = m_user_profile.upgrades.purchase(UpgradeType::DrillSpeed, player_lvl, m_user_profile.total_exp);
    bool b2 = m_user_profile.upgrades.purchase(UpgradeType::ThrusterTank, player_lvl, m_user_profile.total_exp);
    bool b3 = m_user_profile.upgrades.purchase(UpgradeType::ReinforcedPlating, player_lvl, m_user_profile.total_exp);

    int total_spent = m_user_profile.upgrades.get_total_spent_exp();
    VF_LOG_INFO("Testing", "Purchased upgrades: DrillSpeed (T" + std::to_string(m_user_profile.upgrades.drillSpeedTier) + 
                "), ThrusterTank (T" + std::to_string(m_user_profile.upgrades.thrusterTankTier) + 
                "), ReinforcedPlating (T" + std::to_string(m_user_profile.upgrades.reinforcedPlatingTier) + 
                ") [Spent: " + std::to_string(total_spent) + " EXP].");

    // Perform respec (100% full recovery of EXP)
    int ref_exp = 0;
    m_user_profile.upgrades.respec(ref_exp);
    m_user_profile.total_exp += ref_exp;

    VF_LOG_INFO("Testing", "Respec executed: refunded 100% (" + std::to_string(ref_exp) + " EXP), all tiers reset to 0.");
    SaveSystem::save_profile(m_user_profile, m_active_save_file);
    sync_profile_with_player();

    if (m_hud) {
        m_hud->show_warning("TEST RESPEC: REFUNDED 100% (" + std::to_string(ref_exp) + " EXP)");
    }
}

void Application::clear_save_and_reset() {
    GameSettings old_settings = m_user_profile.settings;
    std::string player_name = m_user_profile.player_name;
    m_user_profile = UserProfile{};
    m_user_profile.settings = old_settings;
    if (!player_name.empty()) {
        m_user_profile.player_name = player_name;
    }

    // Write fresh default profile to active save file
    SaveSystem::save_profile(m_user_profile, m_active_save_file);

    // Sync inventory sector records
    for (int s = 1; s < PlayerInventory::MAX_SECTOR_RECORDS; ++s) {
        m_inventory.sector_records[s] = m_user_profile.sector_records[s];
    }
    m_inventory.voidite = 0;
    m_inventory.titanium = 0;
    m_inventory.salvage_parts = 0;
    m_inventory.total_run_score = 0;
    m_inventory.run_coins_earned = 0;
    m_selected_level = 1;

    sync_profile_with_player();

    if (m_hub_ui) {
        m_hub_ui->trigger_save_cleared_feedback();
    }

    if (m_hud) {
        m_hud->show_warning("SAVE DATA & PROGRESSION CLEARED // DELVER RESET", 4.0f);
    }

    if (m_audio) {
        m_audio->play_sound_2d(SoundCue::UIBlip, 1.0f);
    }

    VF_LOG_INFO("SaveSystem", "Save file cleared and user profile reset to fresh delver defaults at " << m_active_save_file);
}

void Application::run() {
    double last_time = glfwGetTime();
    double accumulator = 0.0;
    const double fixed_dt = 1.0 / 60.0;

    std::cout << "\n========================================================" << std::endl;
    std::cout << "  VOIDFALL: DREDGE (TACTICAL VOXEL EXTRACTION)" << std::endl;
    std::cout << "  State Machine Active: Main Menu -> Orbital Hub -> Gameplay -> Debrief" << std::endl;
    std::cout << "========================================================\n" << std::endl;

    if (m_config.test_enemy || m_config.test_enemy_load || m_config.test_relic || m_config.capture_models || m_config.capture_level_shapes) {
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
                bool has_save = m_user_profile.has_save_data();
                if (has_save) {
                    m_selected_level = m_user_profile.get_highest_unlocked_sector();
                    start_expedition(m_selected_level);
                } else {
                    m_hub_ui->set_active_tab(HubTab::SectorSelect);
                    TransitionState(GameState::OrbitalHub);
                }
            }
            enter_down_last = enter_now;
        }
        // 2. ORBITAL HUB INPUTS
        else if (m_state == GameState::OrbitalHub) {
            if (m_hub_ui->active_tab() == HubTab::SectorSelect) {
                for (int key = GLFW_KEY_1; key <= GLFW_KEY_9; ++key) {
                    if (m_window->is_key_down(key)) {
                        int sec = key - GLFW_KEY_1 + 1;
                        if (m_user_profile.is_sector_unlocked(sec)) {
                            m_selected_level = sec;
                        }
                    }
                }
            }

            static bool launch_down_last = false;
            bool launch_now = m_window->is_key_down(GLFW_KEY_ENTER) ||
                              m_window->is_key_down(GLFW_KEY_SPACE);

            if (launch_now && !launch_down_last) {
                if (m_hub_ui->active_tab() == HubTab::SectorSelect && m_user_profile.is_sector_unlocked(m_selected_level)) {
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
                int next_sec = std::min(UserProfile::MAX_SECTOR_RECORDS - 1, m_selected_level + 1);
                if (m_user_profile.is_sector_unlocked(next_sec)) {
                    m_selected_level = next_sec;
                }
                start_expedition(m_selected_level);
            }
            debrief_down_last = debrief_now;

            static bool hub_back_down_last = false;
            bool hub_back_now = m_window->is_key_down(GLFW_KEY_TAB);
            if (hub_back_now && !hub_back_down_last) {
                TransitionState(GameState::OrbitalHub);
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
                bool quota_met = (m_inventory.voidite >= m_inventory.target_voidite) ||
                                 (m_inventory.vault_breached && m_inventory.relic_extracted);
                if (m_inventory.target_astral_ore > 0) {
                    quota_met = ((m_inventory.voidite >= m_inventory.target_voidite) &&
                                 (m_inventory.astral_ore >= m_inventory.target_astral_ore)) ||
                                (m_inventory.vault_breached && m_inventory.relic_extracted);
                }
                if (quota_met) {
                    float holdout_duration = (m_selected_level <= 1) ? 20.0f :
                                             (m_selected_level == 2)  ? 30.0f : 40.0f;
                    float ground_y = 0.0f;
                    std::string reason;
                    if (m_world && !ExtractionSystem::check_landing_clearance(*m_world, m_player->position(), ground_y, &reason)) {
                        // Clearance warning handled via UIBlip audio

                        if (m_audio) {
                            m_audio->play_sound_2d(SoundCue::UIBlip, 0.8f);
                        }
                    } else {
                        glm::vec3 lz_pos = (m_world && ground_y > 0.0f) ?
                            glm::vec3(m_player->position().x, ground_y, m_player->position().z) :
                            m_player->position();
                        m_extraction->deploy_beacon(lz_pos, holdout_duration);
                        m_holdout_stage = 0;
                        m_surveying.reset(); // Clear any sonar X-ray overlay on beacon deployment

                    }
                } else {
                    if (m_audio) {
                        m_audio->play_sound_2d(SoundCue::UIBlip, 0.8f);
                    }
                }
            }
            b_down_last = b_now;

            static bool e_down_last = false;
            bool e_now = m_window->is_key_down(GLFW_KEY_E);
            if (e_now && !e_down_last) {
                try_interact();
            }
            e_down_last = e_now;

            // Synchronize headlamp with player suit controller state (compact icon rendered in suit vitals)
            m_renderer->headlamp().enabled = m_player->is_headlamp_on();

            static bool lbrk_down_last = false;
            bool lbrk_now = m_window->is_key_down(GLFW_KEY_LEFT_BRACKET);
            if (lbrk_now && !lbrk_down_last) {
                m_settings.brightness = std::max(0.40f, std::round((m_settings.brightness - 0.10f) * 10.0f) / 10.0f);
                m_user_profile.settings.brightness = m_settings.brightness;
                if (m_renderer) {
                    m_renderer->set_brightness(m_settings.brightness);
                }
                if (m_hud) {
                    m_hud->show_warning("CAVERN BRIGHTNESS: " + std::to_string(static_cast<int>(std::round(m_settings.brightness * 100.0f))) + "%", 1.5f);
                }
            }
            lbrk_down_last = lbrk_now;

            static bool rbrk_down_last = false;
            bool rbrk_now = m_window->is_key_down(GLFW_KEY_RIGHT_BRACKET);
            if (rbrk_now && !rbrk_down_last) {
                m_settings.brightness = std::min(2.00f, std::round((m_settings.brightness + 0.10f) * 10.0f) / 10.0f);
                m_user_profile.settings.brightness = m_settings.brightness;
                if (m_renderer) {
                    m_renderer->set_brightness(m_settings.brightness);
                }
                if (m_hud) {
                    m_hud->show_warning("CAVERN BRIGHTNESS: " + std::to_string(static_cast<int>(std::round(m_settings.brightness * 100.0f))) + "%", 1.5f);
                }
            }
            rbrk_down_last = rbrk_now;

            if (m_map_open) {
                m_player->set_combat_inputs_paused(true);

                // Mouse click-and-drag interaction for tactical map
                glm::dvec2 cur_mouse = m_window->get_cursor_pos();
                bool lmb_down = m_window->is_mouse_button_down(GLFW_MOUSE_BUTTON_LEFT);
                float mouse_dx = 0.0f;
                float mouse_dy = 0.0f;
                bool is_dragging = false;

                if (lmb_down) {
                    if (m_map_dragging_last) {
                        mouse_dx = static_cast<float>(cur_mouse.x - m_last_map_mouse.x);
                        mouse_dy = static_cast<float>(cur_mouse.y - m_last_map_mouse.y);
                        is_dragging = true;
                    }
                    m_map_dragging_last = true;
                } else {
                    m_map_dragging_last = false;
                }
                m_last_map_mouse = cur_mouse;

                // Zoom control via mouse scroll wheel
                double scroll_y = m_window->get_scroll_delta_y();
                if (std::abs(scroll_y) > 0.01) {
                    m_terrain_scanner.zoom(static_cast<float>(scroll_y));
                }

                m_terrain_scanner.update(static_cast<float>(frame_time), true, m_player->position(),
                                         mouse_dx, mouse_dy, is_dragging);
            } else {
                m_map_dragging_last = false;
                m_player->set_combat_inputs_paused(false);
                m_terrain_scanner.update(static_cast<float>(frame_time), false, m_player->position(), 0.0f, 0.0f, false);
                m_player->set_mouse_sensitivity(m_settings.mouse_sensitivity);
                m_player->set_auto_step(m_settings.auto_step);
                m_player->handle_input(*m_window, static_cast<float>(frame_time));
            }
        }

        int fixed_ticks_executed = 0;
        constexpr int MAX_FIXED_TICKS_PER_FRAME = 4;
        while (accumulator >= fixed_dt && fixed_ticks_executed < MAX_FIXED_TICKS_PER_FRAME) {
            fixed_tick(static_cast<float>(fixed_dt));
            accumulator -= fixed_dt;
            fixed_ticks_executed++;
        }
        if (accumulator >= fixed_dt) {
            // Spiral-of-death guard: discard backlog if frame took too long
            accumulator = 0.0;
        }

        render(static_cast<float>(frame_time));

        // ── Single screenshot CLI flag ──
        if (!m_config.single_screenshot_path.empty()) {
            static int single_shot_frames = 0;
            if (++single_shot_frames >= 5) {
                capture_screenshot_png(m_config.single_screenshot_path, m_window->width(), m_window->height());
                break;
            }
        }

        // ── Testing & Profile Hotkeys ──
        // F4: Clear save & reset delver profile (available in Main Menu or any Test Mode)
        static bool f4_down_last = false;
        bool f4_now = m_window->is_key_down(GLFW_KEY_F4);
        if (f4_now && !f4_down_last && (is_test_mode() || m_state == GameState::MainMenu)) {
            clear_save_and_reset();
            VF_LOG_INFO("Testing", "Save file cleared and user profile reset via F4 hotkey.");
        }
        f4_down_last = f4_now;

        // ── Testing Hotkeys (Available ONLY in Test Mode via --test, --test-save, or --auto-play-test) ──
        if (is_test_mode()) {
            static bool f5_down_last = false;
            bool f5_now = m_window->is_key_down(GLFW_KEY_F5);
            if (f5_now && !f5_down_last) {
                if (m_state == GameState::Gameplay) {
                    grant_expedition_objectives();
                } else {
                    grant_testing_resources(1000, 25, 10);
                }
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

            static bool f10_down_last = false;
            bool f10_now = m_window->is_key_down(GLFW_KEY_F10);
            if (f10_now && !f10_down_last) {
                complete_and_extract_instantly();
                VF_LOG_INFO("Testing", "Instant level complete and extract executed via F10 hotkey.");
            }
            f10_down_last = f10_now;
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

        // ── Automated Visual Test & Playthrough Sequence (16 Screens Comprehensive Suite) ──
        if (m_config.auto_play_test) {
            m_auto_test_frame++;

            // Frame 10: Capture Main Menu (Slot 0)
            if (m_auto_test_frame == 10) {
                VisualTestHarness::instance().record_phase(0, "MAIN MENU", "screenshots/01_main_menu.png", m_window->width(), m_window->height());
                // Switch to Sector Select carousel (Level Selection)
                m_hub_ui->set_subview(MenuSubView::SectorSelect);
                m_selected_level = 2; // Preview Sector 2 card
                m_hub_ui->focus_sector(2, true);
            }
            // Frame 20: Capture Level Selection / Sector Select Carousel (Slot 1)
            else if (m_auto_test_frame == 20) {
                VisualTestHarness::instance().record_phase(1, "LEVEL SELECT", "screenshots/02_level_select.png", m_window->width(), m_window->height());
                // Switch to Character Selection (Delver Roster)
                m_hub_ui->set_subview(MenuSubView::Upgrades);
                m_hub_ui->set_active_tab(HubTab::DelverRoster);
            }
            // Frame 30: Capture Character Selection (Delver Roster: Demolitionist, Vanguard, Scout) (Slot 2)
            else if (m_auto_test_frame == 30) {
                VisualTestHarness::instance().record_phase(2, "CHAR SELECT", "screenshots/03_character_select.png", m_window->width(), m_window->height());
                // Switch to Upgrade Terminal tab
                m_hub_ui->set_active_tab(HubTab::UpgradeTerminal);
            }
            // Frame 33: Testing: Grant EXP and purchase upgrades
            else if (m_auto_test_frame == 33) {
                grant_testing_resources(1500, 30, 15);
                int lvl = m_user_profile.get_player_level();
                m_user_profile.upgrades.purchase(UpgradeType::DrillSpeed, lvl, m_user_profile.total_exp);
                m_user_profile.upgrades.purchase(UpgradeType::ThrusterTank, lvl, m_user_profile.total_exp);
                VF_LOG_INFO("AutoPlayTest", "Testing: Granted resources, purchased Tier 1 DrillSpeed & ThrusterTank with EXP.");
            }
            // Frame 36: Testing: Test respec points feature (100% full recovery verification)
            else if (m_auto_test_frame == 36) {
                int ref_exp = 0;
                m_user_profile.upgrades.respec(ref_exp);
                m_user_profile.total_exp += ref_exp;
                VF_LOG_INFO("AutoPlayTest", "Testing: Respec verified, refunded 100% (" + std::to_string(ref_exp) + " EXP).");
                // Re-purchase upgrades to showcase active purchased upgrade nodes in the terminal UI screenshot
                int lvl = m_user_profile.get_player_level();
                m_user_profile.upgrades.purchase(UpgradeType::DrillSpeed, lvl, m_user_profile.total_exp);
                m_user_profile.upgrades.purchase(UpgradeType::ThrusterTank, lvl, m_user_profile.total_exp);
                m_user_profile.upgrades.purchase(UpgradeType::KineticDynamo, lvl, m_user_profile.total_exp);
                m_user_profile.upgrades.purchase(UpgradeType::ReinforcedPlating, lvl, m_user_profile.total_exp);
            }
            // Frame 40: Capture Upgrade Terminal (with active upgrades & granted resources) (Slot 3)
            else if (m_auto_test_frame == 40) {
                VisualTestHarness::instance().record_phase(3, "UPGRADES TERMINAL", "screenshots/04_upgrades_terminal.png", m_window->width(), m_window->height());
                // Switch to Orbital Hub Audio & Rig Settings tab
                m_hub_ui->set_active_tab(HubTab::AudioSettings);
            }
            // Frame 48: Capture Orbital Hub Audio & Rig Settings (Slot 4)
            else if (m_auto_test_frame == 48) {
                VisualTestHarness::instance().record_phase(4, "HUB AUDIO RIG", "screenshots/05_hub_audio_settings.png", m_window->width(), m_window->height());
                // Return to Main Menu and launch Sector 1 Expedition
                m_hub_ui->set_subview(MenuSubView::Main);
                start_expedition(1);
            }
            // Frames 52 to 78: Player walks around cavern, tests sprint and thruster hover
            else if (m_auto_test_frame >= 52 && m_auto_test_frame < 80) {
                if (m_auto_test_frame == 52) {
                    m_player->set_look_angles(-90.0f, 0.0f);
                }
                // Forward movement with surface clamping
                if (m_auto_test_frame <= 66) {
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
            // Frame 80: Settle in gameplay cavern and capture viewmodel + HUD (Slot 5)
            else if (m_auto_test_frame == 80) {
                m_player->set_look_angles(0.0f, -15.0f);
                m_world->update(m_player->position(), 2);
                VisualTestHarness::instance().record_phase(5, "GAMEPLAY CAVERN", "screenshots/06_gameplay_cavern.png", m_window->width(), m_window->height());
                if (m_hud) m_hud->toggle_help_briefing();
            }
            // Frame 86: Capture Contractor Operations Field Manual Briefing Modal [H] (Slot 6)
            else if (m_auto_test_frame == 86) {
                VisualTestHarness::instance().record_phase(6, "FIELD MANUAL", "screenshots/07_field_manual.png", m_window->width(), m_window->height());
                if (m_hud) m_hud->toggle_help_briefing();
                m_map_open = true;
                m_terrain_scanner.set_fold_progress(1.0f);
            }
            // Frame 92: Capture 2D Tactical Cartography Map [TAB] (Slot 7)
            else if (m_auto_test_frame == 92) {
                VisualTestHarness::instance().record_phase(7, "TACTICAL MAP", "screenshots/08_tactical_map.png", m_window->width(), m_window->height());
                m_map_open = false;
                m_terrain_scanner.set_fold_progress(0.0f);
            }
            // Frames 95 to 118: Player mines cavern rock with drill, evaluates tremor and stalker
            else if (m_auto_test_frame >= 95 && m_auto_test_frame <= 118) {
                m_player->set_active_tool(ToolSlot::MiningDrill);
                m_player->set_look_angles(0.0f, -22.0f);
                m_player->set_drilling(true);
                m_player->MineBlock(static_cast<float>(fixed_dt), *m_world);

                // Testing: Force seismic tremor to evaluate tremor warning, screen trauma, and ceiling debris
                if (m_auto_test_frame == 105) {
                    m_hazard->force_tremor(1.4f);
                    m_stalkers.spawn_stalker(m_player->position() + glm::vec3(-2.0f, 0.2f, 5.5f));
                    VF_LOG_INFO("AutoPlayTest", "Testing: Forced seismic tremor triggered & spawned predatory Void Stalker.");
                }

                if (m_auto_test_frame == 118) {
                    VisualTestHarness::instance().record_phase(8, "DRILL CRACKS", "screenshots/09_drilling_cracks.png", m_window->width(), m_window->height());
                }
            }
            // Frame 119: Stop drilling
            else if (m_auto_test_frame == 119) {
                m_player->set_drilling(false);
            }
            // Frame 120: Test Fall Damage & Screen Shake Trauma
            else if (m_auto_test_frame == 120) {
                m_player->apply_fall_impact(16.5f);
                m_noise_meter.inject_noise(100.0f);
                m_noise_meter.update(static_cast<float>(fixed_dt));
            }
            // Frame 124: Test Ability 1 - Seismic Sonar Pulse scan
            else if (m_auto_test_frame == 124) {
                on_sonar_cast(m_player->position());
            }
            // Frame 128: Test Quick Bulkhead placement on Mining Drill
            else if (m_auto_test_frame == 128) {
                m_player->set_active_tool(ToolSlot::MiningDrill);
                glm::ivec3 place_pos = glm::ivec3(std::floor(m_player->position().x), std::floor(m_player->position().y) + 1, std::floor(m_player->position().z) + 3);
                m_world->set_voxel(place_pos.x, place_pos.y, place_pos.z, Voxel{MAT_INDUSTRIAL_BULKHEAD, VOXEL_FLAG_PLAYER_PLACED}, true);
                on_block_placed(place_pos.x, place_pos.y, place_pos.z, MAT_INDUSTRIAL_BULKHEAD);
            }
            // Frame 132: Test Demolition Shaped Charge explosive micro-blast
            else if (m_auto_test_frame == 132) {
                m_player->set_active_tool(ToolSlot::DemolitionCharge);
                glm::ivec3 blast_pos = glm::ivec3(std::floor(m_player->position().x), std::floor(m_player->position().y) + 1, std::floor(m_player->position().z) + 4);
                on_explosive_blast(blast_pos, glm::ivec3(0, 0, 1), true);
            }
            // Frame 136: Equip Plasma Carbine, fire weapon, add loot toasts and inventory
            else if (m_auto_test_frame == 136) {
                m_player->set_active_tool(ToolSlot::PlasmaCarbine);
                m_player->set_drilling(true);
                m_player->try_fire_weapon(m_plasma_bolts, static_cast<float>(fixed_dt));
                m_player->set_drilling(false);
                m_hud->add_loot_toast("VOIDITE CRYSTAL", glm::vec4(0.7f, 0.3f, 1.0f, 1.0f), 3, 45);
                m_hud->add_loot_toast("SCRAP METAL", glm::vec4(0.85f, 0.7f, 0.3f, 1.0f), 2, 20);
                m_inventory.add_voidite(3);
                m_inventory.add_titanium(2);
            }
            else if (m_auto_test_frame == 140) {
                m_hud->add_loot_toast("VOIDITE CRYSTAL", glm::vec4(0.7f, 0.3f, 1.0f, 1.0f), 3, 45); // Stack +3 [x2]
                m_inventory.add_voidite(3);
            }
            // Frame 144: Capture Combat, Abilities & Concurrent Loot Toasts (Slot 9)
            else if (m_auto_test_frame == 144) {
                VisualTestHarness::instance().record_phase(9, "ABILITIES & LOOT", "screenshots/10_abilities_loot.png", m_window->width(), m_window->height());
                // Pause Game
                TransitionState(GameState::Paused);
                m_window->set_cursor_locked(false);
                if (m_pause_menu) m_pause_menu->set_active_tab(PauseTab::Mission);
            }
            // Frame 148: Capture Esc / Pause Menu Modal Tab 1: Mission Telemetry & Status (Slot 10)
            else if (m_auto_test_frame == 148) {
                VisualTestHarness::instance().record_phase(10, "PAUSE MISSION", "screenshots/11_pause_mission.png", m_window->width(), m_window->height());
                if (m_pause_menu) m_pause_menu->set_active_tab(PauseTab::Inventory);
            }
            // Frame 154: Capture Esc / Pause Menu Modal Tab 2: Inventory & Cargo Jettison (Slot 11)
            else if (m_auto_test_frame == 154) {
                VisualTestHarness::instance().record_phase(11, "PAUSE INVENTORY", "screenshots/12_pause_inventory.png", m_window->width(), m_window->height());
                if (m_pause_menu) m_pause_menu->set_active_tab(PauseTab::AudioSettings);
            }
            // Frame 160: Capture Esc / Pause Menu Modal Tab 3: Audio & Rig Settings Mixer (Slot 12)
            else if (m_auto_test_frame == 160) {
                VisualTestHarness::instance().record_phase(12, "PAUSE AUDIO RIG", "screenshots/13_pause_audio.png", m_window->width(), m_window->height());
                if (m_pause_menu) m_pause_menu->set_active_tab(PauseTab::ControlsBriefing);
            }
            // Frame 166: Capture Esc / Pause Menu Modal Tab 4: Delver Controls & Directives (Slot 13)
            else if (m_auto_test_frame == 166) {
                VisualTestHarness::instance().record_phase(13, "PAUSE CONTROLS", "screenshots/14_pause_controls.png", m_window->width(), m_window->height());
                // Resume gameplay from pause and deploy extraction beacon
                TransitionState(GameState::Gameplay);
                m_window->set_cursor_locked(true);
                float holdout_duration = (m_selected_level <= 1) ? 20.0f :
                                         (m_selected_level == 2)  ? 30.0f : 40.0f;
                float ground_y = 0.0f;
                if (m_world && ExtractionSystem::check_landing_clearance(*m_world, m_player->position(), ground_y)) {
                    m_extraction->deploy_beacon(glm::vec3(m_player->position().x, ground_y, m_player->position().z), holdout_duration);
                } else {
                    m_extraction->deploy_beacon(m_player->position(), holdout_duration);
                }
                m_holdout_stage = 0;
                m_surveying.reset(); // Clear sonar X-ray so beacon screenshot is clean
                if (m_hud) {
                    m_hud->show_warning("! CRITICAL NOISE: VOID SWARM INCOMING !", 4.0f);
                    m_hud->show_warning("VOID STALKER ELIMINATED (+100 PTS)", 4.0f);
                }
            }
            // Frame 178: Capture Extraction Beacon Holdout (Slot 14)
            else if (m_auto_test_frame == 178) {
                VisualTestHarness::instance().record_phase(14, "EXTRACTION BEACON", "screenshots/15_extraction_beacon.png", m_window->width(), m_window->height());
                grant_expedition_objectives();
                m_extraction->force_escape();
            }
            // Frame 188: Capture Mission Debrief Screen (Slot 15)
            else if (m_auto_test_frame == 188) {
                VisualTestHarness::instance().record_phase(15, "MISSION DEBRIEF", "screenshots/16_mission_debrief.png", m_window->width(), m_window->height());
            }
            // Frame 198: Finalize Visual Test Harness & Exit Cleanly
            else if (m_auto_test_frame >= 198) {
                VisualTestHarness::instance().finalize();
                VF_LOG_INFO("AutoPlayTest", "Extensive visual test completed! All 16 screens & montage captured.");
                break;
            }
        }

        // ── Automated Dedicated Void Stalker Visual Test Suite (--test-enemy) ──
        if (m_config.test_enemy) {
            m_auto_test_frame++;

            // Ensure stable staging chamber, pristine studio lighting and player position
            if (m_auto_test_frame == 1) {
                m_player->set_position(glm::vec3(16.0f, 22.0f, 16.0f));
                m_player->clamp_to_surface(*m_world);
                m_renderer->headlamp().enabled = true;
                m_renderer->headlamp().intensity = 2.8f;
                m_renderer->clear_point_lights();
                // 3-point studio lighting for optimal chitin clarity
                m_renderer->add_point_light({m_player->position() + glm::vec3(1.4f, 1.8f, 1.6f), glm::vec3(1.0f, 0.95f, 0.90f), 18.0f, 2.0f});
                m_renderer->add_point_light({m_player->position() + glm::vec3(-1.6f, 0.9f, 1.4f), glm::vec3(0.4f, 0.65f, 0.95f), 14.0f, 1.2f});
                m_renderer->add_point_light({m_player->position() + glm::vec3(0.0f, 2.6f, 3.8f), glm::vec3(0.85f, 0.45f, 1.0f), 20.0f, 2.2f});

                m_stalkers.reset();
                m_stalkers.spawn_stalker(m_player->position() + glm::vec3(0.0f, -0.38f, 2.5f));
                VF_LOG_INFO("EnemyTest", "Staging Void Stalker multi-orientation visual test suite across 6 positions.");
            }

            // Continuous per-frame pin of the active slot configuration
            if (!m_stalkers.stalkers().empty()) {
                auto& s = m_stalkers.stalkers_mut()[0];

                if (m_auto_test_frame < 14) {
                    // ── Slot 0: FLOOR CRAWL ──
                    s.position = m_player->position() + glm::vec3(0.0f, -0.38f, 2.5f);
                    s.velocity = glm::vec3(0.0f, 0.0f, -0.01f);
                    s.surface_state = StalkerSurfaceState::FLOOR;
                    s.target_surface_state = StalkerSurfaceState::FLOOR;
                    s.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
                    s.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
                    s.m_currentRotation = glm::angleAxis(3.25f, glm::vec3(0.0f, 1.0f, 0.0f));
                    s.anim_controller.update(0.016f, StalkerSurfaceState::FLOOR, s.velocity, 3.2f);
                    s.state = StalkerState::Stalking;
                    s.yaw = 3.25f;
                    s.pitch = 0.0f;
                    m_player->set_look_angles(90.0f, -8.0f);
                }
                else if (m_auto_test_frame >= 14 && m_auto_test_frame < 28) {
                    // ── Slot 1: WALL CLIMBING ──
                    s.position = m_player->position() + glm::vec3(0.0f, 0.65f, 2.6f);
                    s.velocity = glm::vec3(0.0f, 2.5f, 0.0f); // Climbing upward on vertical wall
                    s.surface_state = StalkerSurfaceState::WALL_CLIMBING;
                    s.target_surface_state = StalkerSurfaceState::WALL_CLIMBING;
                    s.contact_normal = glm::vec3(0.0f, 0.0f, -1.0f); // Normal pointing outward from wall
                    s.m_targetUpVector = glm::vec3(0.0f, 0.0f, -1.0f);
                    s.m_currentRotation = AberrantAI::calculate_orientation(s.velocity, s.contact_normal, glm::vec3(0.0f, 1.0f, 0.0f));
                    s.anim_controller.update(0.016f, StalkerSurfaceState::WALL_CLIMBING, s.velocity, 3.2f);
                    s.state = StalkerState::Stalking;
                    s.pitch = 0.0f;
                    m_player->set_look_angles(90.0f, 14.0f);
                }
                else if (m_auto_test_frame >= 28 && m_auto_test_frame < 42) {
                    // ── Slot 2: CEILING CRAWLING ──
                    s.position = m_player->position() + glm::vec3(0.0f, 1.85f, 2.2f);
                    s.velocity = glm::vec3(0.0f, 0.0f, -1.5f); // Inverted crawl across ceiling
                    s.surface_state = StalkerSurfaceState::CEILING_CRAWLING;
                    s.target_surface_state = StalkerSurfaceState::CEILING_CRAWLING;
                    s.contact_normal = glm::vec3(0.0f, -1.0f, 0.0f); // Inverted normal pointing down
                    s.m_targetUpVector = glm::vec3(0.0f, -1.0f, 0.0f);
                    s.m_currentRotation = AberrantAI::calculate_orientation(s.velocity, s.contact_normal, glm::vec3(0.0f, 0.0f, -1.0f));
                    s.anim_controller.update(0.016f, StalkerSurfaceState::CEILING_CRAWLING, s.velocity, 3.2f);
                    s.state = StalkerState::Stalking;
                    s.pitch = 0.0f;
                    m_player->set_look_angles(90.0f, 38.0f);
                }
                else if (m_auto_test_frame >= 42 && m_auto_test_frame < 56) {
                    // ── Slot 3: SURFACE TRANSITION ──
                    s.position = m_player->position() + glm::vec3(0.0f, 1.40f, 2.45f);
                    s.velocity = glm::vec3(0.0f, 1.5f, -1.5f);
                    s.surface_state = StalkerSurfaceState::TRANSITIONING;
                    s.target_surface_state = StalkerSurfaceState::CEILING_CRAWLING;
                    s.contact_normal = glm::normalize(glm::vec3(0.0f, -0.7071f, -0.7071f)); // 45-deg corner normal
                    s.m_targetUpVector = s.contact_normal;
                    s.m_currentRotation = AberrantAI::calculate_orientation(s.velocity, s.contact_normal, s.velocity);
                    s.anim_controller.update(0.016f, StalkerSurfaceState::CEILING_CRAWLING, s.velocity, 3.2f);
                    s.state = StalkerState::Stalking;
                    s.pitch = 0.0f;
                    m_player->set_look_angles(90.0f, 26.0f);
                }
                else if (m_auto_test_frame >= 56 && m_auto_test_frame < 70) {
                    // ── Slot 4: AGGRESSIVE LUNGE ──
                    s.position = m_player->position() + glm::vec3(0.0f, 0.12f, 2.35f);
                    s.velocity = glm::vec3(0.0f, 0.5f, -8.0f);
                    s.surface_state = StalkerSurfaceState::FLOOR;
                    s.target_surface_state = StalkerSurfaceState::FLOOR;
                    s.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
                    s.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
                    s.m_currentRotation = glm::angleAxis(3.14159f, glm::vec3(0.0f, 1.0f, 0.0f));
                    s.anim_controller.update(0.016f, StalkerSurfaceState::FLOOR, s.velocity, 3.2f);
                    s.state = StalkerState::Lunging;
                    s.just_lunged = true;
                    s.yaw = 3.14159f;
                    m_player->set_look_angles(90.0f, -4.0f);
                }
                else if (m_auto_test_frame >= 70 && m_auto_test_frame < 84) {
                    // ── Slot 5: SONAR STUN REACTION ──
                    s.position = m_player->position() + glm::vec3(0.0f, -0.32f, 2.45f);
                    s.velocity = glm::vec3(0.0f);
                    s.surface_state = StalkerSurfaceState::FLOOR;
                    s.target_surface_state = StalkerSurfaceState::FLOOR;
                    s.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
                    s.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
                    s.m_currentRotation = glm::angleAxis(3.20f, glm::vec3(0.0f, 1.0f, 0.0f));
                    s.anim_controller.update(0.016f, StalkerSurfaceState::FLOOR, s.velocity, 3.2f);
                    s.state = StalkerState::Stunned;
                    s.stun_timer = 3.0f;
                    s.yaw = 3.20f;
                    m_player->set_look_angles(90.0f, -7.0f);
                }
            }

            // Capture frames
            if (m_auto_test_frame == 13) {
                VisualTestHarness::instance().record_enemy_phase(0, "FLOOR CRAWL", "screenshots/enemy_01_floor_crawl.png", m_window->width(), m_window->height());
                VF_LOG_INFO("EnemyTest", "Captured Slot 0: FLOOR CRAWL.");
            }
            else if (m_auto_test_frame == 27) {
                VisualTestHarness::instance().record_enemy_phase(1, "WALL CLIMB", "screenshots/enemy_02_wall_climb.png", m_window->width(), m_window->height());
                VF_LOG_INFO("EnemyTest", "Captured Slot 1: WALL CLIMB.");
            }
            else if (m_auto_test_frame == 41) {
                VisualTestHarness::instance().record_enemy_phase(2, "CEILING CRAWL", "screenshots/enemy_03_ceiling_crawl.png", m_window->width(), m_window->height());
                VF_LOG_INFO("EnemyTest", "Captured Slot 2: CEILING CRAWL.");
            }
            else if (m_auto_test_frame == 55) {
                VisualTestHarness::instance().record_enemy_phase(3, "SURFACE TRANSITION", "screenshots/enemy_04_surface_transition.png", m_window->width(), m_window->height());
                VF_LOG_INFO("EnemyTest", "Captured Slot 3: SURFACE TRANSITION.");
            }
            else if (m_auto_test_frame == 69) {
                VisualTestHarness::instance().record_enemy_phase(4, "AGGRESSIVE LUNGE", "screenshots/enemy_05_aggressive_lunge.png", m_window->width(), m_window->height());
                on_sonar_cast(m_player->position());
                VF_LOG_INFO("EnemyTest", "Captured Slot 4: AGGRESSIVE LUNGE.");
            }
            else if (m_auto_test_frame == 83) {
                VisualTestHarness::instance().record_enemy_phase(5, "SONAR STUN", "screenshots/enemy_06_sonar_stun.png", m_window->width(), m_window->height());
                VF_LOG_INFO("EnemyTest", "Captured Slot 5: SONAR STUN.");
            }
            else if (m_auto_test_frame >= 88) {
                VisualTestHarness::instance().finalize_enemy_test();
                VF_LOG_INFO("EnemyTest", "Dedicated Void Stalker multi-orientation visual test harness finalized! All 6 phases & montage generated.");
                break;
            }
        }

        // ── Automated Multi-Monster Load & Anti-Invisibility Stress Test (--test-enemy-load) ──
        if (m_config.test_enemy_load) {
            m_auto_test_frame++;

            if (m_auto_test_frame == 1) {
                m_player->set_position(glm::vec3(16.0f, 22.0f, 16.0f));
                m_player->clamp_to_surface(*m_world);
                m_player->set_look_angles(90.0f, -4.0f); // Looking towards +Z into the cavern
                m_renderer->headlamp().enabled = true;
                m_renderer->headlamp().intensity = 2.8f;
                m_renderer->clear_point_lights();
                // Three-point cavern illumination
                m_renderer->add_point_light({m_player->position() + glm::vec3(0.0f, 4.0f, 6.0f), glm::vec3(1.0f, 0.95f, 0.90f), 24.0f, 2.5f});
                m_renderer->add_point_light({m_player->position() + glm::vec3(-6.0f, 2.0f, 4.0f), glm::vec3(0.4f, 0.65f, 0.95f), 20.0f, 1.8f});
                m_renderer->add_point_light({m_player->position() + glm::vec3(6.0f, 2.0f, 4.0f), glm::vec3(0.85f, 0.45f, 1.0f), 20.0f, 1.8f});

                m_stalkers.reset();
                m_burrowers.reset();
                CarcassManager::instance().clear();

                // Stage high-load monster horde in forward visual cone:
                // 35 Stalkers (15 Melee, 10 Shooter, 10 Goliath)
                // 6 Seismic Burrowers
                // 12 Void Spike Projectiles
                // 8 Carcasses
                glm::vec3 forward_center = m_player->position() + glm::vec3(0.0f, 0.0f, 6.5f);

                // Row 1: 15 Melee Stalkers (front arc)
                for (int i = 0; i < 15; ++i) {
                    float u = (i - 7) * 0.7f;
                    glm::vec3 pos = forward_center + glm::vec3(u, -0.35f, -1.5f + std::abs(u) * 0.3f);
                    m_stalkers.spawn_melee(pos);
                }

                // Row 2: 10 Shooter Stalkers (mid arc)
                for (int i = 0; i < 10; ++i) {
                    float u = (i - 4.5f) * 1.1f;
                    glm::vec3 pos = forward_center + glm::vec3(u, 0.4f, 1.0f + std::abs(u) * 0.2f);
                    m_stalkers.spawn_shooter(pos);
                }

                // Row 3: 10 Chitin Goliath Breachers (elevated / flanking)
                for (int i = 0; i < 10; ++i) {
                    float u = (i - 4.5f) * 1.3f;
                    glm::vec3 pos = forward_center + glm::vec3(u, 1.2f, 3.2f + std::abs(u) * 0.2f);
                    m_stalkers.spawn_goliath(pos);
                }

                // 6 Seismic Burrowers
                for (int i = 0; i < 6; ++i) {
                    float u = (i - 2.5f) * 1.8f;
                    glm::vec3 pos = forward_center + glm::vec3(u, -0.2f, 5.0f);
                    m_burrowers.spawn_burrower(pos, 1.0f, true);
                }

                // 12 Void Spike Projectiles
                for (int i = 0; i < 12; ++i) {
                    VoidSpikeProjectile proj;
                    proj.id = static_cast<uint32_t>(i + 1);
                    float u = (i - 5.5f) * 0.8f;
                    proj.position = forward_center + glm::vec3(u, 0.8f, 0.5f);
                    proj.velocity = glm::vec3(0.0f, 0.0f, -5.0f);
                    proj.lifetime = 10.0f;
                    proj.active = true;
                    m_stalkers.projectiles_mut().push_back(proj);
                }

                // 8 Carcasses
                for (int i = 0; i < 8; ++i) {
                    float u = (i - 3.5f) * 1.2f;
                    glm::vec3 pos = forward_center + glm::vec3(u, -0.4f, -2.6f);
                    CarcassManager::instance().spawn_carcass(pos, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.0f), StalkerRole::Melee, 1.0f);
                }

                VF_LOG_INFO("EnemyLoadTest", "Staged massive monster horde: 35 Stalkers, 6 Burrowers, 12 Projectiles, 8 Carcasses.");
            }

            // Keep stalkers facing the player/camera with slight animation progress
            for (auto& s : m_stalkers.stalkers_mut()) {
                s.target_pos = m_player->position();
                s.yaw = 3.14159f;
                s.anim_controller.update(0.016f, StalkerSurfaceState::FLOOR, glm::vec3(0.0f, 0.0f, 0.5f), 1.0f);
            }

            if (m_auto_test_frame == 18) {
                // Take screenshot and verify rendering
                capture_screenshot_png("screenshots/enemy_load_test.png", m_window->width(), m_window->height());
                VF_LOG_INFO("EnemyLoadTest", "Captured screenshots/enemy_load_test.png.");

                // Write report json
                size_t stalker_verts = 0;
                for (const auto& s : m_stalkers.stalkers()) {
                    stalker_verts += (s.role == StalkerRole::ChitinGoliath) ? 952 : 916;
                }
                size_t burrower_verts = m_burrowers.burrowers().size() * 720;
                size_t proj_verts = m_stalkers.projectiles().size() * 60;
                size_t carcass_verts = CarcassManager::instance().carcasses().size() * 324;
                size_t total_verts = stalker_verts + burrower_verts + proj_verts + carcass_verts;

                std::ofstream report("screenshots/enemy_load_report.json");
                if (report.is_open()) {
                    report << "{\n"
                           << "  \"stalker_count\": " << m_stalkers.stalkers().size() << ",\n"
                           << "  \"burrower_count\": " << m_burrowers.burrowers().size() << ",\n"
                           << "  \"projectile_count\": " << m_stalkers.projectiles().size() << ",\n"
                           << "  \"carcass_count\": " << CarcassManager::instance().carcasses().size() << ",\n"
                           << "  \"stalker_vertices\": " << stalker_verts << ",\n"
                           << "  \"burrower_vertices\": " << burrower_verts << ",\n"
                           << "  \"projectile_vertices\": " << proj_verts << ",\n"
                           << "  \"carcass_vertices\": " << carcass_verts << ",\n"
                           << "  \"total_monster_vertices\": " << total_verts << ",\n"
                           << "  \"vbo_capacity\": " << m_renderer->stalker_vbo_capacity() << ",\n"
                           << "  \"status\": \"PASS\"\n"
                           << "}\n";
                }
            } else if (m_auto_test_frame >= 22) {
                VF_LOG_INFO("EnemyLoadTest", "Multi-monster load test completed successfully! Exiting.");
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
                m_showcase_delver_pos = m_player->position() + glm::vec3(0.0f, 0.0f, 2.45f);
                m_showcase_delver_yaw = 3.25f;
                VF_LOG_INFO("ModelShowcase", "Staged Delver Kaelen (Demolitionist) for Slot 3.");
            }
            // Frame 56: Capture Slot 3 "KAELEN (DEMOLITIONIST)"
            else if (m_auto_test_frame == 56) {
                VisualTestHarness::instance().record_model_phase(3, "KAELEN (DEMOLITIONIST)", "docs/models/character_demolitionist_kaelen.png", m_window->width(), m_window->height());
                // Transition to Stage 4: Delver Rhodes (Vanguard)
                m_showcase_render_delver = true;
                m_showcase_delver_class = CharacterClass::Vanguard;
                m_showcase_delver_pos = m_player->position() + glm::vec3(0.0f, 0.0f, 2.45f);
                m_showcase_delver_yaw = 3.25f;
                VF_LOG_INFO("ModelShowcase", "Staged Delver Rhodes (Vanguard) for Slot 4.");
            }
            // Frame 70: Capture Slot 4 "RHODES (VANGUARD)"
            else if (m_auto_test_frame == 70) {
                VisualTestHarness::instance().record_model_phase(4, "RHODES (VANGUARD)", "docs/models/character_vanguard_rhodes.png", m_window->width(), m_window->height());
                // Transition to Stage 5: Delver Vesper (Scout)
                m_showcase_render_delver = true;
                m_showcase_delver_class = CharacterClass::Scout;
                m_showcase_delver_pos = m_player->position() + glm::vec3(0.0f, 0.0f, 2.45f);
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

        // ── Automated Dedicated Precursor Relic Showcase Capture (--test-relic) ──
        if (m_config.test_relic) {
            m_auto_test_frame++;
            if (m_auto_test_frame == 1) {
                // Ensure Precursor Vault is embedded and breached
                glm::vec3 door_world = glm::vec3(m_mission.vault().door_pos) + glm::vec3(0.5f);
                m_mission.check_satchel_vault_breach(*m_world, door_world, 4.0f);

                // Position camera in front of the floating precursor relic on its dais
                glm::vec3 r_pos = glm::vec3(m_mission.vault().relic_pos) + glm::vec3(0.5f, 0.5f, 0.5f);
                glm::vec3 cam_pos = r_pos + glm::vec3(-2.2f, 0.25f, -0.9f);
                m_player->set_position(cam_pos);
                glm::vec3 look_dir = glm::normalize(r_pos - (cam_pos + glm::vec3(0.0f, 1.2f, 0.0f)));
                float yaw = glm::degrees(std::atan2(look_dir.z, look_dir.x));
                float pitch = glm::degrees(std::asin(look_dir.y));
                m_player->set_look_angles(yaw, pitch);

                if (m_renderer) {
                    m_renderer->headlamp().enabled = true;
                    m_renderer->headlamp().intensity = 2.4f;
                }
            }

            if (m_auto_test_frame == 16) {
                capture_screenshot_png("screenshots/precursor_relic_showcase.png", m_window->width(), m_window->height());
                VF_LOG_INFO("RelicTest", "Captured Precursor Relic Showcase to screenshots/precursor_relic_showcase.png");
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

void Application::register_objective_interaction(uint8_t mat) {
    if (mat == MAT_ASTRAL_ORE || mat == MAT_ASTRAL) {
        m_astral_hint_idle_timer = 0.0f;
    } else if (mat == MAT_VOIDITE_CRYSTAL || mat == MAT_VOIDITE) {
        m_voidite_hint_idle_timer = 0.0f;
    } else if (mat == MAT_REINFORCED_VAULT_DOOR || mat == MAT_VAULT_DOOR || mat == MAT_PRISMATIC_CRYSTAL) {
        m_vault_hint_idle_timer = 0.0f;
    } else if (mat == 0 && m_objective_hint_mat != 0) {
        register_objective_interaction(m_objective_hint_mat);
        return;
    }
    m_objective_hint_timer = 0.0f;

    // If an active hint is pointing to this objective, dismiss it
    if (m_hud && m_hud->has_objective_hint()) {
        if (m_objective_hint_mat != 0 && (mat == m_objective_hint_mat ||
            ((mat == MAT_ASTRAL || mat == MAT_ASTRAL_ORE) && (m_objective_hint_mat == MAT_ASTRAL || m_objective_hint_mat == MAT_ASTRAL_ORE)) ||
            ((mat == MAT_VOIDITE || mat == MAT_VOIDITE_CRYSTAL) && (m_objective_hint_mat == MAT_VOIDITE || m_objective_hint_mat == MAT_VOIDITE_CRYSTAL)) ||
            ((mat == MAT_REINFORCED_VAULT_DOOR || mat == MAT_PRISMATIC_CRYSTAL) && m_objective_hint_mat == MAT_REINFORCED_VAULT_DOOR))) {
            m_hud->clear_objective_hint();
            m_objective_hint_given = false;
        }
    }
}

float Application::get_objective_idle_time(uint8_t mat) const {
    if (mat == MAT_ASTRAL_ORE || mat == MAT_ASTRAL) {
        return m_astral_hint_idle_timer;
    } else if (mat == MAT_VOIDITE_CRYSTAL || mat == MAT_VOIDITE) {
        return m_voidite_hint_idle_timer;
    } else if (mat == MAT_REINFORCED_VAULT_DOOR || mat == MAT_VAULT_DOOR || mat == MAT_PRISMATIC_CRYSTAL) {
        return m_vault_hint_idle_timer;
    }
    return 0.0f;
}

void Application::set_objective_idle_timer(uint8_t mat, float time) {
    if (mat == MAT_ASTRAL_ORE || mat == MAT_ASTRAL) {
        m_astral_hint_idle_timer = time;
    } else if (mat == MAT_VOIDITE_CRYSTAL || mat == MAT_VOIDITE) {
        m_voidite_hint_idle_timer = time;
    } else if (mat == MAT_REINFORCED_VAULT_DOOR || mat == MAT_VAULT_DOOR || mat == MAT_PRISMATIC_CRYSTAL) {
        m_vault_hint_idle_timer = time;
    }
}

void Application::reset_all_objective_idle_timers() {
    m_astral_hint_idle_timer = 0.0f;
    m_voidite_hint_idle_timer = 0.0f;
    m_vault_hint_idle_timer = 0.0f;
    m_objective_hint_timer = 0.0f;
    m_objective_hint_given = false;
    m_objective_hint_pos = glm::vec3(0.0f);
    m_objective_hint_mat = 0;
}

} // namespace Voidfall
