#include "void_stalker.hpp"
#include "seismic_burrower.hpp"
#include "../../ai/swarm_manager.hpp"
#include "../../core/logger.hpp"
#include "../../voxel/world.hpp"
#include "../../voxel/level_shapes.hpp"
#include "../carcass_manager.hpp"
#include "../flare.hpp"
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <random>

namespace Voidfall {

// ─── VoidStalkerManager ──────────────────────────────────────────────

void VoidStalkerManager::spawn_stalker(const glm::vec3 &pos,
                                       float difficulty_mul, StalkerRole role,
                                       bool emerging) {
  VoidStalker s;
  s.id = m_next_id++;
  s.role = role;
  s.position = pos;
  s.patrol_anchor = pos;
  s.patrol_waypoint = pos;
  s.patrol_timer = 2.0f;
  s.hp = 40.0f * difficulty_mul;
  s.max_hp = s.hp;
  s.damage = 18.0f * difficulty_mul;
  s.scale = 1.3f;
  s.move_speed = 4.8f;

  if (role == StalkerRole::ChitinGoliath) {
    // Chitin Goliath (Heavy Carapace Breacher / Apex Precursor Vault Boss)
    s.hp = 220.0f * difficulty_mul;
    s.max_hp = s.hp;
    s.damage = 36.0f * difficulty_mul;
    s.cleave_damage = 38.0f * difficulty_mul;
    s.slam_damage = 48.0f * difficulty_mul;
    s.charge_damage = 52.0f * difficulty_mul;
    s.scale = 2.2f;
    s.move_speed = 2.6f;
    s.detection_range = 28.0f;
    s.flee_hp_threshold = 0.0f; // Boss fights to the death, NEVER flees
    s.boss_slam_cooldown = 2.0f;
    s.boss_charge_cooldown = 4.5f;
    s.boss_quill_cooldown = 3.5f;
  }

  s.state = emerging ? StalkerState::Emerging : StalkerState::Idle;
  s.state_timer = 0.0f;
  s.glow_phase = static_cast<float>(m_next_id) * 1.7f; // Offset pulsing
  s.screech_timer = 0.0f; // Alert frame screech available on first sighting
  s.spawn_grace_timer = 0.0f; // Baseline live/test spawn grace timer (set to 4.0s for roosting)
  s.stuck_checkpoint_pos = pos;
  s.stuck_checkpoint_timer = 0.0f;
  s.just_screeched = false;
  s.burrow_entry = pos;
  s.burrow_duration = (role == StalkerRole::ChitinGoliath) ? 2.2f :
                      (role == StalkerRole::Shooter) ? 1.5f : 1.2f;
  s.drill_timer = 0.0f;
  s.chitter_timer = 0.15f;
  s.just_dug = emerging;

  // Stagger player-tracking path calculations across a 10 Hz accumulator (m_pathTimer >= 0.10f)
  // with randomized initial phase per enemy [0.0f, 0.10f)
  static std::mt19937 phase_rng(1337);
  std::uniform_real_distribution<float> phase_dist(0.0f, 0.10f);
  s.m_pathTimer = phase_dist(phase_rng);

  // Initialize surface climbing orientation & rig alignment
  s.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
  s.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
  s.surface_state = StalkerSurfaceState::FLOOR;
  s.target_surface_state = StalkerSurfaceState::FLOOR;
  s.prev_surface_state = StalkerSurfaceState::FLOOR;
  s.surface_offset = glm::vec3(0.0f, 0.05f, 0.0f);
  glm::vec3 init_fwd(0.0f, 0.0f, 1.0f);
  s.m_targetRotation =
      AberrantAI::calculate_orientation(init_fwd, s.contact_normal, init_fwd);
  s.m_currentRotation = s.m_targetRotation;
  s.anim_controller.set_immediate_state(StalkerSurfaceState::FLOOR);

  m_stalkers.push_back(s);
}

void VoidStalkerManager::spawn_emerging(const glm::vec3 &pos, const glm::vec3 &normal,
                                        float difficulty_mul, StalkerRole role) {
  spawn_stalker(pos, difficulty_mul, role, true);
  if (!m_stalkers.empty()) {
    auto &s = m_stalkers.back();
    glm::vec3 n = normal;
    if (glm::length(n) < 0.01f) n = glm::vec3(0.0f, 1.0f, 0.0f);
    else n = glm::normalize(n);

    s.contact_normal = n;
    s.m_targetUpVector = n;
    s.burrow_entry = pos;
    s.burrow_dir = -n; // burrowing in from behind the rock face
    s.surface_state = AberrantAI::classify_normal(n);
    s.target_surface_state = s.surface_state;
    s.prev_surface_state = s.surface_state;

    glm::vec3 fwd(0.0f, 0.0f, 1.0f);
    if (std::abs(glm::dot(n, fwd)) > 0.85f) {
      fwd = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    s.m_targetRotation = AberrantAI::calculate_orientation(fwd, n, fwd);
    s.m_currentRotation = s.m_targetRotation;
    s.anim_controller.set_immediate_state(s.surface_state);
  }
}

void VoidStalkerManager::spawn_roosting(const glm::vec3 &pos, StalkerRole role,
                                        float difficulty_mul) {
  spawn_stalker(pos, difficulty_mul, role);
  if (!m_stalkers.empty()) {
    auto &s = m_stalkers.back();
    s.state = StalkerState::Roosting;
    s.state_timer = 0.0f;
    s.velocity = glm::vec3(0.0f);
    s.screech_timer = 0.0f; // Alert screech ready if triggered
    s.spawn_grace_timer = 4.0f; // Initial dormancy grace window

    // Pre-spawned cavern fauna initialize dormant on cavern walls or ceilings
    if (role == StalkerRole::ChitinGoliath) {
      // Boss monster stands ground on floor in imposing sentinel stance, never clinging to ceiling
      s.surface_state = StalkerSurfaceState::FLOOR;
      s.target_surface_state = StalkerSurfaceState::FLOOR;
      s.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
      s.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
      s.spawn_grace_timer = 0.0f; // Boss is immediately vigilant
      s.slumber_timer = 0.0f;
    } else if (pos.y >= 7.0f) {
      s.surface_state = StalkerSurfaceState::CEILING_CRAWLING;
      s.target_surface_state = StalkerSurfaceState::CEILING_CRAWLING;
      s.contact_normal = glm::vec3(0.0f, -1.0f, 0.0f);
      s.m_targetUpVector = glm::vec3(0.0f, -1.0f, 0.0f);
    } else {
      s.surface_state = StalkerSurfaceState::WALL_CLIMBING;
      s.target_surface_state = StalkerSurfaceState::WALL_CLIMBING;
      s.contact_normal = glm::vec3(1.0f, 0.0f, 0.0f);
      s.m_targetUpVector = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::vec3 init_fwd(0.0f, 0.0f, 1.0f);
    s.m_targetRotation = AberrantAI::calculate_orientation(
        init_fwd, s.contact_normal, glm::vec3(0.0f, 0.0f, 1.0f));
    s.m_currentRotation = s.m_targetRotation;
    s.anim_controller.set_immediate_state(s.surface_state);
  }
}

void VoidStalkerManager::spawn_ambient_stalker(const glm::vec3 &room_center,
                                               const World &world) {
  static std::mt19937 rng(1337);
  std::uniform_real_distribution<float> offset_dist(-5.5f, 5.5f);

  glm::vec3 spawn_pos = room_center;
  for (int attempt = 0; attempt < 15; ++attempt) {
    float ox = offset_dist(rng);
    float oz = offset_dist(rng);
    glm::vec3 candidate = room_center + glm::vec3(ox, 0.0f, oz);
    candidate.x = std::clamp(candidate.x, 6.0f, 66.0f);
    candidate.z = std::clamp(candidate.z, 6.0f, 66.0f);

    int ix = static_cast<int>(std::floor(candidate.x));
    int iz = static_cast<int>(std::floor(candidate.z));

    // Find floor in room column
    for (int y = static_cast<int>(candidate.y) + 4;
         y >= static_cast<int>(candidate.y) - 3; --y) {
      if (world.get_voxel(ix, y, iz).material_id == MAT_AIR &&
          world.get_voxel(ix, y - 1, iz).material_id != MAT_AIR) {
        float surf_y = AberrantAI::get_voxel_surface_y(world, ix, y - 1, iz, candidate.x, candidate.z);
        spawn_pos =
            glm::vec3(candidate.x, surf_y + 0.05f, candidate.z);
        break;
      }
    }
  }

  // Alternate ambient roles so caverns have both Red Melee and Green Shooter
  // threats
  StalkerRole role =
      (m_next_id % 2 == 0) ? StalkerRole::Shooter : StalkerRole::Melee;
  spawn_stalker(spawn_pos, 1.0f, role);
  if (!m_stalkers.empty()) {
    auto &s = m_stalkers.back();
    s.state = StalkerState::Idle;
    s.surface_state = StalkerSurfaceState::FLOOR;
    s.target_surface_state = StalkerSurfaceState::FLOOR;
    s.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
    s.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
    s.patrol_anchor = spawn_pos;
    s.patrol_waypoint = spawn_pos;
  }
}

void VoidStalkerManager::spawn_wave(const glm::vec3 &player_pos, int count,
                                    const World &world, bool emerging) {
  if (m_spawn_cooldown > 0.0f)
    return;

  float base_angle = static_cast<float>(m_next_id % 8) * (glm::two_pi<float>() / 8.0f);
  for (int i = 0; i < count; ++i) {
    float wave_angle = base_angle + static_cast<float>(i) * (glm::two_pi<float>() / std::max(1, count));
    glm::vec3 surface_normal(0.0f, 1.0f, 0.0f);
    glm::vec3 spawn = find_spawn_pos(player_pos, world, wave_angle, &surface_normal);
    // Tactical wave composition: Even index is Melee (Red), Odd index is
    // Shooter (Green)
    StalkerRole role = (i % 2 == 0) ? StalkerRole::Melee : StalkerRole::Shooter;
    if (emerging) {
      spawn_emerging(spawn, surface_normal, 1.0f, role);
    } else {
      spawn_stalker(spawn, 1.0f, role, false);
    }
  }
  m_spawn_cooldown = 8.0f; // Minimum 8s between spawned waves
  VF_LOG_INFO("VoidStalker",
              "Spawned wave of "
                  << count
                  << " stalkers (tactical Melee/Shooter mix) near player");
}

glm::vec3 VoidStalkerManager::find_spawn_pos(const glm::vec3 &near,
                                             const World &world,
                                             float preferred_angle,
                                             glm::vec3 *out_normal) {
  if (out_normal) {
    *out_normal = glm::vec3(0.0f, 1.0f, 0.0f);
  }
  // Smart Spawning: Spawn 18–32m away from player, preferring positions with NO
  // direct line-of-sight (out from behind corners, rock pillars, or cavern
  // bends) so enemies stalk in naturally
  static std::mt19937 rng(42);
  std::uniform_real_distribution<float> angle_dist(0.0f, glm::two_pi<float>());
  std::uniform_real_distribution<float> angle_jitter(-0.35f, 0.35f);
  std::uniform_real_distribution<float> dist_range(18.0f, 32.0f);
  std::uniform_real_distribution<float> height_dist(-2.0f, 5.0f);

  glm::vec3 fallback_candidate = near;
  bool found_any_valid = false;
  glm::vec3 found_normal(0.0f, 1.0f, 0.0f);

  for (int attempt = 0; attempt < 25; ++attempt) {
    float angle = (preferred_angle >= 0.0f) ? (preferred_angle + angle_jitter(rng)) : angle_dist(rng);
    float dist = dist_range(rng);
    float dy = height_dist(rng);

    glm::vec3 candidate(near.x + std::cos(angle) * dist, near.y + dy,
                        near.z + std::sin(angle) * dist);

    // Clamp to sector bounds
    candidate.x = std::clamp(candidate.x, 5.0f, 67.0f);
    candidate.y = std::clamp(candidate.y, 4.0f, 25.0f);
    candidate.z = std::clamp(candidate.z, 5.0f, 67.0f);

    // Check that spawn pos is in air (not inside a wall)
    int ix = static_cast<int>(std::floor(candidate.x));
    int iy = static_cast<int>(std::floor(candidate.y));
    int iz = static_cast<int>(std::floor(candidate.z));

    Voxel v = world.get_voxel(ix, iy, iz);
    // Arbitrary Surface Spawn Selection: Query candidate voxels with exposed
    // air faces
    if (v.material_id == MAT_AIR) {
      struct SurfaceCandidate {
        glm::ivec3 offset;
        glm::vec3 normal;
      };
      const SurfaceCandidate surfaces[6] = {
          {glm::ivec3(0, -1, 0),
           glm::vec3(0.0f, 1.0f, 0.0f)}, // Floor: n = (0, 1, 0)
          {glm::ivec3(0, 1, 0),
           glm::vec3(0.0f, -1.0f, 0.0f)}, // Ceiling: n = (0, -1, 0)
          {glm::ivec3(-1, 0, 0),
           glm::vec3(1.0f, 0.0f, 0.0f)}, // West wall: n = (1, 0, 0)
          {glm::ivec3(1, 0, 0),
           glm::vec3(-1.0f, 0.0f, 0.0f)}, // East wall: n = (-1, 0, 0)
          {glm::ivec3(0, 0, -1),
           glm::vec3(0.0f, 0.0f, 1.0f)}, // North wall: n = (0, 0, 1)
          {glm::ivec3(0, 0, 1),
           glm::vec3(0.0f, 0.0f, -1.0f)} // South wall: n = (0, 0, -1)
      };

      for (const auto &sfc : surfaces) {
        Voxel adj = world.get_voxel(ix + sfc.offset.x, iy + sfc.offset.y,
                                    iz + sfc.offset.z);
        if (adj.material_id != MAT_AIR && adj.material_id != MAT_GAS &&
            adj.material_id != MAT_VOLATILE_SMOKE) {
          glm::vec3 surface_pos =
              glm::vec3(ix + 0.5f, iy + 0.5f, iz + 0.5f) + sfc.normal * 0.35f;
          if (!world.is_solid(glm::ivec3(std::floor(surface_pos.x), std::floor(surface_pos.y), std::floor(surface_pos.z)))) {
            // Enforce minimum 6.5m separation between active stalkers
            bool too_close = false;
            for (const auto &s : m_stalkers) {
              if (s.state != StalkerState::Dead && glm::distance(surface_pos, s.position) < 6.5f) {
                too_close = true;
                break;
              }
            }
            if (too_close) {
              continue;
            }

            bool los =
                has_line_of_sight(surface_pos + glm::vec3(0.0f, 0.5f, 0.0f),
                                  near + glm::vec3(0.0f, 1.5f, 0.0f), world);
            if (!los) {
              if (out_normal) *out_normal = sfc.normal;
              return surface_pos;
            }
            fallback_candidate = surface_pos;
            found_normal = sfc.normal;
            found_any_valid = true;
            break;
          }
        }
      }
    }
  }

  if (found_any_valid) {
    if (out_normal) *out_normal = found_normal;
    return fallback_candidate;
  }

  // Fallback systematic search: never spawn inside solid rock walls
  for (float r = 8.0f; r <= 32.0f; r += 3.0f) {
    for (int a = 0; a < 16; ++a) {
      float ang = a * (glm::two_pi<float>() / 16.0f);
      int sx = static_cast<int>(std::floor(near.x + std::cos(ang) * r));
      int sz = static_cast<int>(std::floor(near.z + std::sin(ang) * r));
      if (sx < 6 || sx > 66 || sz < 6 || sz > 66) continue;
      for (int sy = static_cast<int>(near.y) + 3; sy >= static_cast<int>(near.y) - 4; --sy) {
        if (sy < 4 || sy > 25) continue;
        if (!world.is_solid(glm::ivec3(sx, sy, sz)) &&
            !world.is_solid(glm::ivec3(sx, sy + 1, sz)) &&
            world.is_solid(glm::ivec3(sx, sy - 1, sz))) {
          glm::vec3 cand(sx + 0.5f, sy + 0.1f, sz + 0.5f);
          bool too_close = false;
          for (const auto &s : m_stalkers) {
            if (s.state != StalkerState::Dead && glm::distance(cand, s.position) < 5.0f) {
              too_close = true;
              break;
            }
          }
          if (!too_close) {
            return cand;
          }
        }
      }
    }
  }

  // Stagger final emergency fallback position so multiple stalkers never stack on the exact same coordinate
  float fallback_ang = static_cast<float>(m_stalkers.size()) * 1.57f;
  return near + glm::vec3(std::cos(fallback_ang) * 5.0f, 0.1f, std::sin(fallback_ang) * 5.0f);
}

bool VoidStalkerManager::has_line_of_sight(const glm::vec3 &from,
                                           const glm::vec3 &to,
                                           const World &world) const {
  return AberrantAI::has_line_of_sight(from, to, world);
}

bool VoidStalkerManager::is_in_headlamp(const glm::vec3 &pos,
                                        const glm::vec3 &player_pos,
                                        const glm::vec3 &headlamp_dir,
                                        float inner_angle) const {
  glm::vec3 to_stalker = glm::normalize(pos - player_pos);
  float dot = glm::dot(headlamp_dir, to_stalker);
  float cone_cos = std::cos(glm::radians(inner_angle));
  return dot > cone_cos;
}

VoidStalkerManager::FrameResult VoidStalkerManager::update(
    float dt, const glm::vec3 &player_pos, const glm::vec3 &player_forward,
    const glm::vec3 &headlamp_dir, bool headlamp_on, float noise_level_pct,
    bool player_is_drilling, const World &world,
    const std::vector<SoundEvent> &sound_events, bool player_is_crouching) {
  FrameResult result;
  m_spawn_cooldown = std::max(0.0f, m_spawn_cooldown - dt);
  m_combat_recovery_timer = std::max(0.0f, m_combat_recovery_timer - dt);
  m_global_screech_cooldown = std::max(0.0f, m_global_screech_cooldown - dt);
  m_alert_screech_cooldown = std::max(0.0f, m_alert_screech_cooldown - dt);
  m_global_hiss_cooldown = std::max(0.0f, m_global_hiss_cooldown - dt);
  m_global_chitter_cooldown = std::max(0.0f, m_global_chitter_cooldown - dt);

  // Compute player velocity for predictive leading attacks
  glm::vec3 player_vel(0.0f);
  if (m_has_prev_player_pos && dt > 0.0001f) {
    player_vel = (player_pos - m_prev_player_pos) / dt;
    float speed = glm::length(player_vel);
    if (speed > 25.0f) {
      player_vel = (player_vel / speed) * 25.0f;
    }
  }
  m_prev_player_pos = player_pos;
  m_has_prev_player_pos = true;

  // Build zero-allocation spatial hash grid for swarm entities
  m_spatial_hash.clear();
  for (const auto &stalker : m_stalkers) {
    if (stalker.state != StalkerState::Dead &&
        stalker.state != StalkerState::Dying &&
        stalker.state != StalkerState::Roosting) {
      m_spatial_hash.insert(stalker.id, stalker.position);
    }
  }

  for (auto &s : m_stalkers) {
    glm::vec3 frame_start_pos = s.position;
    // Reset per-frame one-shot notification flags for all entities
    s.screech_timer = std::max(0.0f, s.screech_timer - dt);
    s.spawn_grace_timer = std::max(0.0f, s.spawn_grace_timer - dt);
    s.boss_slam_cooldown = std::max(0.0f, s.boss_slam_cooldown - dt);
    s.boss_charge_cooldown = std::max(0.0f, s.boss_charge_cooldown - dt);
    s.boss_quill_cooldown = std::max(0.0f, s.boss_quill_cooldown - dt);
    s.just_screeched = false;
    s.just_chittered = false;
    s.just_dug = false;
    s.just_escaped = false;
    s.just_spotted_player = false;
    s.just_heard_sound = false;
    s.just_lunged = false;
    s.just_hit_player = false;
    s.just_fired_spine = false;
    s.just_died = false;
    s.just_emerged = false;
    s.just_slammed = false;
    s.just_charged = false;
    s.just_enraged = false;
    s.just_boss_roared = false;
    s.just_stepped = false;

    if (s.state == StalkerState::Dead)
      continue;

    if (s.state == StalkerState::Dying) {
      s.hit_flash_timer = 0.0f;
      s.death_timer += dt;
      s.velocity.y -= 18.0f * dt; // Cavern gravity
      s.position += s.velocity * dt;

      // Ground collision clamp
      float dead_ground = 0.0f;
      if (AberrantAI::find_ground_surface(world, s.position, 0.5f, 2.0f, &dead_ground)) {
        if (s.position.y <= dead_ground + 0.1f) {
          s.position.y = dead_ground;
          s.velocity = glm::vec3(0.0f);
        }
      }

      // Tumble rotation
      float ang_speed = glm::length(s.death_angular_velocity);
      if (ang_speed > 0.001f) {
        glm::quat delta_rot = glm::angleAxis(
            ang_speed * dt, glm::normalize(s.death_angular_velocity));
        s.m_currentRotation = delta_rot * s.m_currentRotation;
      }

      if (s.death_timer >= s.death_duration) {
        s.state = StalkerState::Dead;
        result.stalkers_killed++;
        trigger_combat_recovery((m_sector_level <= 1) ? 6.0f : ((m_sector_level == 2) ? 5.0f : 4.0f));
        CarcassManager::instance().spawn_carcass(
            s.position, s.m_currentRotation, s.death_hit_dir, s.role, s.scale);
      }
      continue;
    }

    float speed_multiplier = SwarmManager::instance().GetSpeedMultiplier();
    float effective_move_speed = s.move_speed * speed_multiplier;

    s.state_timer += dt;
    s.m_pathTickTimer += dt;
    s.route_repath_timer = std::max(0.0f, s.route_repath_timer - dt);
    s.glow_phase += dt * 2.0f;
    s.hit_flash_timer = std::max(0.0f, s.hit_flash_timer - dt);
    s.attack_cooldown = std::max(0.0f, s.attack_cooldown - dt);
    s.projectile_cooldown = std::max(0.0f, s.projectile_cooldown - dt);
    s.slash_fx_timer = std::max(0.0f, s.slash_fx_timer - dt);

    float dist_to_player = glm::distance(s.position, player_pos);

    glm::vec3 to_player = player_pos - s.position;
    float horiz_dist = std::hypot(to_player.x, to_player.z);
    if (horiz_dist > 0.05f) {
      s.pitch = std::atan2(to_player.y, horiz_dist);
    }

    // Dynamic Surface Sampling & Contact Normal Detection with debounced
    // hysteresis
    s.surface_cooldown = std::max(0.0f, s.surface_cooldown - dt);
    s.hit_surface_lock_timer = std::max(0.0f, s.hit_surface_lock_timer - dt);

    SurfaceContactSample sample;
    if (s.state == StalkerState::Roosting) {
      // Roosting enemies are dormant and cling to their ceiling/wall perch
      sample.surface_state = s.surface_state;
      sample.contact_normal = s.contact_normal;
      sample.distance = 0.05f;
      sample.has_contact = true;
    } else if (s.hit_surface_lock_timer > 0.0f) {
      // When recovering from hit/shot, lock firmly to floor to eliminate rapid
      // surface flipping jitter
      sample.surface_state = StalkerSurfaceState::FLOOR;
      sample.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
      sample.distance = 0.05f;
      sample.has_contact = true;
    } else if (dist_to_player > 35.0f) {
      // If distance to player > 35.0m, enter dormant low-tick mode: skip 6-direction raycast collision
      sample.surface_state = s.surface_state;
      sample.contact_normal = s.contact_normal;
      sample.distance = 0.05f;
      sample.has_contact = true;
    } else {
      glm::vec3 sample_target = s.target_pos;
      if (s.route_type == AlternateRouteType::VerticalClimb && s.has_active_alternate_route()) {
        sample_target.y = s.position.y + 2.0f;
      }
      sample = AberrantAI::sample_surface_normal(s.position, world, 1.4f,
                                                 s.surface_state, &sample_target);
    }

    // Debounce state transitions: only allow shifting if cooldown has elapsed.
    // Floor landing override: when descending wall or ceiling and ground is reached, instantly transition to FLOOR
    bool floor_landing_override = (sample.has_contact && sample.surface_state == StalkerSurfaceState::FLOOR &&
                                   (s.surface_state == StalkerSurfaceState::WALL_CLIMBING ||
                                    s.surface_state == StalkerSurfaceState::CEILING_CRAWLING));
    if (sample.surface_state != s.target_surface_state &&
        (s.surface_cooldown <= 0.0f || floor_landing_override)) {
      s.prev_surface_state = s.target_surface_state;
      s.target_surface_state = sample.surface_state;
      s.surface_state = StalkerSurfaceState::TRANSITIONING;
      s.surface_transition_timer = 0.0f;
      s.surface_cooldown = floor_landing_override ? 0.2f : 0.5f; // Faster recovery upon landing
    }

    if (s.surface_transition_timer < 0.2f) {
      s.surface_transition_timer += dt;
      // Smoothly blend contact normal across transition window instead of
      // snapping, guarding against opposite-normal (0,0,0) cancellation NaNs
      float blend = std::clamp(s.surface_transition_timer / 0.2f, 0.0f, 1.0f);
      if (glm::dot(s.contact_normal, sample.contact_normal) < -0.85f) {
        s.contact_normal = sample.contact_normal;
      } else {
        glm::vec3 mixed = glm::mix(s.contact_normal, sample.contact_normal, blend);
        if (glm::length(mixed) > 0.001f && !std::isnan(mixed.x)) {
          s.contact_normal = glm::normalize(mixed);
        } else {
          s.contact_normal = sample.contact_normal;
        }
      }
      s.m_targetUpVector = s.contact_normal;
      if (s.surface_transition_timer >= 0.2f) {
        s.surface_state = s.target_surface_state;
        s.contact_normal = sample.contact_normal;
        s.m_targetUpVector = sample.contact_normal;
      }
    } else {
      s.surface_state = s.target_surface_state;
      if (glm::dot(s.contact_normal, sample.contact_normal) < -0.85f) {
        s.contact_normal = sample.contact_normal;
      } else {
        glm::vec3 mixed = glm::mix(s.contact_normal, sample.contact_normal,
                                   std::clamp(dt * 12.0f, 0.0f, 1.0f));
        if (glm::length(mixed) > 0.001f && !std::isnan(mixed.x)) {
          s.contact_normal = glm::normalize(mixed);
        } else {
          s.contact_normal = sample.contact_normal;
        }
      }
      s.m_targetUpVector = s.contact_normal;
    }

    // Animation controller updates & velocity-proportional play rate modulation
    s.anim_controller.update(dt, s.target_surface_state, s.velocity,
                             s.m_climbSpeedScalar);
    float play_rate =
        s.anim_controller.calculate_play_rate(s.velocity, s.m_climbSpeedScalar);
    s.walk_cycle += play_rate * dt;

    // Smooth Transform & Model Re-Orientation (Quaternion Slerp without
    // snapping)
    glm::vec3 fallback_fwd(std::sin(s.yaw), 0.0f, std::cos(s.yaw));
    // Anti-jitter: When engaged or recovering from shot knockback, keep facing
    // forward toward target
    glm::vec3 orient_fwd = (s.hit_flash_timer > 0.0f || s.is_engaged_state())
                               ? fallback_fwd
                               : s.velocity;
    s.m_targetRotation = AberrantAI::calculate_orientation(
        orient_fwd, s.contact_normal, fallback_fwd);
    s.m_currentRotation = AberrantAI::slerp_rotation(
        s.m_currentRotation, s.m_targetRotation, dt, 10.0f);
    if (std::isnan(s.m_currentRotation.x) || std::isnan(s.m_currentRotation.y) ||
        std::isnan(s.m_currentRotation.z) || std::isnan(s.m_currentRotation.w)) {
      s.m_currentRotation = glm::angleAxis(s.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // Surface Snapping Offset smoothly interpolated
    glm::vec3 target_offset = AberrantAI::compute_surface_snapping_offset(
        s.surface_state, s.contact_normal);
    s.surface_offset = glm::mix(s.surface_offset, target_offset,
                                std::clamp(dt * 8.0f, 0.0f, 1.0f));
    if (std::isnan(s.surface_offset.x) || std::isnan(s.surface_offset.y) ||
        std::isnan(s.surface_offset.z)) {
      s.surface_offset = glm::vec3(0.0f);
    }

    // Ambient Cavern Echo Screeches (subtle, rare & atmospheric - minimum 60s
    // global cooldown)
    if (s.state != StalkerState::Dead && s.state != StalkerState::Stunned) {
      if (m_global_screech_cooldown <= 0.0f && !result.any_screech &&
          s.screech_timer <= 0.0f && dist_to_player > 20.0f && !s.is_in_light) {
        s.just_screeched = true;
        result.any_screech = true;
        s.screech_timer = 50.0f;
        m_global_screech_cooldown =
            60.0f + static_cast<float>((s.id * 37) % 35);
        m_alert_screech_cooldown = std::max(m_alert_screech_cooldown, 12.0f);
        VF_LOG_INFO("VoidStalker",
                    "Stalker " << s.id
                               << " emitted distant cavern echo screech (dist="
                               << dist_to_player << "m)");
      }
    }

    // ─── 1. Acoustic Sound Hearing Processing ────────────────────────
    // Check if any sound events occurred within hearing range
    // (salience-weighted)
    const SoundEvent *best_sound = nullptr;
    float best_salience = 0.0f;

    for (const auto &snd : sound_events) {
      float snd_dist = glm::distance(s.position, snd.position);
      // Subterranean rock transmits acoustic vibrations effectively (0.80x
      // dampening)
      bool sound_los = has_line_of_sight(
          s.position + glm::vec3(0.0f, 0.4f, 0.0f), snd.position, world);
      float effective_radius = snd.audible_radius * (sound_los ? 1.0f : 0.80f);

      if (snd_dist <= effective_radius) {
        float salience = (snd.intensity * (sound_los ? 1.0f : 0.80f)) /
                         std::max(1.0f, snd_dist);
        if (snd.type == SoundEventType::Gunshot) {
          salience *= 3.5f; // Gunfire muzzle blast commands immediate predatory
                            // attention!
        } else if (snd.type == SoundEventType::DemolitionBlast ||
                   snd.type == SoundEventType::SeismicTremor) {
          salience *= 3.5f; // Massive concussive detonation/tremor heavily
                            // commands attention
        } else if (snd.type == SoundEventType::BulletImpact) {
          salience *= 2.2f; // Bullet impact distraction
        } else if (snd.type == SoundEventType::DrillVibration) {
          salience *= 2.5f; // Drilling vibration
        } else if (snd.type == SoundEventType::FootstepSprint) {
          salience *= 2.0f;
        } else if (snd.type == SoundEventType::FootstepWalk) {
          salience *= 1.5f;
        }

        if (salience > best_salience) {
          best_salience = salience;
          best_sound = &snd;
        }
      }
    }

    // Fallback acoustic detection for continuous drill grinding or elevated
    // global noise (suppressed when crouching in stealth unless actively drilling)
    if (!best_sound && (player_is_drilling || (noise_level_pct > 25.0f && !player_is_crouching))) {
      bool sound_path_clear =
          has_line_of_sight(s.position + glm::vec3(0.0f, 0.4f, 0.0f),
                            player_pos + glm::vec3(0.0f, 0.7f, 0.0f), world);
      float obstruction_factor = sound_path_clear ? 1.0f : 0.75f;
      float effective_hearing_radius =
          (player_is_drilling ? 24.0f
                              : (12.0f + (noise_level_pct / 100.0f) * 16.0f)) *
          obstruction_factor;
      if (dist_to_player <= effective_hearing_radius) {
        // Synthetic continuous sound event centered at player
        s.investigation_target = player_pos;
        s.has_sound_target = true;
        s.just_heard_sound = true;
        result.any_heard_sound = true;
      }
    }

    // Check Acoustic Profile system events in StealthSystem
    const AcousticNoiseEvent *best_acoustic_evt = nullptr;
    float best_acoustic_db = 0.0f;
    for (const auto &a_evt : StealthSystem::instance().GetAcousticEvents()) {
      if (a_evt.is_expired())
        continue;
      bool a_los = has_line_of_sight(s.position + glm::vec3(0.0f, 0.4f, 0.0f),
                                     a_evt.position, world);
      float rx_db = AberrantAI::calculate_received_noise_db(
          s.position, a_evt.position, a_evt.db, a_evt.radius, a_los);
      if (rx_db > best_acoustic_db) {
        best_acoustic_db = rx_db;
        best_acoustic_evt = &a_evt;
      }
    }

    if (best_acoustic_evt &&
        best_acoustic_db > AberrantAI::THRESHOLD_INVESTIGATE_DB) {
      s.investigation_target = best_acoustic_evt->position;
      s.has_sound_target = true;
      s.just_heard_sound = true;
      result.any_heard_sound = true;
    }

    if (best_sound) {
      s.investigation_target = best_sound->position;
      s.has_sound_target = true;
      s.just_heard_sound = true;
      result.any_heard_sound = true;
    }

    // ─── 2. Visual & Line-of-Sight Perception Processing ─────────────
    // Check headlamp cone illumination
    s.is_in_light = headlamp_on && dist_to_player < 22.0f &&
                    is_in_headlamp(s.position, player_pos, headlamp_dir, 32.0f);

    // Also check deployed chemical flares / torches
    if (!s.is_in_light) {
      for (const auto &flare : FlareManager::instance().flares()) {
        if (flare.is_alive() &&
            glm::distance(flare.position, s.position) <= flare.light_radius) {
          s.is_in_light = true;
          break;
        }
      }
    }

    // Raycast Line of Sight from stalker eyes to delver torso (skip if > 35.0m in dormant low-tick mode)
    bool los = false;
    if (dist_to_player <= 35.0f) {
      los = has_line_of_sight(s.position + glm::vec3(0.0f, 0.4f, 0.0f),
                              player_pos + glm::vec3(0.0f, 0.7f, 0.0f), world);
    }

    // Visual detection range (shadow vs light vs stealth crouch)
    float visual_range = 10.0f; // Baseline dark shadow vision
    if (s.is_pursuing_attacker) {
      visual_range = s.pursuit_break_dist; // High alert predator pursuit tracking
    } else if (player_is_crouching) {
      visual_range = s.is_in_light ? 6.0f : 4.5f; // Delver crouching has high visual concealment even in illuminated areas
    } else if (s.is_in_light) {
      visual_range = 22.0f; // Directly lit up by flashlight beam
    } else if (s.state == StalkerState::Stalking ||
               s.state == StalkerState::Circling ||
               s.state == StalkerState::Lunging) {
      visual_range = 16.0f; // Active combat engagement
    } else if (s.state == StalkerState::Investigating) {
      visual_range = 12.0f; // Investigating sound
    } else if (noise_level_pct > 25.0f || player_is_drilling) {
      visual_range = 14.0f; // High environmental noise
    }

    // Tactile proximity trigger: physically colliding within 0.85m senses contact.
    bool tactile_proximity = (dist_to_player < 0.85f);

    // Vision cone check: Creature only visually sees targets within its forward field of view.
    // Sneaking targets in crouch stance exploit blind spots: narrowed 90 deg forward cone (45 deg half-angle).
    float vision_half_angle = player_is_crouching ? glm::radians(45.0f) : glm::radians(60.0f);
    bool in_fov = AberrantAI::is_in_vision_cone(s.position, s.forward(), player_pos, vision_half_angle);

    bool can_see_player = false;
    if (s.state == StalkerState::Roosting) {
      if (player_is_crouching) {
        // When delver is sneaking in stealth, roosting enemies (closed eyes, dormant) only awaken
        // if physically bumped (tactile contact) or directly illuminated by headlamp within close visual range (<= 6.0m).
        // Crouching in darkness or at distance (> 6.0m) never awakens them.
        can_see_player = tactile_proximity || (s.is_in_light && los && dist_to_player <= visual_range);
      } else {
        // Standing / walking delver is directly spotted if within visual range and line-of-sight
        can_see_player = (los && dist_to_player <= visual_range) || (s.is_in_light && los && dist_to_player <= visual_range) || tactile_proximity;
      }
    } else if (s.is_engaged_state()) {
      // Active combat engagement: stalker is tracking the delver
      can_see_player = (los && dist_to_player <= visual_range) || tactile_proximity;
    } else if (s.state == StalkerState::Investigating) {
      // Stalker is actively investigating a suspicious sound: alert to disturbances
      if (player_is_crouching) {
        // Crouching delver: MUST be within forward vision cone of creature's eyes, line-of-sight, AND visual range!
        // Stalker cannot see behind its back just because a light is on!
        can_see_player = (los && dist_to_player <= visual_range && in_fov) || tactile_proximity;
      } else {
        // Standing / walking delver is spotted by actively investigating stalker within visual range
        can_see_player = (los && dist_to_player <= visual_range) || tactile_proximity;
      }
    } else {
      // Unalerted / Idling / Emerging:
      if (player_is_crouching) {
        // Sneaking delver: must be within visual range, line-of-sight, AND strictly within forward vision cone of eyes!
        bool visually_spotted = los && (dist_to_player <= visual_range) && in_fov;
        can_see_player = visually_spotted || tactile_proximity;
      } else {
        // Standing / walking delver: forward vision cone, headlamp, or proximity (< 6.2m) with line-of-sight
        bool in_vision = in_fov || (s.is_in_light && los) || (dist_to_player < 6.2f);
        can_see_player = (los && dist_to_player <= visual_range && in_vision) || tactile_proximity;
      }
    }

    if (s.is_engaged_state()) {
      s.has_player_los = los;
    } else {
      s.has_player_los = los && can_see_player;
    }

    if (can_see_player) {
      s.last_seen_player_pos = player_pos;
      s.lost_los_timer = 0.0f;
    } else {
      s.lost_los_timer += dt;
    }

    // Retaliation pursuit tracking de-escalation:
    // If enemy was attacked by player, it pursues the player. It cleanly loses
    // interest and drops pursuit alert if line-of-sight is broken and player remains hidden.
    if (s.is_pursuing_attacker) {
      if (dist_to_player > s.pursuit_break_dist || (player_is_crouching && !can_see_player)) {
        float flee_rate = player_is_crouching ? 2.5f : 1.0f;
        s.pursuit_lost_timer += dt * flee_rate;
        if (s.pursuit_lost_timer >= s.pursuit_break_time) {
          s.is_pursuing_attacker = false;
          s.pursuit_lost_timer = 0.0f;
          s.state = StalkerState::Idle;
          s.state_timer = 0.0f;
          s.velocity = glm::vec3(0.0f);
          VF_LOG_INFO("VoidStalker",
                      "Stalker " << s.id
                                 << " lost track of hidden attacker -> pursuit subsided (dist="
                                 << dist_to_player << "m)");
        }
      } else {
        // Maintained visual contact: reset flee timeout
        s.pursuit_lost_timer = 0.0f;
      }
    }

    // === FSM State Transitions & Behavior ===
    switch (s.state) {

    case StalkerState::Emerging: {
      s.velocity = glm::vec3(0.0f); // Anchored to borehole during emergence animation

      // Periodic digging / rock chipping sound cue & particle trigger
      s.drill_timer -= dt;
      if (s.drill_timer <= 0.0f) {
        s.drill_timer = 0.28f + static_cast<float>((s.id * 7) % 10) * 0.02f;
        s.just_dug = true;
        result.any_digging = true;
      }

      // Agitated chattering clicks during emergence
      s.chitter_timer -= dt;
      if (s.chitter_timer <= 0.0f) {
        s.chitter_timer = 0.35f;
        s.just_chittered = true;
        result.any_chitter = true;
      }

      // Check if emergence duration has completed
      if (s.state_timer >= s.burrow_duration) {
        s.just_emerged = true;
        // Transition into combat state depending on Line of Sight & role
        if (can_see_player) {
          s.state = StalkerState::Stalking;
          s.just_spotted_player = true;
          result.any_spotted = true;
        } else if (s.has_sound_target) {
          s.state = StalkerState::Investigating;
          s.investigation_timer = 4.0f;
        } else {
          s.state = StalkerState::Idle;
        }
        s.state_timer = 0.0f;
        s.drill_timer = 0.0f;
        VF_LOG_INFO("VoidStalker",
                    "Stalker " << s.id << " (Role="
                               << (s.is_goliath() ? "Goliath" : (s.is_shooter() ? "Shooter" : "Melee"))
                               << ") COMPLETED emergence into cavern!");
      }
      break;
    }

    case StalkerState::Roosting: {
      // Roosting enemies remain stationary clamped to surfaces until disturbed
      s.velocity = glm::vec3(0.0f);

      bool directly_damaged = (s.is_pursuing_attacker || s.hp < s.max_hp ||
                               s.hit_flash_timer > 0.0f);
      if (directly_damaged) {
        // Disturbed by direct player fire -> immediately enter combat
        s.state = StalkerState::Stalking;
        s.state_timer = 0.0f;
        s.lost_los_timer = 0.0f;
        s.target_pos = player_pos;
        s.is_pursuing_attacker = true;
        s.just_spotted_player = true;
        result.any_spotted = true;
        s.surface_state = StalkerSurfaceState::FLOOR;
        s.target_surface_state = StalkerSurfaceState::FLOOR;
        s.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
        s.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
        VF_LOG_INFO("VoidStalker",
                    "Roosting Stalker "
                        << s.id << " DISTURBED by direct weapon damage!");
        break;
      }

      // Calculate arriving noise in dB from acoustic events or spatial sounds
      float sound_db = best_acoustic_db;
      if (best_sound) {
        float emitted =
            best_sound->db > 0.0f
                ? best_sound->db
                : (best_sound->type == SoundEventType::DemolitionBlast  ? 100.0f
                   : best_sound->type == SoundEventType::Gunshot        ? 75.0f
                   : best_sound->type == SoundEventType::DrillVibration ? 55.0f
                   : best_sound->type == SoundEventType::FootstepWalk
                       ? 15.0f
                       : best_sound->intensity);
        bool snd_los =
            has_line_of_sight(s.position + glm::vec3(0.0f, 0.4f, 0.0f),
                              best_sound->position, world);
        float rx = AberrantAI::calculate_received_noise_db(
            s.position, best_sound->position, emitted,
            best_sound->audible_radius, snd_los);
        sound_db = std::max(sound_db, rx);
      }

      // Boss Apex Guardian awaken check (never stays asleep when player approaches or makes sound)
      if (s.is_boss()) {
        if (can_see_player || dist_to_player < 26.0f || sound_db > 20.0f || directly_damaged) {
          s.state = StalkerState::Stalking;
          s.state_timer = 0.0f;
          s.lost_los_timer = 0.0f;
          s.target_pos = player_pos;
          s.is_pursuing_attacker = true;
          s.just_spotted_player = true;
          s.just_boss_roared = true;
          s.just_screeched = true;
          result.any_screech = true;
          result.any_spotted = true;
          result.any_boss_roar = true;
          s.surface_state = StalkerSurfaceState::FLOOR;
          s.target_surface_state = StalkerSurfaceState::FLOOR;
          s.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
          s.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
          VF_LOG_INFO("VoidStalker", "APEX BOSS Chitin Goliath " << s.id
                      << " AWAKENS WITH TERRIFYING BATTLE ROAR!");
          break;
        }
      }

      // Spawn grace period safeguard: Roosting enemies ignore distant visual sightlines
      // and quiet ambient footsteps during entry grace unless physically disturbed
      bool direct_disturbance = tactile_proximity || (s.is_in_light && los && dist_to_player <= visual_range);
      bool sight_awakens = direct_disturbance || (can_see_player && (s.spawn_grace_timer <= 0.0f || !player_is_crouching));

      // Direct player alert (spotted in vision, illuminated by light, or tactile contact) -> PURSUIT
      if (sight_awakens) {
        if (can_engage_new_chaser()) {
          s.state = StalkerState::Stalking; // AIState::PURSUIT
        } else {
          s.state = StalkerState::Investigating;
          s.investigation_timer = 4.0f;
        }
        s.state_timer = 0.0f;
        s.lost_los_timer = 0.0f;
        s.target_pos = player_pos;
        if (m_global_hiss_cooldown <= 0.0f) {
          s.just_spotted_player = true;
          m_global_hiss_cooldown = 3.0f;
        }
        result.any_spotted = true;
        if (!result.any_screech && (s.screech_timer <= 0.0f || can_see_player)) {
          s.just_screeched = true;
          result.any_screech = true;
          s.screech_timer = 25.0f;
          m_alert_screech_cooldown = 2.0f;
          m_global_screech_cooldown = std::max(m_global_screech_cooldown, 14.0f);
        }
        SwarmManager::instance().AddAgitation(0.20f);
        // Swarm alert: alert at most 1 nearby packmate to investigate the commotion
        if (s.has_player_los) {
          int alerted_pack = 0;
          for (auto &other : m_stalkers) {
            if (&other != &s && other.state != StalkerState::Dead &&
                other.state != StalkerState::Dying && !other.is_engaged_state()) {
              if (glm::distance(other.position, s.position) <= 10.0f &&
                  has_line_of_sight(s.position, other.position, world)) {
                if (alerted_pack < 1) {
                  alerted_pack++;
                  other.target_pos = s.position;
                  other.investigation_target = s.position;
                  other.has_sound_target = true;
                  other.surface_state = StalkerSurfaceState::FLOOR;
                  other.target_surface_state = StalkerSurfaceState::FLOOR;
                  other.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
                  other.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
                  other.screech_timer = std::max(other.screech_timer, 15.0f);
                  other.state = StalkerState::Investigating;
                  other.investigation_timer = 4.0f;
                }
              }
            }
          }
        }
        s.surface_state = StalkerSurfaceState::FLOOR;
        s.target_surface_state = StalkerSurfaceState::FLOOR;
        s.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
        s.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
        VF_LOG_INFO("VoidStalker",
                    "Roosting Stalker "
                        << s.id
                        << " awakens into combat (spotted player)");
        break;
      }

      // Rule 3: When noise reaching an enemy exceeds 30 dB -> transition to
      // AIState::INVESTIGATING toward noise source
      if (sound_db > AberrantAI::THRESHOLD_INVESTIGATE_DB ||
          (s.has_sound_target && s.just_heard_sound)) {
        s.state = StalkerState::Investigating;
        s.state_timer = 0.0f;
        s.investigation_timer = 4.5f;
        s.target_pos = s.investigation_target;
        s.surface_state = StalkerSurfaceState::FLOOR;
        s.target_surface_state = StalkerSurfaceState::FLOOR;
        s.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
        s.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 to_snd = s.investigation_target - s.position;
        if (glm::length(glm::vec2(to_snd.x, to_snd.z)) > 0.1f) {
          s.yaw = std::atan2(to_snd.x, to_snd.z);
        }
        VF_LOG_INFO("VoidStalker",
                    "Roosting Stalker "
                        << s.id << " HEARD sound disturbance (" << sound_db
                        << " dB) -> Awakens into Investigating!");
        break;
      }

      // Autonomous waking from roosting dormancy into active zone prowl
      s.slumber_timer -= dt;
      if (s.slumber_timer <= 0.0f && s.spawn_grace_timer <= 0.0f) {
        s.state = StalkerState::Idle;
        s.state_timer = 0.0f;
        s.surface_state = StalkerSurfaceState::FLOOR;
        s.target_surface_state = StalkerSurfaceState::FLOOR;
        s.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
        s.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
        s.patrol_pause_timer = 1.0f;
        s.patrol_timer = 2.0f;
        VF_LOG_INFO("VoidStalker", "Roosting Stalker " << s.id
                    << " awakens autonomously into Idle zone prowl!");
        break;
      }

      break;
    }

    case StalkerState::Idle: {
      // Calculate arriving noise in dB
      float idle_sound_db = best_acoustic_db;
      if (best_sound) {
        float emitted =
            best_sound->db > 0.0f
                ? best_sound->db
                : (best_sound->type == SoundEventType::DemolitionBlast  ? 100.0f
                   : best_sound->type == SoundEventType::Gunshot        ? 75.0f
                   : best_sound->type == SoundEventType::DrillVibration ? 55.0f
                   : best_sound->type == SoundEventType::FootstepWalk
                       ? 15.0f
                       : best_sound->intensity);
        bool snd_los =
            has_line_of_sight(s.position + glm::vec3(0.0f, 0.4f, 0.0f),
                              best_sound->position, world);
        float rx = AberrantAI::calculate_received_noise_db(
            s.position, best_sound->position, emitted,
            best_sound->audible_radius, snd_los);
        idle_sound_db = std::max(idle_sound_db, rx);
      }

      // Boss Apex Guardian alert check (active vigilant defense of chamber)
      if (s.is_boss() && (can_see_player || dist_to_player < 26.0f || idle_sound_db > 20.0f || s.hp < s.max_hp)) {
        s.state = StalkerState::Stalking;
        s.state_timer = 0.0f;
        s.lost_los_timer = 0.0f;
        s.target_pos = player_pos;
        s.is_pursuing_attacker = true;
        s.just_spotted_player = true;
        s.just_boss_roared = true;
        s.just_screeched = true;
        result.any_screech = true;
        result.any_spotted = true;
        result.any_boss_roar = true;
        VF_LOG_INFO("VoidStalker", "APEX BOSS Chitin Goliath " << s.id
                    << " ENTERS ACTIVE COMBAT PURSUIT!");
        break;
      }

      // Direct visual contact -> PURSUIT with screech and swarm alert
      if (can_see_player) {
        if (can_see_player && dist_to_player < 4.2f &&
            (std::abs(player_pos.y - s.position.y) <= 1.8f || s.surface_state == StalkerSurfaceState::WALL_CLIMBING) &&
            s.role == StalkerRole::Melee) {
          s.state = StalkerState::Lunging;
          s.state_timer = 0.0f;
          s.lost_los_timer = 0.0f;
          s.just_lunged = true;
          result.any_lunge = true;
          result.any_spotted = true;
          if (m_global_hiss_cooldown <= 0.0f) {
            s.just_spotted_player = true;
            m_global_hiss_cooldown = 3.0f;
          }
          VF_LOG_INFO("VoidStalker",
                      "Melee Stalker "
                          << s.id << " AMBUSH LUNGES at close player (dist="
                          << dist_to_player << "m)!");
        } else {
          if (can_engage_new_chaser()) {
            s.state = StalkerState::Stalking;
          } else {
            s.state = StalkerState::Investigating;
            s.investigation_timer = 4.0f;
          }
          s.state_timer = 0.0f;
          s.lost_los_timer = 0.0f;
          s.target_pos = player_pos;
          if (m_global_hiss_cooldown <= 0.0f) {
            s.just_spotted_player = true;
            m_global_hiss_cooldown = 3.0f;
          }
          result.any_spotted = true;
          if (!result.any_screech && s.screech_timer <= 0.0f &&
              m_alert_screech_cooldown <= 0.0f) {
            s.just_screeched = true;
            result.any_screech = true;
            s.screech_timer = 25.0f;
            m_alert_screech_cooldown = 14.0f;
            m_global_screech_cooldown = std::max(m_global_screech_cooldown, 14.0f);
          }
          SwarmManager::instance().AddAgitation(0.15f);
          glm::vec3 to_pl = s.target_pos - s.position;
          if (glm::length(glm::vec2(to_pl.x, to_pl.z)) > 0.1f) {
            float target_yaw = std::atan2(to_pl.x, to_pl.z);
            float diff = target_yaw - s.yaw;
            while (diff > glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
            while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();
            s.yaw += std::clamp(diff, -10.0f * dt, 10.0f * dt);
          }
          VF_LOG_INFO("VoidStalker",
                      "Stalker "
                          << s.id << " enters PURSUIT with screech (dist="
                          << dist_to_player << "m, sound_db=" << idle_sound_db
                          << ")");
        }
        break;
      }

      // Rule 3: Heard a sound event > 30 dB -> Transition to Investigating
      // sound location!
      if (idle_sound_db > AberrantAI::THRESHOLD_INVESTIGATE_DB ||
          (s.has_sound_target && s.just_heard_sound)) {
        s.state = StalkerState::Investigating;
        s.state_timer = 0.0f;
        s.investigation_timer = 4.5f;
        s.target_pos = s.investigation_target;
        glm::vec3 to_snd = s.investigation_target - s.position;
        if (glm::length(glm::vec2(to_snd.x, to_snd.z)) > 0.1f) {
          float target_yaw = std::atan2(to_snd.x, to_snd.z);
          float diff = target_yaw - s.yaw;
          while (diff > glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
          while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();
          s.yaw += std::clamp(diff, -10.0f * dt, 10.0f * dt);
        }
        VF_LOG_INFO("VoidStalker",
                    "Stalker " << s.id << " HEARD sound disturbance ("
                               << idle_sound_db << " dB) -> Investigating ("
                               << s.investigation_target.x << ", "
                               << s.investigation_target.y << ", "
                               << s.investigation_target.z << ")");
        break;
      }

      // Ambient prowling & roaming within designated territory zone
      if (s.patrol_waypoints.empty()) {
        if (s.has_patrol_zone) {
          s.patrol_waypoints = AberrantAI::generate_zone_waypoints(s.patrol_zone, world, 4);
        } else {
          PatrolZone auto_zone;
          auto_zone.center = s.patrol_anchor;
          auto_zone.radius = 8.5f;
          auto_zone.has_bounds = false;
          s.patrol_waypoints = AberrantAI::generate_zone_waypoints(auto_zone, world, 4);
        }
        s.patrol_waypoint_idx = 0;
        if (!s.patrol_waypoints.empty()) {
          s.patrol_waypoint = s.patrol_waypoints[0];
        }
      }

      if (s.patrol_pause_timer > 0.0f) {
        // Paused at waypoint: actively scanning / looking around with rotating vision cone!
        s.patrol_pause_timer -= dt;
        s.velocity = glm::vec3(0.0f);

        s.patrol_scan_interval -= dt;
        if (s.patrol_scan_interval <= 0.0f) {
          // Look around: alternate sweeping angles to inspect room and corridors
          float scan_offset = ((s.patrol_waypoint_idx + static_cast<int>(s.state_timer * 2)) % 2 == 0) ? 0.95f : -0.95f;
          s.patrol_scan_yaw = s.yaw + scan_offset;
          s.patrol_scan_interval = 1.0f + static_cast<float>((s.id * 17) % 15) * 0.1f;
        }

        // Smoothly rotate yaw toward scan target, sweeping creature's vision cone!
        float yaw_diff = s.patrol_scan_yaw - s.yaw;
        while (yaw_diff > glm::pi<float>()) yaw_diff -= 2.0f * glm::pi<float>();
        while (yaw_diff < -glm::pi<float>()) yaw_diff += 2.0f * glm::pi<float>();
        s.yaw += std::clamp(yaw_diff, -3.5f * dt, 3.5f * dt);

        if (s.patrol_pause_timer <= 0.0f) {
          // Pause complete: pick next waypoint in zone!
          if (!s.patrol_waypoints.empty()) {
            s.patrol_waypoint_idx = (s.patrol_waypoint_idx + 1) % s.patrol_waypoints.size();
            s.patrol_waypoint = s.patrol_waypoints[s.patrol_waypoint_idx];
          }
          s.patrol_timer = 12.0f; // Failsafe timeout to reach waypoint
        }
      } else {
        // Moving toward patrol waypoint
        glm::vec3 to_wp = s.patrol_waypoint - s.position;
        float wp_dist = std::hypot(to_wp.x, to_wp.z);

        if (wp_dist <= 0.65f) {
          // Arrived at patrol waypoint! Enter vigilant scanning pause
          s.patrol_pause_timer = 2.0f + static_cast<float>((s.id * 23) % 25) * 0.1f;
          s.patrol_scan_yaw = s.yaw;
          s.patrol_scan_interval = 0.4f;
          s.velocity = glm::vec3(0.0f);
        } else {
          // Eerie stop-and-scan prowl: scurry in bursts, pause to sense surroundings
          glm::vec3 prowl_dir(0.0f);
          float prowl_burst =
              std::max(0.12f, 0.55f + 0.55f * std::sin(s.walk_cycle * 0.65f));

          if (s.surface_state == StalkerSurfaceState::WALL_CLIMBING) {
            prowl_dir = AberrantAI::calculate_wall_traversal_direction(
                s.position, s.patrol_waypoint, s.contact_normal);
          } else {
            glm::vec3 to_wp_xz(to_wp.x, 0.0f, to_wp.z);
            if (glm::length(to_wp_xz) > 0.05f) {
              prowl_dir = glm::normalize(to_wp_xz);
              prowl_dir = AberrantAI::steer_around_obstacles(
                  s.position, prowl_dir, s.contact_normal, world);
            }
          }

          if (glm::length(prowl_dir) > 0.01f) {
            s.velocity = prowl_dir * s.prowl_speed * prowl_burst;
            s.m_targetVelocity = s.velocity;
            s.position += s.velocity * dt;
            s.walk_cycle += dt * 3.2f;

            if (s.has_patrol_zone && !s.patrol_zone.contains(s.position)) {
              glm::vec3 to_center = s.patrol_zone.center - s.position;
              to_center.y = 0.0f;
              if (glm::length(to_center) > 0.01f) {
                s.velocity = glm::normalize(to_center) * s.prowl_speed;
              }
            }

            AberrantAI::resolve_voxel_collision(
                s.position, s.velocity, 0.45f, 0.9f,
                s.surface_state, s.contact_normal, world);

            if (s.surface_state != StalkerSurfaceState::WALL_CLIMBING &&
                glm::length(glm::vec2(s.velocity.x, s.velocity.z)) > 0.08f) {
              float target_yaw = std::atan2(s.velocity.x, s.velocity.z);
              float diff = target_yaw - s.yaw;
              while (diff > glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
              while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();
              s.yaw += std::clamp(diff, -5.0f * dt, 5.0f * dt);
            }
          }

          s.patrol_timer -= dt;
          if (s.patrol_timer <= 0.0f) {
            // Failsafe: if taking too long to reach waypoint, advance to next
            if (!s.patrol_waypoints.empty()) {
              s.patrol_waypoint_idx = (s.patrol_waypoint_idx + 1) % s.patrol_waypoints.size();
              s.patrol_waypoint = s.patrol_waypoints[s.patrol_waypoint_idx];
            }
            s.patrol_timer = 12.0f;
          }
        }
      }
      s.position.y += std::sin(s.walk_cycle * 2.2f) * 0.010f * dt;

      // Ambient chattering / mandible clicking during idle prowl
      s.chitter_timer -= dt;
      if (s.chitter_timer <= 0.0f) {
        s.chitter_timer = 4.0f + static_cast<float>((s.id * 23) % 25) * 0.1f;
        if (m_global_chitter_cooldown <= 0.0f) {
          s.just_chittered = true;
          result.any_chitter = true;
          m_global_chitter_cooldown = 2.2f;
        }
      }
      break;
    }

    case StalkerState::Investigating: {
      float inv_sound_db = best_acoustic_db;
      if (best_sound) {
        float emitted =
            best_sound->db > 0.0f
                ? best_sound->db
                : (best_sound->type == SoundEventType::DemolitionBlast  ? 100.0f
                   : best_sound->type == SoundEventType::Gunshot        ? 75.0f
                   : best_sound->type == SoundEventType::DrillVibration ? 55.0f
                   : best_sound->type == SoundEventType::FootstepWalk
                       ? 15.0f
                       : best_sound->intensity);
        bool snd_los =
            has_line_of_sight(s.position + glm::vec3(0.0f, 0.4f, 0.0f),
                              best_sound->position, world);
        float rx = AberrantAI::calculate_received_noise_db(
            s.position, best_sound->position, emitted,
            best_sound->audible_radius, snd_los);
        inv_sound_db = std::max(inv_sound_db, rx);
      }
      // If direct line-of-sight to player is established, screech and enter PURSUIT subject to chaser limits
      if (can_see_player) {
        if (can_engage_new_chaser()) {
          if (m_global_hiss_cooldown <= 0.0f) {
            s.just_spotted_player = true;
            m_global_hiss_cooldown = 3.0f;
          }
          result.any_spotted = true;
          if (!result.any_screech && s.screech_timer <= 0.0f &&
              m_alert_screech_cooldown <= 0.0f) {
            s.just_screeched = true;
            result.any_screech = true;
            s.screech_timer = 25.0f;
            m_alert_screech_cooldown = 14.0f;
            m_global_screech_cooldown = std::max(m_global_screech_cooldown, 14.0f);
          }
          SwarmManager::instance().AddAgitation(0.15f);
          s.state = StalkerState::Stalking; // AIState::PURSUIT
          s.state_timer = 0.0f;
          s.lost_los_timer = 0.0f;
          s.target_pos = player_pos;
          VF_LOG_INFO("VoidStalker",
                      "Stalker " << s.id << " SCREECHES and enters PURSUIT (spotted player)!");
        } else {
          s.target_pos = player_pos;
          s.investigation_timer = 4.0f;
        }
        break;
      } else if (inv_sound_db > AberrantAI::THRESHOLD_INVESTIGATE_DB || (s.has_sound_target && s.just_heard_sound)) {
        // Redirect investigation toward new acoustic disturbance
        s.target_pos = s.investigation_target;
        s.investigation_timer = 4.5f;
      }

      // Gained line of sight beyond 16m -> Attack/stalk
      if (can_see_player) {
        if (dist_to_player < 4.2f && s.role == StalkerRole::Melee) {
          s.state = StalkerState::Lunging;
          s.state_timer = 0.0f;
          s.lost_los_timer = 0.0f;
          s.just_lunged = true;
          result.any_lunge = true;
          VF_LOG_INFO("VoidStalker",
                      "Melee Stalker "
                          << s.id
                          << " SPOTTED player while investigating and LUNGED!");
        } else if (can_engage_new_chaser()) {
          s.state = StalkerState::Stalking;
          s.state_timer = 0.0f;
          s.lost_los_timer = 0.0f;
          s.target_pos = player_pos;
          if (m_global_hiss_cooldown <= 0.0f) {
            s.just_spotted_player = true;
            m_global_hiss_cooldown = 3.0f;
          }
          result.any_spotted = true;
          VF_LOG_INFO("VoidStalker",
                      "Stalker "
                          << s.id
                          << " SPOTTED player during sound investigation!");
        } else {
          s.target_pos = player_pos;
          s.investigation_timer = 4.0f;
        }
        break;
      }

      // Priority 2: Newer sound heard -> Redirect investigation
      if (s.just_heard_sound) {
        s.investigation_timer = 4.5f;
        s.target_pos = s.investigation_target;
        glm::vec3 to_snd_dir = s.investigation_target - s.position;
        if (glm::length(glm::vec2(to_snd_dir.x, to_snd_dir.z)) > 0.1f) {
          float target_yaw = std::atan2(to_snd_dir.x, to_snd_dir.z);
          float diff = target_yaw - s.yaw;
          while (diff > glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
          while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();
          s.yaw += std::clamp(diff, -10.0f * dt, 10.0f * dt);
        }
      }

      // Move towards sound disturbance location
      glm::vec3 to_snd = s.investigation_target - s.position;
      float snd_dist = std::hypot(to_snd.x, to_snd.z);
      float total_dist = glm::length(to_snd);
      float vert_diff = s.position.y - s.investigation_target.y;

      // If player is crouching in stealth mode, suspicion decays steadily (2.5x)
      float stealth_decay_mul = player_is_crouching ? 3.5f : 1.0f;
      if (player_is_crouching) {
        s.investigation_timer -= dt * 2.5f;
      }

      // Reached sound origin only if close in 3D (prevent stopping on ceiling directly above sound)
      bool reached_sound = (s.surface_state == StalkerSurfaceState::FLOOR)
                               ? (snd_dist <= 1.0f)
                               : (total_dist <= 1.2f && std::abs(vert_diff) <= 1.2f);

      if (!reached_sound) {
        glm::vec3 move_dir(0.0f);
        float burst = 0.60f + 0.65f * std::abs(std::sin(s.walk_cycle * 0.8f));

        StalkerSurfaceState active_surf =
            (s.surface_state == StalkerSurfaceState::TRANSITIONING)
                ? s.target_surface_state
                : s.surface_state;

        if (active_surf == StalkerSurfaceState::CEILING_CRAWLING) {
          if (vert_diff > 1.2f) {
            move_dir = AberrantAI::find_descending_wall_direction(
                s.position, s.investigation_target, world);
          } else {
            glm::vec3 to_snd_xz(to_snd.x, 0.0f, to_snd.z);
            if (glm::length(to_snd_xz) > 0.1f)
              move_dir = glm::normalize(to_snd_xz);
          }
          move_dir = AberrantAI::steer_around_obstacles(
              s.position, move_dir, s.contact_normal, world);
        } else if (active_surf == StalkerSurfaceState::WALL_CLIMBING) {
          move_dir = AberrantAI::calculate_wall_traversal_direction(
              s.position, s.investigation_target, s.contact_normal);
          move_dir = AberrantAI::steer_around_obstacles(
              s.position, move_dir, s.contact_normal, world);
        } else {
          float snd_vert_diff = s.investigation_target.y - s.position.y;
          glm::vec3 to_snd_xz(to_snd.x, 0.0f, to_snd.z);
          float horiz_to_snd = glm::length(to_snd_xz);
          if (snd_vert_diff > 1.5f && (horiz_to_snd < 5.0f || !has_line_of_sight(s.position, s.investigation_target, world))) {
            move_dir = AberrantAI::find_ascending_wall_direction(
                s.position, s.investigation_target, world);
          } else if (snd_vert_diff < -1.8f && (horiz_to_snd < 6.0f || !has_line_of_sight(s.position, s.investigation_target, world))) {
            move_dir = AberrantAI::find_descending_wall_direction(
                s.position, s.investigation_target, world);
          } else if (horiz_to_snd > 0.1f) {
            move_dir = glm::normalize(to_snd_xz);
          }
          if (glm::length(move_dir) > 0.01f) {
            move_dir = AberrantAI::steer_around_obstacles(
                s.position, move_dir, s.contact_normal, world);
          }
        }

        if (glm::length(move_dir) > 0.01f) {
          s.velocity = move_dir * effective_move_speed * 1.15f * burst;
          s.m_targetVelocity = s.velocity;
          s.position += s.velocity * dt;
          if (s.surface_state != StalkerSurfaceState::WALL_CLIMBING &&
              glm::length(glm::vec2(s.velocity.x, s.velocity.z)) > 0.1f) {
            float target_yaw = std::atan2(s.velocity.x, s.velocity.z);
            float diff = target_yaw - s.yaw;
            while (diff > glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
            while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();
            s.yaw += std::clamp(diff, -10.0f * dt, 10.0f * dt);
          }
        }
      } else {
        // Reached sound origin! Stand, twitch, tilt pitch, chitter suspiciously
        s.velocity = glm::vec3(0.0f);
        s.investigation_timer -= dt * stealth_decay_mul;

        s.chitter_timer -= dt;
        if (s.chitter_timer <= 0.0f) {
          s.chitter_timer = 2.5f + static_cast<float>((s.id * 19) % 20) * 0.1f;
          if (m_global_chitter_cooldown <= 0.0f) {
            s.just_chittered = true;
            result.any_chitter = true;
            m_global_chitter_cooldown = 2.2f;
          }
        }
      }

      // Nothing found and silence resumed / suspicion dissipated
      if (s.investigation_timer <= 0.0f) {
        s.patrol_anchor = s.position;
        s.patrol_waypoint = s.position;
        s.state = StalkerState::Idle;
        s.state_timer = 0.0f;
        s.has_sound_target = false;
        s.is_pursuing_attacker = false;
        s.velocity = glm::vec3(0.0f);
        VF_LOG_INFO("VoidStalker",
                    "Stalker " << s.id
                               << " suspicion subsided -> returned to Idle");
        break;
      }
      break;
    }

    case StalkerState::Stalking: {
      s.target_pos = can_see_player ? player_pos : s.last_seen_player_pos;

      // Behind Wall Detection & Alternate Route Determination when attacking
      bool direct_los = AberrantAI::has_line_of_sight(
          s.position + glm::vec3(0.0f, 0.45f, 0.0f),
          player_pos + glm::vec3(0.0f, 0.50f, 0.0f), world);

      if (direct_los) {
        if (s.is_wall_obstructed) {
          s.clear_alternate_route();
        }
        s.wall_stuck_timer = 0.0f;
      } else {
        // Behind a wall and cannot get directly to the player!
        s.is_wall_obstructed = true;
        s.wall_stuck_timer += dt;

        bool needs_repath = !s.has_active_alternate_route() ||
                            s.route_repath_timer <= 0.0f ||
                            glm::distance(player_pos, s.route_target_pos) > 3.0f ||
                            s.wall_stuck_timer > 1.2f;

        if (needs_repath && dist_to_player <= 35.0f) {
          s.route_repath_timer = 0.8f + static_cast<float>((s.id * 17) % 6) * 0.05f;
          s.route_target_pos = player_pos;
          s.wall_stuck_timer = 0.0f;

          AlternateRoute route = AberrantAI::find_alternate_route(
              s.position, player_pos, world, 32.0f, !s.is_boss());

          if (route.is_valid && !route.waypoints.empty()) {
            s.alternate_route = route.waypoints;
            s.route_type = route.type;
            // Skip first waypoint if already at or very close to start (< 0.8m)
            s.current_route_index = (s.alternate_route.size() > 1 &&
                                     glm::distance(glm::vec2(s.position.x, s.position.z),
                                                   glm::vec2(s.alternate_route[0].x, s.alternate_route[0].z)) < 0.8f) ? 1 : 0;
            VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " behind wall; determined alternate "
                        << (route.type == AlternateRouteType::GroundDetour ? "GROUND DETOUR" : "VERTICAL CLIMB")
                        << " route (" << route.waypoints.size() << " waypoints) to reach player!");
          }
        }
      }

      // Chitinous mandible clicks & chattering while stalking prey
      s.chitter_timer -= dt;
      if (s.chitter_timer <= 0.0f) {
        s.chitter_timer =
            (!s.is_in_light)
                ? (3.0f + static_cast<float>((s.id * 19) % 20) * 0.1f)
                : (4.2f + static_cast<float>((s.id * 19) % 25) * 0.1f);
        s.just_chittered = true;
        result.any_chitter = true;
      }

      // Lost Line-of-Sight for sustained duration without hearing fresh sound
      // In stealth crouch mode, stalkers lose track of the delver much faster
      // (2.0s vs 4.5s)
      float los_break_time = player_is_crouching ? 2.0f : 4.5f;
      if (s.has_active_alternate_route()) {
        los_break_time = 8.0f; // Give stalker sufficient time to navigate around the wall
      }
      if (!can_see_player) {
        if (s.just_heard_sound && s.has_sound_target) {
          // Reacquired through acoustic perception!
          s.last_seen_player_pos = s.investigation_target;
          s.target_pos = s.investigation_target;
          s.lost_los_timer = 0.0f;
        } else if (s.lost_los_timer > los_break_time && s.state_timer >= 1.0f) {
          s.investigation_target = s.last_seen_player_pos;
          s.investigation_timer = player_is_crouching ? 2.2f : 3.5f;
          s.target_pos = s.investigation_target;
          s.state = StalkerState::Investigating;
          s.state_timer = 0.0f;
          s.lost_los_timer = 0.0f;
          VF_LOG_INFO("VoidStalker",
                      "Stalker " << s.id
                                 << " lost line-of-sight to sneaking delver -> "
                                    "investigating last known coords");
          break;
        }
      }

      // Acoustic distraction: if a loud sound (explosion, tremor, bullet
      // impact, rock break) occurred away from player
      if (s.just_heard_sound && s.has_sound_target) {
        float dist_sound_to_player =
            glm::distance(s.investigation_target, player_pos);
        float dist_to_sound = glm::distance(s.position, s.investigation_target);
        bool is_distraction_sound =
            (best_sound &&
             (best_sound->type == SoundEventType::DemolitionBlast ||
              best_sound->type == SoundEventType::SeismicTremor ||
              best_sound->type == SoundEventType::BulletImpact ||
              best_sound->type == SoundEventType::VoxelFracture ||
              best_sound->intensity >= 16.0f));

        // If the distraction occurred away from the player (> 4.5m) and stalker
        // is not in point-blank melee lunge reach, and stalker is not already hunting the attacker:
        if (!s.is_pursuing_attacker && is_distraction_sound && dist_sound_to_player > 4.5f &&
            dist_to_sound > 2.0f && (!can_see_player || dist_to_player > 5.0f)) {
          s.state = StalkerState::Investigating;
          s.state_timer = 0.0f;
          s.investigation_timer = 5.0f;
          s.target_pos = s.investigation_target;
          VF_LOG_INFO("VoidStalker", "Stalker "
                                         << s.id << " DISTRACTED by sound at ("
                                         << s.investigation_target.x << ", "
                                         << s.investigation_target.y << ", "
                                         << s.investigation_target.z << ")!");
          break;
        }
      }

      // Close range (< 6.8m) combat reactions (only if within vertical melee reach or on climbing wall; bypassed for Boss):
      if (!s.is_boss() && dist_to_player < 6.8f && can_see_player &&
          (std::abs(player_pos.y - s.position.y) <= 1.8f || s.surface_state == StalkerSurfaceState::WALL_CLIMBING)) {
        if (s.role == StalkerRole::Melee) {
          if (AberrantAI::has_hazard_between(s.position, player_pos, world)) {
            // Fears launching into lethal hazard; circles to find safe flanking angle
            s.state = StalkerState::Circling;
            s.state_timer = 0.0f;
            break;
          }
          s.state = StalkerState::Lunging;
          s.state_timer = 0.0f;
          s.just_lunged = true;
          result.any_lunge = true;
          VF_LOG_INFO("VoidStalker",
                      "Melee Stalker "
                          << s.id << " ENRAGES and LUNGES at close player!");
          break;
        } else if (s.role == StalkerRole::Shooter) {
          // Shooter keeps distance and never lunges
          s.state = StalkerState::Circling;
          s.state_timer = 0.0f;
          break;
        }
      }

      // Lost interest if very far away (bypassed if pursuing an attacker who
      // damaged us)
      if (!s.is_pursuing_attacker &&
          dist_to_player > s.detection_range * 1.6f) {
        s.state = StalkerState::Idle;
        s.state_timer = 0.0f;
        break;
      }

      // Headlamp light: ONLY flee if at critical health (<= 12 HP)
      if (s.is_in_light) {
        if (s.hp <= s.flee_hp_threshold) {
          s.state = StalkerState::Fleeing;
          s.state_timer = 0.0f;
          VF_LOG_INFO("VoidStalker",
                      "Wounded Stalker " << s.id << " retreats from light");
          break;
        }
        // Healthy Melee: illuminated within 9m -> provokes an aggressive lunge
        // attack!
        if (dist_to_player < 9.0f && s.role == StalkerRole::Melee &&
            can_see_player) {
          if (AberrantAI::has_hazard_between(s.position, player_pos, world)) {
            s.state = StalkerState::Circling;
            s.state_timer = 0.0f;
            break;
          }
          s.state = StalkerState::Lunging;
          s.state_timer = 0.0f;
          s.just_lunged = true;
          result.any_lunge = true;
          VF_LOG_INFO("VoidStalker",
                      "Melee Stalker "
                          << s.id
                          << " PROVOKED by headlamp beam! Lunging into light!");
          break;
        }
      }

      // Ranged Attack: Launch crystalline void spine if at mid-to-long range
      // and has LOS (Shooters ONLY!)
      if (s.role == StalkerRole::Shooter && s.projectile_cooldown <= 0.0f &&
          dist_to_player > 4.5f && dist_to_player < 20.0f &&
          has_line_of_sight(s.position + glm::vec3(0, 0.4f, 0), player_pos,
                            world)) {
        glm::vec3 spawn_pt = s.position + glm::vec3(0.0f, 0.4f, 0.0f);
        float flight_time = dist_to_player / 15.0f;
        glm::vec3 predicted_pos =
            player_pos + player_vel * (flight_time * 0.75f);
        glm::vec3 aim_dir = glm::normalize(
            predicted_pos + glm::vec3(0.0f, 0.5f, 0.0f) - spawn_pt);
        // Projectile ballistics inherit shooter velocity so circling/leaping does not distort spine trajectory
        m_projectiles.push_back({m_next_proj_id++, spawn_pt + aim_dir * 0.4f,
                                 aim_dir * 15.0f + s.velocity * 0.4f, 2.5f, 10.0f, true});
        s.projectile_cooldown = 2.4f;
        s.just_fired_spine = true;
        result.any_projectile_fired = true;
        VF_LOG_INFO("VoidStalker", "Shooter Stalker "
                                       << s.id
                                       << " FIRED predictive crystalline void "
                                          "spine volley at player!");
      }

      // Recalculate full player-tracking velocity vectors only every 0.10s (10 Hz accumulator)
      bool do_path_tick = (s.m_pathTimer >= 0.10f);
      if (do_path_tick) {
        s.m_pathTimer = 0.0f;

        // Advance alternate route waypoints as they are reached
        if (s.has_active_alternate_route()) {
          glm::vec3 cur_wp = s.alternate_route[s.current_route_index];
          float dist_to_wp = glm::distance(glm::vec2(s.position.x, s.position.z),
                                           glm::vec2(cur_wp.x, cur_wp.z));
          if (dist_to_wp < 1.15f || glm::distance(s.position, cur_wp) < 1.35f) {
            s.current_route_index++;
            if (s.current_route_index >= s.alternate_route.size()) {
              s.clear_alternate_route();
            }
          }
        }

        glm::vec3 current_tgt = s.has_active_alternate_route()
                                    ? s.get_active_route_target()
                                    : s.target_pos;

        glm::vec3 move_dir(0.0f);
        float weave = 0.45f * std::sin(s.walk_cycle * 1.2f);

        StalkerSurfaceState active_surf =
            (s.surface_state == StalkerSurfaceState::TRANSITIONING)
                ? s.target_surface_state
                : s.surface_state;

        if (active_surf == StalkerSurfaceState::CEILING_CRAWLING) {
          float vert_diff = s.position.y - current_tgt.y;
          if (vert_diff > 1.2f) {
            // Stalker on ceiling: directly navigate walls to descend and reach player!
            move_dir = AberrantAI::find_descending_wall_direction(
                s.position, current_tgt, world);
          } else {
            glm::vec3 to_tgt_xz(current_tgt.x - s.position.x, 0.0f,
                                current_tgt.z - s.position.z);
            float horiz_to_tgt = glm::length(to_tgt_xz);
            if (horiz_to_tgt > 0.1f) {
              move_dir = to_tgt_xz / horiz_to_tgt;
            }
          }
          if (glm::length(move_dir) > 0.01f) {
            glm::vec3 perp(-move_dir.z, 0.0f, move_dir.x);
            float effective_weave = s.has_active_alternate_route() ? 0.0f : weave;
            move_dir = glm::normalize(move_dir + perp * effective_weave);
            if (dist_to_player <= 35.0f) {
              move_dir = AberrantAI::steer_around_obstacles(
                  s.position, move_dir, s.contact_normal, world);
            }
          }
        } else if (active_surf == StalkerSurfaceState::WALL_CLIMBING) {
          move_dir = AberrantAI::calculate_wall_traversal_direction(
              s.position, current_tgt, s.contact_normal);
          if (dist_to_player <= 35.0f) {
            move_dir = AberrantAI::steer_around_obstacles(
                s.position, move_dir, s.contact_normal, world);
          }
        } else {
          if (s.has_active_alternate_route() && s.route_type == AlternateRouteType::VerticalClimb) {
            // Player is behind wall; vertical climbing route chosen: seek wall to climb over
            move_dir = AberrantAI::find_ascending_wall_direction(
                s.position, current_tgt, world);
          } else {
            float vert_diff = current_tgt.y - s.position.y;
            glm::vec3 to_tgt_xz(current_tgt.x - s.position.x, 0.0f,
                                current_tgt.z - s.position.z);
            float horiz_to_tgt = glm::length(to_tgt_xz);
            if (vert_diff > 1.5f && (horiz_to_tgt < 5.0f || !has_line_of_sight(s.position, current_tgt, world))) {
              // Player is elevated / directly above: seek adjacent wall to climb up to them!
              move_dir = AberrantAI::find_ascending_wall_direction(
                  s.position, current_tgt, world);
            } else if (vert_diff < -1.8f && (horiz_to_tgt < 6.0f || !has_line_of_sight(s.position, current_tgt, world))) {
              // Target is below (lower floor, shaft, ledge): seek descending wall or drop path!
              move_dir = AberrantAI::find_descending_wall_direction(
                  s.position, current_tgt, world);
            } else if (horiz_to_tgt > 0.1f) {
              glm::vec3 dir_xz = to_tgt_xz / horiz_to_tgt;
              float effective_weave = s.has_active_alternate_route() ? (weave * 0.15f) : weave;
              glm::vec3 perp(-dir_xz.z, 0.0f, dir_xz.x);
              move_dir = glm::normalize(dir_xz + perp * effective_weave);
            }
          }
          if (glm::length(move_dir) > 0.01f && dist_to_player <= 35.0f) {
            move_dir = AberrantAI::steer_around_obstacles(
                s.position, move_dir, s.contact_normal, world);
          }
        }

        // Swarm Boid Separation: Spatial grid query, completely skipped if distance to player > 35.0m (dormant low-tick mode)
        glm::vec3 f_sep(0.0f);
        if (dist_to_player <= 35.0f) {
          f_sep = m_spatial_hash.calculate_separation(s.id, s.position, 16.0f, 8.0f);
        }

        if (glm::length(move_dir) > 0.01f) {
          s.m_targetVelocity = move_dir * effective_move_speed + f_sep;
        } else {
          s.m_targetVelocity = f_sep;
        }
      }

      // Interpolate movement smoothly between ticks
      float burst = 0.50f + 0.70f * std::abs(std::sin(s.walk_cycle * 0.45f));
      glm::vec3 desired_vel = s.m_targetVelocity * burst;
      if (glm::length(s.velocity) < 0.01f) {
        s.velocity = desired_vel;
      } else {
        s.velocity = glm::mix(s.velocity, desired_vel, std::clamp(dt * 15.0f, 0.0f, 1.0f));
      }

      if (glm::length(s.velocity) > 0.01f) {
        s.position += s.velocity * dt;
        if (s.has_active_alternate_route()) {
          glm::vec3 cur_wp = s.alternate_route[s.current_route_index];
          float dist_to_wp = glm::distance(glm::vec2(s.position.x, s.position.z),
                                           glm::vec2(cur_wp.x, cur_wp.z));
          if (dist_to_wp < 1.15f || glm::distance(s.position, cur_wp) < 1.35f) {
            s.current_route_index++;
            if (s.current_route_index >= s.alternate_route.size()) {
              s.clear_alternate_route();
            }
          }
        }
        if (s.surface_state != StalkerSurfaceState::WALL_CLIMBING ||
            glm::length(glm::vec2(s.velocity.x, s.velocity.z)) > 0.1f) {
          float target_yaw = std::atan2(s.velocity.x, s.velocity.z);
          float diff = target_yaw - s.yaw;
          while (diff > glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
          while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();
          s.yaw += std::clamp(diff, -12.0f * dt, 12.0f * dt);
        }
      }

      if (s.is_boss()) {
        if (!s.is_enraged && s.hp <= s.max_hp * 0.5f) {
          s.is_enraged = true;
          s.just_enraged = true;
          s.move_speed = 3.6f;
          s.just_boss_roared = true;
          result.any_boss_roar = true;
          VF_LOG_INFO("VoidStalker", "APEX BOSS Chitin Goliath ENRAGED! Speed increased to 3.6m/s!");
        }

        // Boss Special Attacks (Titan Ground Slam, Juggernaut Bull Charge, Quill Barrage, Heavy Cleave)
        if (can_see_player) {
          // 1. Titan Ground Slam: close range AOE
          if (dist_to_player <= 5.5f && s.boss_slam_cooldown <= 0.0f &&
              s.surface_state == StalkerSurfaceState::FLOOR) {
            s.state = StalkerState::GroundSlam;
            s.state_timer = 0.0f;
            s.boss_attack_windup = s.is_enraged ? 0.45f : 0.70f;
            s.boss_slam_cooldown = s.is_enraged ? 5.0f : 8.0f;
            s.velocity = glm::vec3(0.0f);
            VF_LOG_INFO("VoidStalker", "APEX BOSS " << s.id << " winds up TITAN GROUND SLAM!");
            break;
          }

          // 2. Juggernaut Bull Charge: mid-range ram
          if (dist_to_player >= 6.0f && dist_to_player <= 22.0f && s.boss_charge_cooldown <= 0.0f &&
              s.surface_state == StalkerSurfaceState::FLOOR && std::abs(player_pos.y - s.position.y) <= 2.2f) {
            s.state = StalkerState::Charging;
            s.state_timer = 0.0f;
            s.boss_attack_windup = s.is_enraged ? 0.40f : 0.60f;
            s.boss_charge_timer = 2.2f;
            s.boss_charge_cooldown = s.is_enraged ? 6.0f : 10.0f;
            glm::vec3 to_p = player_pos - s.position;
            to_p.y = 0.0f;
            s.boss_charge_dir = (glm::length(to_p) > 0.01f) ? glm::normalize(to_p) : glm::vec3(0.0f, 0.0f, 1.0f);
            s.velocity = glm::vec3(0.0f);
            VF_LOG_INFO("VoidStalker", "APEX BOSS " << s.id << " winds up JUGGERNAUT BULL CHARGE!");
            break;
          }

          // 3. Carapace Quill Barrage: elevated or ranged players
          if ((player_pos.y - s.position.y > 1.8f || dist_to_player > 8.0f) &&
              s.boss_quill_cooldown <= 0.0f && dist_to_player <= 26.0f) {
            glm::vec3 spawn_pt = s.position + glm::vec3(0.0f, 1.2f, 0.0f);
            glm::vec3 base_dir = (glm::length(player_pos + glm::vec3(0.0f, 0.5f, 0.0f) - spawn_pt) > 0.01f)
                                     ? glm::normalize(player_pos + glm::vec3(0.0f, 0.5f, 0.0f) - spawn_pt)
                                     : glm::vec3(0.0f, 0.0f, 1.0f);
            glm::vec3 side = glm::cross(base_dir, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::length(side) > 0.01f) side = glm::normalize(side);
            else side = glm::vec3(1.0f, 0.0f, 0.0f);

            // Fan of 3 quills: center, left (-12 deg), right (+12 deg)
            m_projectiles.push_back({m_next_proj_id++, spawn_pt + base_dir * 0.8f, base_dir * 18.0f, 3.0f, 14.0f, true});
            glm::vec3 left_dir = glm::normalize(base_dir - side * 0.22f);
            m_projectiles.push_back({m_next_proj_id++, spawn_pt + left_dir * 0.8f, left_dir * 18.0f, 3.0f, 14.0f, true});
            glm::vec3 right_dir = glm::normalize(base_dir + side * 0.22f);
            m_projectiles.push_back({m_next_proj_id++, spawn_pt + right_dir * 0.8f, right_dir * 18.0f, 3.0f, 14.0f, true});

            s.boss_quill_cooldown = s.is_enraged ? 3.5f : 5.5f;
            s.just_fired_spine = true;
            result.any_projectile_fired = true;
            VF_LOG_INFO("VoidStalker", "APEX BOSS " << s.id << " fires 3-SPIKE CARAPACE QUILL BARRAGE!");
          }

          // 4. Heavy Cleave: close melee strike
          if (dist_to_player <= 3.2f && s.attack_cooldown <= 0.0f &&
              std::abs(player_pos.y - s.position.y) <= 2.2f) {
            result.total_damage += s.cleave_damage;
            s.just_hit_player = true;
            result.any_melee_hit = true;
            s.attack_cooldown = s.is_enraged ? 0.9f : 1.4f;
            s.slash_fx_timer = 0.5f;
            VF_LOG_INFO("VoidStalker", "APEX BOSS " << s.id << " CLEAVES player for " << s.cleave_damage << " damage!");
          }
        }
      }

      // Transition to circling at mid-range (6.8m to 12.0m) if in open sight
      // (only when on floor and player is not elevated above; NEVER for Boss)
      if (!s.is_boss() && s.surface_state == StalkerSurfaceState::FLOOR && can_see_player &&
          dist_to_player <= 12.0f && dist_to_player >= 6.8f && (player_pos.y - s.position.y <= 1.5f)) {
        s.state = StalkerState::Circling;
        s.state_timer = 0.0f;
      }

      // Immediate lunge if within lunge range (Melee ONLY, and within vertical melee reach!)
      if (can_see_player && s.role == StalkerRole::Melee &&
          dist_to_player <= s.lunge_range &&
          (std::abs(player_pos.y - s.position.y) <= 1.8f || s.surface_state == StalkerSurfaceState::WALL_CLIMBING)) {
        if (AberrantAI::has_hazard_between(s.position, player_pos, world)) {
          s.state = StalkerState::Circling;
          s.state_timer = 0.0f;
        } else {
          s.state = StalkerState::Lunging;
          s.state_timer = 0.0f;
          s.just_lunged = true;
          result.any_lunge = true;
          VF_LOG_INFO("VoidStalker",
                      "Melee Stalker " << s.id << " LUNGING at close player!");
        }
      }
      break;
    }

    case StalkerState::Circling: {
      if (s.is_boss()) {
        s.state = StalkerState::Stalking;
        s.state_timer = 0.0f;
        break;
      }

      // If player approaches within 5.8m (and within vertical melee reach):
      if (dist_to_player < 5.8f && can_see_player &&
          (std::abs(player_pos.y - s.position.y) <= 1.8f || s.surface_state == StalkerSurfaceState::WALL_CLIMBING)) {
        if (s.role == StalkerRole::Melee) {
          if (!AberrantAI::has_hazard_between(s.position, player_pos, world)) {
            s.state = StalkerState::Lunging;
            s.state_timer = 0.0f;
            s.just_lunged = true;
            result.any_lunge = true;
            VF_LOG_INFO("VoidStalker",
                        "Melee Stalker "
                            << s.id
                            << " springs aggressive ambush from circling orbit!");
            break;
          }
        } else {
          if (dist_to_player < 4.0f) {
            s.state = StalkerState::Fleeing;
            s.state_timer = 0.0f;
            break;
          }
        }
      }

      // Only flee if low HP
      if (s.is_in_light && s.hp <= s.flee_hp_threshold) {
        s.state = StalkerState::Fleeing;
        s.state_timer = 0.0f;
        break;
      }

      // Acoustic distraction while circling: loud explosion, tremor, or bullet
      // impact lures stalker away
      if (s.just_heard_sound && s.has_sound_target) {
        float dist_sound_to_player =
            glm::distance(s.investigation_target, player_pos);
        float dist_to_sound = glm::distance(s.position, s.investigation_target);
        bool is_distraction_sound =
            (best_sound &&
             (best_sound->type == SoundEventType::DemolitionBlast ||
              best_sound->type == SoundEventType::SeismicTremor ||
              best_sound->type == SoundEventType::BulletImpact ||
              best_sound->type == SoundEventType::VoxelFracture ||
              best_sound->intensity >= 16.0f));

        if (!s.is_pursuing_attacker && is_distraction_sound && dist_sound_to_player > 4.5f &&
            dist_to_sound > 2.0f && (!can_see_player || dist_to_player > 5.0f)) {
          s.state = StalkerState::Investigating;
          s.state_timer = 0.0f;
          s.investigation_timer = 5.0f;
          s.target_pos = s.investigation_target;
          VF_LOG_INFO("VoidStalker", "Circling Stalker "
                                         << s.id << " DISTRACTED by sound at ("
                                         << s.investigation_target.x << ", "
                                         << s.investigation_target.y << ", "
                                         << s.investigation_target.z << ")!");
          break;
        }
      }

      // Ranged Attack while circling (Shooters ONLY):
      if (s.role == StalkerRole::Shooter && s.projectile_cooldown <= 0.0f &&
          dist_to_player > 4.0f && dist_to_player < 14.0f &&
          has_line_of_sight(s.position + glm::vec3(0, 0.4f, 0), player_pos,
                            world)) {
        glm::vec3 spawn_pt = s.position + glm::vec3(0.0f, 0.4f, 0.0f);
        float flight_time = dist_to_player / 15.0f;
        glm::vec3 predicted_pos =
            player_pos + player_vel * (flight_time * 0.75f);
        glm::vec3 aim_dir = glm::normalize(
            predicted_pos + glm::vec3(0.0f, 0.5f, 0.0f) - spawn_pt);
        // Projectile ballistics inherit shooter velocity so circling/flanking does not distort spine trajectory
        m_projectiles.push_back({m_next_proj_id++, spawn_pt + aim_dir * 0.4f,
                                 aim_dir * 15.0f + s.velocity * 0.4f, 2.5f, 10.0f, true});
        s.projectile_cooldown = 2.2f;
        s.just_fired_spine = true;
        result.any_projectile_fired = true;
        VF_LOG_INFO(
            "VoidStalker",
            "Shooter Stalker "
                << s.id
                << " FIRED predictive void spine from flanking circle!");
      }

      // If line of sight lost for > 3.0s, break circling and search
      if (!can_see_player && s.lost_los_timer > 3.0f) {
        s.state = StalkerState::Investigating;
        s.investigation_target = s.last_seen_player_pos;
        s.investigation_timer = 3.5f;
        s.state_timer = 0.0f;
        break;
      }

      bool do_circle_tick = (s.m_pathTimer >= 0.10f);
      if (do_circle_tick) {
        s.m_pathTimer = 0.0f;
        glm::vec3 circle_dir(0.0f);
        if (s.surface_state == StalkerSurfaceState::WALL_CLIMBING) {
          circle_dir = AberrantAI::calculate_wall_traversal_direction(
              s.position, player_pos, s.contact_normal);
        } else if (s.surface_state == StalkerSurfaceState::CEILING_CRAWLING) {
          circle_dir = AberrantAI::find_descending_wall_direction(
              s.position, player_pos, world);
        } else {
          float vert_diff = player_pos.y - s.position.y;
          float horiz_to_player = glm::distance(glm::vec2(s.position.x, s.position.z),
                                                glm::vec2(player_pos.x, player_pos.z));
          if (vert_diff > 1.8f && horiz_to_player < 5.0f) {
            circle_dir = AberrantAI::find_ascending_wall_direction(
                s.position, player_pos, world);
          } else if (vert_diff < -1.8f && horiz_to_player < 5.0f) {
            circle_dir = AberrantAI::find_descending_wall_direction(
                s.position, player_pos, world);
          } else {
            // Eerie variable-radius orbit with height undulation (floor pattern)
            float orbit_radius = 5.0f + std::sin(s.glow_phase * 0.65f) * 1.8f;
            float orbit_angle = s.glow_phase * 0.85f;
            glm::vec3 orbit_offset(std::cos(orbit_angle) * orbit_radius,
                                   std::sin(s.glow_phase * 1.1f) * 0.75f,
                                   std::sin(orbit_angle) * orbit_radius);
            glm::vec3 target = player_pos + orbit_offset;
            circle_dir = glm::normalize(target - s.position);
          }
        }
        glm::vec3 f_sep(0.0f);
        if (dist_to_player <= 35.0f) {
          f_sep = m_spatial_hash.calculate_separation(s.id, s.position, 16.0f, 8.0f);
        }
        s.m_targetVelocity = circle_dir * s.circle_speed + f_sep;
      }

      float orbit_burst =
          0.65f + 0.50f * std::abs(std::sin(s.walk_cycle * 0.40f));
      glm::vec3 desired_circle_vel = s.m_targetVelocity * orbit_burst;
      if (glm::length(s.velocity) < 0.01f) {
        s.velocity = desired_circle_vel;
      } else {
        s.velocity = glm::mix(s.velocity, desired_circle_vel, std::clamp(dt * 15.0f, 0.0f, 1.0f));
      }
      s.position += s.velocity * dt;
      s.yaw = std::atan2((player_pos.x - s.position.x),
                         (player_pos.z - s.position.z));

      // Agitated mandible chattering while circling
      s.chitter_timer -= dt;
      if (s.chitter_timer <= 0.0f) {
        s.chitter_timer = 2.0f + static_cast<float>((s.id * 13) % 20) * 0.1f;
        s.just_chittered = true;
        result.any_chitter = true;
      }

      // Lunge conditions: player drilling, or circled for 1.8s (Melee ONLY!)
      bool should_lunge =
          (s.role == StalkerRole::Melee) &&
          ((player_is_drilling && dist_to_player < s.lunge_range * 1.5f) ||
           (s.state_timer > 1.8f && dist_to_player < s.lunge_range * 1.3f));

      if (should_lunge && can_see_player) {
        s.state = StalkerState::Lunging;
        s.state_timer = 0.0f;
        s.just_lunged = true;
        result.any_lunge = true;
        VF_LOG_INFO("VoidStalker",
                    "Melee Stalker "
                        << s.id << " committed to LUNGE from flanking circle!");
      }

      if (dist_to_player > 13.0f) {
        s.state = StalkerState::Stalking;
        s.state_timer = 0.0f;
      }
      break;
    }

    case StalkerState::Lunging: {
      if (s.role != StalkerRole::Melee) {
        s.state = StalkerState::Circling;
        break;
      }
      s.slash_fx_timer = 0.5f;

      // Project destination to point along approach vector offset by
      // MIN_STRIKE_DISTANCE
      glm::vec3 to_player = player_pos - s.position;
      float target_dist = glm::length(to_player);
      glm::vec3 dir = (target_dist > 0.05f) ? (to_player / target_dist)
                                            : glm::vec3(0.0f, 0.0f, 1.0f);

      glm::vec3 lunge_target = AberrantAI::calculate_lunge_target(
          s.position, player_pos, AberrantAI::MIN_STRIKE_DISTANCE);
      glm::vec3 to_lunge = lunge_target - s.position;
      float dist_to_target = glm::length(to_lunge);

      float lunge_ramp = (s.state_timer < 0.12f)
                             ? std::max(0.40f, (s.state_timer / 0.12f) * (s.state_timer / 0.12f))
                             : 1.0f;

      // Once the enemy reaches within MIN_STRIKE_DISTANCE of the player:
      // Cancel forward lunge momentum smoothly at stand-off boundary
      if (dist_to_player <= AberrantAI::MIN_STRIKE_DISTANCE ||
          dist_to_target < 0.15f) {
        AberrantAI::cancel_forward_momentum(s.velocity, dir);
      } else {
        float step_travel = s.lunge_speed * lunge_ramp * dt;
        float remaining_to_standoff = dist_to_player - AberrantAI::MIN_STRIKE_DISTANCE;
        if (step_travel >= remaining_to_standoff) {
          s.position += dir * std::max(0.0f, remaining_to_standoff);
          AberrantAI::cancel_forward_momentum(s.velocity, dir);
        } else {
          s.velocity = dir * s.lunge_speed * lunge_ramp;
          s.position += s.velocity * dt;
        }
      }
      s.yaw = std::atan2(dir.x, dir.z);

      // Strike registration: Trigger claw/bite damage tick against player
      // within MELEE_STRIKE_REACH
      if (dist_to_player <= AberrantAI::MELEE_STRIKE_REACH &&
          s.attack_cooldown <= 0.0f) {
        result.total_damage += s.damage;
        s.just_hit_player = true;
        result.any_melee_hit = true;
        s.attack_cooldown = 1.0f;
        VF_LOG_INFO("VoidStalker",
                    "Stalker " << s.id << " SLASHED player at stand-off reach ("
                               << dist_to_player << "m) (-" << s.damage
                               << " HP)!");

        if (s.hp <= s.flee_hp_threshold) {
          s.state = StalkerState::Fleeing;
          s.state_timer = 0.0f;
        } else {
          s.state = StalkerState::Circling;
          s.state_timer = 0.0f;
        }
      }

      // Soft repulsion force to prevent phasing through player capsule
      if (dist_to_player < AberrantAI::MIN_STRIKE_DISTANCE) {
        float repulsion_factor =
            1.0f - (dist_to_player / AberrantAI::MIN_STRIKE_DISTANCE);
        s.position -=
            dir * AberrantAI::ENEMY_PLAYER_REPULSION * dt * repulsion_factor;
      }

      // Lunge timeout — overshot or completed
      if (s.state_timer > 1.2f) {
        s.state = StalkerState::Circling;
        s.state_timer = 0.0f;
      }
      break;
    }

    case StalkerState::GroundSlam: {
      s.velocity = glm::vec3(0.0f);
      // Face player yaw smoothly during windup
      glm::vec3 to_p = player_pos - s.position;
      if (glm::length(glm::vec2(to_p.x, to_p.z)) > 0.05f) {
        float target_yaw = std::atan2(to_p.x, to_p.z);
        float diff = target_yaw - s.yaw;
        while (diff > glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
        while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();
        s.yaw += std::clamp(diff, -8.0f * dt, 8.0f * dt);
      }

      // Windup telegraph period (rearing up heavy spiked claws)
      if (s.state_timer < s.boss_attack_windup) {
        break;
      }

      // Slam impact moment
      if (!s.just_slammed && s.state_timer >= s.boss_attack_windup &&
          (s.state_timer - dt) < s.boss_attack_windup) {
        s.just_slammed = true;
        result.any_boss_slam = true;
        result.boss_slam_pos = s.position;
        s.slash_fx_timer = 0.6f;

        // Concussive shockwave AOE radius (6.5m)
        if (dist_to_player <= 6.5f) {
          float falloff = std::max(0.35f, 1.0f - (dist_to_player / 6.5f));
          float dealt = s.slam_damage * falloff;
          result.total_damage += dealt;
          s.just_hit_player = true;
          result.any_melee_hit = true;
          VF_LOG_INFO("VoidStalker",
                      "TITAN GROUND SLAM concussive shockwave hit player for "
                          << dealt << " damage!");
        }
      }

      // Recovery window (0.45s after slam before resuming chase)
      if (s.state_timer >= s.boss_attack_windup + 0.45f) {
        s.state = StalkerState::Stalking;
        s.state_timer = 0.0f;
      }
      break;
    }

    case StalkerState::Charging: {
      // Windup telegraph (scraping carapace claws / aiming rush vector)
      if (s.state_timer < s.boss_attack_windup) {
        s.velocity = glm::vec3(0.0f);
        glm::vec3 to_p = player_pos - s.position;
        if (glm::length(glm::vec2(to_p.x, to_p.z)) > 0.05f) {
          float target_yaw = std::atan2(to_p.x, to_p.z);
          s.yaw = target_yaw;
          s.boss_charge_dir = glm::normalize(glm::vec3(to_p.x, 0.0f, to_p.z));
        }
        break;
      }

      // Charge initiated
      if (!s.just_charged) {
        s.just_charged = true;
        result.any_boss_charge = true;
      }

      float charge_speed = s.is_enraged ? 13.5f : 10.5f;
      s.velocity = s.boss_charge_dir * charge_speed;
      s.position += s.velocity * dt;
      s.yaw = std::atan2(s.boss_charge_dir.x, s.boss_charge_dir.z);

      // 1. Direct ram impact with player
      if (dist_to_player <= 2.2f && std::abs(player_pos.y - s.position.y) <= 2.0f) {
        result.total_damage += s.charge_damage;
        s.just_hit_player = true;
        result.any_melee_hit = true;
        s.state = StalkerState::Stalking;
        s.state_timer = 0.0f;
        s.boss_charge_cooldown = s.is_enraged ? 5.0f : 8.0f;
        VF_LOG_INFO("VoidStalker", "JUGGERNAUT BULL CHARGE RAMMED player for "
                                       << s.charge_damage << " damage!");
        break;
      }

      // 2. Wall collision check: probe ahead in charge direction across width and height for solid blocks
      bool hit_solid_wall = false;
      glm::vec3 charge_side(-s.boss_charge_dir.z, 0.0f, s.boss_charge_dir.x);
      for (float probe_d = 0.4f; probe_d <= 1.6f; probe_d += 0.4f) {
        for (float lateral : {0.0f, -0.55f, 0.55f}) {
          for (float h_off : {0.2f, 0.8f}) {
            glm::vec3 probe_pt = s.position + s.boss_charge_dir * probe_d + charge_side * lateral + glm::vec3(0.0f, h_off, 0.0f);
            glm::ivec3 block_coord(std::floor(probe_pt.x), std::floor(probe_pt.y),
                                   std::floor(probe_pt.z));
            if (world.is_solid(block_coord)) {
              hit_solid_wall = true;
              break;
            }
          }
          if (hit_solid_wall) break;
        }
        if (hit_solid_wall) break;
      }

      if (hit_solid_wall) {
        // Slammed into solid cavern rock/bulkhead! Stun boss & breach front armor!
        s.state = StalkerState::Stunned;
        s.stun_timer = 2.4f;
        s.state_timer = 0.0f;
        s.velocity = -s.boss_charge_dir * 2.0f;
        s.boss_charge_cooldown = s.is_enraged ? 6.0f : 9.0f;
        result.any_boss_slam = true;
        result.boss_slam_pos = s.position;
        VF_LOG_INFO("VoidStalker",
                    "JUGGERNAUT BULL CHARGE crashed into wall! Boss STUNNED for "
                    "2.4s (front armor breached)!");
        break;
      }

      // 3. Charge timeout
      if (s.state_timer >= s.boss_attack_windup + s.boss_charge_timer) {
        s.state = StalkerState::Stalking;
        s.state_timer = 0.0f;
      }
      break;
    }

    case StalkerState::Stunned: {
      s.stun_timer -= dt;
      s.velocity = glm::vec3(0.0f);
      if (s.stun_timer <= 0.0f) {
        if (s.is_boss()) {
          s.state = StalkerState::Stalking;
          s.state_timer = 0.0f;
        } else {
          s.state = StalkerState::Fleeing;
          s.state_timer = 0.0f;
        }
      }
      break;
    }

    case StalkerState::Fleeing: {
      if (s.is_boss()) {
        s.state = StalkerState::Stalking;
        s.state_timer = 0.0f;
        break;
      }
      glm::vec3 flee_dir = glm::normalize(s.position - player_pos);
      flee_dir.x += std::sin(s.glow_phase * 3.0f) * 0.3f;
      flee_dir.z += std::cos(s.glow_phase * 3.0f) * 0.3f;
      if (glm::length(flee_dir) > 0.1f) {
        flee_dir = glm::normalize(flee_dir);
        flee_dir = AberrantAI::steer_around_obstacles(s.position, flee_dir, s.contact_normal, world);
      }
      s.velocity = flee_dir * effective_move_speed * 1.5f;
      s.position += s.velocity * dt;
      s.yaw = std::atan2(flee_dir.x, flee_dir.z);

      // Chattering noises during frantic escape flight
      s.chitter_timer -= dt;
      if (s.chitter_timer <= 0.0f) {
        s.chitter_timer = 1.0f + static_cast<float>((s.id * 11) % 15) * 0.08f;
        s.just_chittered = true;
        result.any_chitter = true;
      }

      // Check if fleeing stalker reaches a cavern wall or rock face to burrow
      // and escape
      glm::vec3 direct_flee = (glm::length(s.position - player_pos) > 0.01f)
                                  ? glm::normalize(s.position - player_pos)
                                  : flee_dir;

      bool hitting_wall = false;
      glm::vec3 test_burrow_dir = flee_dir;

      // Probe forward along flee direction and direct fleeing vector for solid
      // voxels
      for (float d = 0.4f; d <= 1.5f; d += 0.3f) {
        glm::vec3 probe_flee = s.position + flee_dir * d;
        glm::vec3 probe_direct = s.position + direct_flee * d;
        if (world.is_solid(glm::ivec3(std::floor(probe_flee.x),
                                      std::floor(probe_flee.y),
                                      std::floor(probe_flee.z)))) {
          hitting_wall = true;
          test_burrow_dir = flee_dir;
          break;
        }
        if (world.is_solid(glm::ivec3(std::floor(probe_direct.x),
                                      std::floor(probe_direct.y),
                                      std::floor(probe_direct.z)))) {
          hitting_wall = true;
          test_burrow_dir = direct_flee;
          break;
        }
      }

      bool hitting_boundary = (s.position.x <= 5.5f || s.position.x >= 66.5f ||
                               s.position.z <= 5.5f || s.position.z >= 66.5f);
      bool wall_climbing =
          (s.surface_state == StalkerSurfaceState::WALL_CLIMBING);

      if (s.hp <= s.flee_hp_threshold &&
          (hitting_wall || hitting_boundary ||
           (wall_climbing && s.state_timer > 0.4f) || s.state_timer > 4.5f)) {
        s.state = StalkerState::Burrowing;
        s.state_timer = 0.0f;
        s.burrow_duration = 2.0f;
        s.burrow_entry = s.position;
        s.burrow_dir = hitting_wall
                           ? test_burrow_dir
                           : (wall_climbing ? -s.contact_normal : flee_dir);
        if (glm::length(s.burrow_dir) > 0.01f) {
          s.burrow_dir = glm::normalize(s.burrow_dir);
        }
        s.drill_timer = 0.0f; // Immediate first drilling sound
        s.chitter_timer =
            0.25f; // Rapid agitated chattering when digging into rock
        s.velocity = glm::vec3(0.0f);
        VF_LOG_INFO("VoidStalker",
                    "Stalker " << s.id
                               << " reached wall/rock -> BURROWING to escape!");
        break;
      }

      if (s.state_timer > 3.0f && s.hp > s.flee_hp_threshold) {
        s.state = StalkerState::Stalking;
        s.state_timer = 0.0f;
      }
      break;
    }

    case StalkerState::Burrowing: {
      if (glm::length(s.burrow_dir) > 0.01f) {
        s.yaw = std::atan2(s.burrow_dir.x, s.burrow_dir.z);
      }

      // Slowly tunnel forward into the rock wall
      s.velocity = s.burrow_dir * 1.25f;
      s.position += s.velocity * dt;

      // 1. Drilling and digging noises indicating wall penetration
      s.drill_timer -= dt;
      if (s.drill_timer <= 0.0f) {
        s.drill_timer = 0.38f + static_cast<float>((s.id * 7) % 10) * 0.02f;
        s.just_dug = true;
        result.any_digging = true;
        result.digging_positions.push_back(s.position);
      }

      // 2. Chattering noises during burrowing escape
      s.chitter_timer -= dt;
      if (s.chitter_timer <= 0.0f) {
        s.chitter_timer = 0.70f + static_cast<float>((s.id * 13) % 10) * 0.05f;
        s.just_chittered = true;
        result.any_chitter = true;
      }

      // 3. Complete escape once fully burrowed into the wall
      if (s.state_timer >= s.burrow_duration) {
        s.state = StalkerState::Dead;
        s.just_escaped = true;
        result.stalkers_escaped++;
        VF_LOG_INFO(
            "VoidStalker",
            "Stalker "
                << s.id
                << " successfully burrowed through cavern wall and escaped!");
      }
      break;
    }

    case StalkerState::Dead:
      break;
    }

    // Hard Collision Separation Solver against player capsule
    if (s.state != StalkerState::Burrowing) {
      AberrantAI::resolve_player_penetration(s.position, player_pos, 0.5f,
                                             0.35f);
    }

    // Clamp velocity to maximum speed to prevent physics launches or tunneling
    float max_speed = (s.role == StalkerRole::ChitinGoliath) ? 14.0f : 16.0f;
    if (glm::length(s.velocity) > max_speed) {
      s.velocity = glm::normalize(s.velocity) * max_speed;
    }

    // Clamp position to sector bounds
    s.position.x = std::clamp(s.position.x, 4.5f, 67.5f);
    s.position.y = std::clamp(s.position.y, 0.5f, 31.0f);
    s.position.z = std::clamp(s.position.z, 4.5f, 67.5f);

    // ─── Kinematic Voxel Navigation, Step-Clambering & Wall Sliding ───
    if (s.state != StalkerState::Burrowing && s.state != StalkerState::Dead &&
        s.state != StalkerState::Dying) {
      // Net displacement tracking across time intervals (eliminates high-frequency collision ping-pong)
      s.stuck_checkpoint_timer += dt;
      if (s.stuck_checkpoint_timer >= 0.35f) {
        float net_disp = glm::distance(s.position, s.stuck_checkpoint_pos);
        float move_speed = glm::length(s.velocity);
        float desired_speed = glm::length(s.m_targetVelocity);

        bool has_intent = (move_speed > 0.25f || desired_speed > 0.25f);
        if (s.state == StalkerState::Stalking || s.state == StalkerState::Investigating ||
            s.state == StalkerState::Circling || s.state == StalkerState::Fleeing ||
            (s.state == StalkerState::Idle && s.patrol_pause_timer <= 0.0f)) {
          has_intent = true;
        }

        // If entity has active movement intent, but net displacement over 0.35s is under 0.18m
        if (has_intent && net_disp < 0.18f) {
          s.stuck_timer += s.stuck_checkpoint_timer;
        } else {
          s.stuck_timer = std::max(0.0f, s.stuck_timer - s.stuck_checkpoint_timer * 1.5f);
        }
        s.stuck_checkpoint_pos = s.position;
        s.stuck_checkpoint_timer = 0.0f;
      }

      // Anti-stuck resolution:
      if (s.stuck_timer > 0.30f) {
        if (s.state == StalkerState::Fleeing || s.hp <= s.flee_hp_threshold) {
          // Cornered against wall/rock -> immediately burrow into the rock to escape!
          s.state = StalkerState::Burrowing;
          s.state_timer = 0.0f;
          s.burrow_duration = 2.0f;
          s.burrow_entry = s.position;
          s.burrow_dir = -s.contact_normal;
          if (glm::length(s.burrow_dir) < 0.01f)
            s.burrow_dir = glm::vec3(0.0f, 0.0f, 1.0f);
          s.drill_timer = 0.0f;
          s.chitter_timer = 0.25f;
          s.velocity = glm::vec3(0.0f);
          s.stuck_timer = 0.0f;
          VF_LOG_INFO("VoidStalker",
                      "Stalker " << s.id
                                 << " cornered against rock face -> DECISIVELY "
                                    "BURROWS into rock to escape!");
        } else if (s.stuck_timer > 1.2f && !s.is_pursuing_attacker &&
                   (s.state == StalkerState::Investigating || s.state == StalkerState::Circling)) {
          // If stuck while investigating sound or circling (not hunting attacker), de-escalate back to Idle!
          s.state = StalkerState::Idle;
          s.state_timer = 0.0f;
          s.has_sound_target = false;
          s.stuck_timer = 0.0f;
          s.velocity = glm::vec3(0.0f);
          s.m_targetVelocity = glm::vec3(0.0f);
          VF_LOG_INFO("VoidStalker",
                      "Stalker " << s.id << " trapped while investigating -> DE-ESCALATING to Idle");
        } else if (s.stuck_timer > 1.6f && s.state == StalkerState::Stalking) {
          // Obstacle diversion: deflect laterally around obstacles
          glm::vec2 fwd_v(std::sin(s.yaw), std::cos(s.yaw));
          glm::vec2 perp(-fwd_v.y, fwd_v.x);
          float sign = ((s.id % 2) == 0) ? 1.0f : -1.0f;
          glm::vec3 deflect(perp.x * sign, 0.0f, perp.y * sign);
          s.velocity += deflect * 4.0f;
          s.m_targetVelocity += deflect * 4.0f;
          // Only hop up if target is at or above our elevation and headroom is clear
          if (s.target_pos.y >= s.position.y - 0.5f) {
            float col_h = (s.role == StalkerRole::ChitinGoliath) ? 1.4f : 0.85f;
            int px = static_cast<int>(std::floor(s.position.x));
            int py_head = static_cast<int>(std::floor(s.position.y + col_h + 0.35f));
            int pz = static_cast<int>(std::floor(s.position.z));
            if (py_head < 32 && !world.is_solid(glm::ivec3(px, py_head, pz))) {
              s.velocity.y = std::max(s.velocity.y, 3.8f); // Hop up smoothly over obstacle
            }
          }
          s.stuck_timer = 0.0f;
          VF_LOG_INFO("VoidStalker",
                      "Stalker " << s.id << " obstructed by rock -> DEFLECTS laterally around obstacle");
        } else if (s.state == StalkerState::Idle && s.stuck_timer > 0.45f) {
          // Idle patrol auto-advance: skip to next waypoint when blocked
          if (!s.patrol_waypoints.empty()) {
            s.patrol_waypoint_idx = (s.patrol_waypoint_idx + 1) % s.patrol_waypoints.size();
            s.patrol_waypoint = s.patrol_waypoints[s.patrol_waypoint_idx];
          }
          s.patrol_timer = 12.0f;
          s.stuck_timer = 0.0f;
          VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " blocked during Idle prowl -> ADVANCED to next waypoint");
        } else {
          // Responsive clambering & lateral steering for earlier stuck thresholds (0.3s - 1.2s)
          // 1. Crowd congestion check: if friendly monsters nearby are causing pressure, lane-shift laterally
          bool crowd_congested = false;
          glm::vec3 crowd_flow(0.0f);
          if (dist_to_player <= 35.0f) {
            crowd_flow = m_spatial_hash.calculate_separation(s.id, s.position, 2.25f, 10.0f);
            crowd_congested = (glm::length(crowd_flow) > 0.5f);
          }

          bool target_is_below = (s.target_pos.y < s.position.y - 0.8f);

          if (crowd_congested) {
            // Crowd congestion: DO NOT hop up! Lane-shift laterally or flow along separation vector
            glm::vec2 fwd_v(std::sin(s.yaw), std::cos(s.yaw));
            glm::vec2 perp(-fwd_v.y, fwd_v.x);
            float sign = ((s.id % 2) == 0) ? 1.0f : -1.0f;
            glm::vec3 lane_shift(perp.x * sign, 0.0f, perp.y * sign);
            s.m_targetVelocity += (lane_shift * 3.5f) + crowd_flow * 0.5f;
            s.velocity += (lane_shift * 3.0f);
            s.stuck_timer = std::max(0.0f, s.stuck_timer - 0.20f);
          } else if (target_is_below) {
            // Descending down stairs/slope/cliff: DO NOT hop up! Push downhill and step down cleanly
            glm::vec3 fwd_dir(std::sin(s.yaw), 0.0f, std::cos(s.yaw));
            s.velocity.x += fwd_dir.x * 2.5f;
            s.velocity.z += fwd_dir.z * 2.5f;
            s.m_targetVelocity += fwd_dir * 2.5f;
            float step_down_y = 0.0f;
            if (AberrantAI::find_ground_surface(world, s.position + fwd_dir * 0.6f, 0.5f, 2.5f, &step_down_y)) {
              if (step_down_y < s.position.y) {
                s.position.y = step_down_y;
                s.velocity.y = 0.0f;
              }
            }
            s.stuck_timer = 0.0f;
          } else {
            // Facing an upward obstacle on floor
            if (s.surface_state == StalkerSurfaceState::FLOOR || s.surface_state == StalkerSurfaceState::TRANSITIONING) {
              int px = static_cast<int>(std::floor(s.position.x));
              int py_head = static_cast<int>(std::floor(s.position.y + 1.6f));
              int pz = static_cast<int>(std::floor(s.position.z));
              if (py_head < 32 && !world.is_solid(glm::ivec3(px, py_head, pz))) {
                s.velocity.y = std::max(s.velocity.y, 3.8f); // Immediate clamber hop
                glm::vec3 fwd_dir(std::sin(s.yaw), 0.0f, std::cos(s.yaw));
                s.velocity += fwd_dir * 2.2f;
                s.m_targetVelocity += fwd_dir * 2.2f;
              }
            }

            // Wall climbing escalation if wedged on floor for > 0.85s
            if (s.stuck_timer > 0.85f && s.surface_state == StalkerSurfaceState::FLOOR) {
              glm::vec3 wall_dir = AberrantAI::find_ascending_wall_direction(s.position, s.target_pos, world);
              if (glm::length(wall_dir) > 0.1f) {
                s.m_targetVelocity += wall_dir * 3.5f;
                s.velocity += wall_dir * 3.0f;
              }
            } else {
              // Deflect laterally around obstacle to slide around corners
              glm::vec2 fwd_v(std::sin(s.yaw), std::cos(s.yaw));
              glm::vec2 perp(-fwd_v.y, fwd_v.x);
              float sign = ((s.id % 2) == 0) ? 1.0f : -1.0f;
              glm::vec3 deflect(perp.x * sign, 0.0f, perp.y * sign);
              s.m_targetVelocity += deflect * 3.0f;
              s.velocity += deflect * 2.5f;
            }
          }
        }
      }

      // Surface-aware gravity & attachment:
      // Downward gravity applies snappy ground adhesion on floors, stairs, slopes, and cliffs
      if ((s.surface_state == StalkerSurfaceState::FLOOR || s.surface_state == StalkerSurfaceState::TRANSITIONING) &&
          s.state != StalkerState::Burrowing && s.state != StalkerState::Roosting) {
        int fx = static_cast<int>(std::floor(s.position.x));
        int fy = static_cast<int>(std::floor(s.position.y));
        int fz = static_cast<int>(std::floor(s.position.z));
        int uy = static_cast<int>(std::floor(s.position.y - 0.15f));
        Voxel vf = world.get_voxel(fx, fy, fz);
        Voxel vu = world.get_voxel(fx, uy, fz);
        if (vf.is_liquid() || vu.is_liquid()) {
          // Inside or on surface of liquid pool: buoyant in fluid, do not plunge through
        } else {
          float ground_y = 0.0f;
          if (AberrantAI::find_ground_surface(world, s.position, 1.35f, 4.0f, &ground_y)) {
            if (s.position.y > ground_y + 0.05f) {
              s.position.y = std::max(ground_y, s.position.y - 9.8f * dt);
            }
          } else {
            s.position.y -= 9.8f * dt; // Cavern gravity over cliffs / drops
          }
        }
      }

      // Robust Voxel Collision & Boundary Clamping (Prevents moving through
      // walls, enables 1-block step clambering)
      float col_radius = (s.role == StalkerRole::ChitinGoliath) ? 0.65f : 0.38f;
      float col_height = (s.role == StalkerRole::ChitinGoliath) ? 1.4f : 0.85f;
      AberrantAI::resolve_voxel_collision(s.position, s.velocity, col_radius,
                                          col_height, s.surface_state,
                                          s.contact_normal, world);

      // ─── Environmental Hazard Perception & Damage (Lava, Acid, Spikes, Toxic Gas) ───
      s.spike_damage_timer = std::max(0.0f, s.spike_damage_timer - dt);

      glm::vec3 feet_pos = s.position;
      glm::vec3 torso_pos = s.position + glm::vec3(0.0f, 0.45f, 0.0f);
      glm::vec3 underfoot_pos = s.position - glm::vec3(0.0f, 0.15f, 0.0f);

      Voxel v_feet = world.get_voxel(static_cast<int>(std::floor(feet_pos.x)),
                                     static_cast<int>(std::floor(feet_pos.y)),
                                     static_cast<int>(std::floor(feet_pos.z)));
      Voxel v_torso = world.get_voxel(static_cast<int>(std::floor(torso_pos.x)),
                                      static_cast<int>(std::floor(torso_pos.y)),
                                      static_cast<int>(std::floor(torso_pos.z)));
      Voxel v_under = world.get_voxel(static_cast<int>(std::floor(underfoot_pos.x)),
                                      static_cast<int>(std::floor(underfoot_pos.y)),
                                      static_cast<int>(std::floor(underfoot_pos.z)));

      bool in_lava = (v_feet.material_id == MAT_THERMITE_SLAG || v_feet.material_id == MAT_LAVA || v_feet.material_id == MAT_MOLTEN_MAGMA ||
                      v_torso.material_id == MAT_THERMITE_SLAG || v_torso.material_id == MAT_LAVA || v_torso.material_id == MAT_MOLTEN_MAGMA ||
                      v_under.material_id == MAT_THERMITE_SLAG || v_under.material_id == MAT_LAVA || v_under.material_id == MAT_MOLTEN_MAGMA);

      bool in_acid = (v_feet.material_id == MAT_ACID || v_torso.material_id == MAT_ACID || v_under.material_id == MAT_ACID);

      bool in_coolant = (v_feet.material_id == MAT_COOLANT || v_torso.material_id == MAT_COOLANT || v_under.material_id == MAT_COOLANT);

      bool in_spikes = (v_feet.material_id == MAT_OBSIDIAN_SPIKES || v_under.material_id == MAT_OBSIDIAN_SPIKES);

      bool in_gas = (v_feet.material_id == MAT_GAS || v_torso.material_id == MAT_GAS || v_under.material_id == MAT_GAS);

      s.is_in_lava = in_lava;
      s.is_in_hazard = in_lava || in_acid || in_coolant || in_spikes || in_gas;

      if (in_lava) {
        float lava_dmg = 35.0f * dt;
        s.hp -= lava_dmg;
        s.hit_flash_timer = 0.15f;
        s.velocity.x *= std::max(0.0f, 1.0f - 6.0f * dt);
        s.velocity.z *= std::max(0.0f, 1.0f - 6.0f * dt);
        s.velocity.y = std::max(s.velocity.y, 3.8f);
        if (s.screech_timer <= 0.0f) {
          s.just_screeched = true;
          s.screech_timer = 1.0f;
          result.any_screech = true;
        }
        if (s.state != StalkerState::Fleeing && s.state != StalkerState::Dying && s.state != StalkerState::Dead) {
          s.state = StalkerState::Fleeing;
          s.state_timer = 0.0f;
        }
        if (s.hp <= 0.0f) {
          s.hp = 0.0f;
          s.state = StalkerState::Dying;
          s.just_died = true;
          s.death_timer = 0.0f;
          s.hit_flash_timer = 0.0f;
          s.death_hit_dir = glm::vec3(0.0f, 1.0f, 0.0f);
          s.death_impulse = glm::vec3(0.0f, 3.5f, 0.0f);
          s.velocity = s.death_impulse;
          s.death_angular_velocity = glm::vec3(4.0f, 1.0f, 2.0f);
          VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " INCINERATED in molten lava / thermite slag!");
        }
      } else if (in_acid) {
        float acid_dmg = 25.0f * dt;
        s.hp -= acid_dmg;
        s.hit_flash_timer = 0.12f;
        s.velocity.x *= std::max(0.0f, 1.0f - 4.0f * dt);
        s.velocity.z *= std::max(0.0f, 1.0f - 4.0f * dt);
        s.velocity.y = std::max(s.velocity.y, 2.8f);
        if (s.screech_timer <= 0.0f) {
          s.just_screeched = true;
          s.screech_timer = 1.2f;
          result.any_screech = true;
        }
        if (s.state != StalkerState::Fleeing && s.state != StalkerState::Dying && s.state != StalkerState::Dead) {
          s.state = StalkerState::Fleeing;
          s.state_timer = 0.0f;
        }
        if (s.hp <= 0.0f) {
          s.hp = 0.0f;
          s.state = StalkerState::Dying;
          s.just_died = true;
          s.death_timer = 0.0f;
          s.hit_flash_timer = 0.0f;
          s.death_hit_dir = glm::vec3(0.0f, 1.0f, 0.0f);
          s.death_impulse = glm::vec3(0.0f, 2.5f, 0.0f);
          s.velocity = s.death_impulse;
          VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " DISSOLVED in corrosive acid!");
        }
      } else if (in_coolant) {
        float cryo_dmg = 15.0f * dt;
        s.hp -= cryo_dmg;
        s.hit_flash_timer = 0.10f;
        s.velocity.x *= std::max(0.0f, 1.0f - 8.0f * dt); // Heavy frost slow
        s.velocity.z *= std::max(0.0f, 1.0f - 8.0f * dt);
        s.velocity.y = std::max(s.velocity.y, 2.0f);
        if (s.screech_timer <= 0.0f) {
          s.just_screeched = true;
          s.screech_timer = 1.5f;
          result.any_screech = true;
        }
        if (s.state != StalkerState::Fleeing && s.state != StalkerState::Dying && s.state != StalkerState::Dead) {
          s.state = StalkerState::Fleeing;
          s.state_timer = 0.0f;
        }
        if (s.hp <= 0.0f) {
          s.hp = 0.0f;
          s.state = StalkerState::Dying;
          s.just_died = true;
          s.death_timer = 0.0f;
          s.hit_flash_timer = 0.0f;
          s.death_hit_dir = glm::vec3(0.0f, 1.0f, 0.0f);
          s.death_impulse = glm::vec3(0.0f, 2.0f, 0.0f);
          s.velocity = s.death_impulse;
          VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " FROZEN in cryogenic coolant!");
        }
      } else if (in_spikes) {
        s.velocity.x *= 0.25f;
        s.velocity.z *= 0.25f;
        if (s.spike_damage_timer <= 0.0f) {
          s.spike_damage_timer = 0.8f;
          s.hp -= 20.0f;
          s.hit_flash_timer = 0.15f;
          s.velocity.y = std::max(s.velocity.y, 3.0f);
          if (s.screech_timer <= 0.0f) {
            s.just_screeched = true;
            s.screech_timer = 0.8f;
            result.any_screech = true;
          }
          if (s.state != StalkerState::Fleeing && s.state != StalkerState::Dying && s.state != StalkerState::Dead) {
            s.state = StalkerState::Fleeing;
            s.state_timer = 0.0f;
          }
          if (s.hp <= 0.0f) {
            s.hp = 0.0f;
            s.state = StalkerState::Dying;
            s.just_died = true;
            s.death_timer = 0.0f;
            s.hit_flash_timer = 0.0f;
            VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " IMPALED on obsidian punji spikes!");
          }
        }
      } else if (in_gas) {
        float gas_dmg = 8.0f * dt;
        s.hp -= gas_dmg;
        if (s.hp <= 0.0f) {
          s.hp = 0.0f;
          s.state = StalkerState::Dying;
          s.just_died = true;
          s.death_timer = 0.0f;
          s.hit_flash_timer = 0.0f;
          VF_LOG_INFO("VoidStalker", "Stalker " << s.id << " SUFFOCATED in toxic gas!");
        }
      }
    }

    // ─── Adaptive Locomotion & Footstep Audio Cadence ───
    if (s.state != StalkerState::Dead && s.state != StalkerState::Dying &&
        s.state != StalkerState::Roosting && s.state != StalkerState::Burrowing &&
        s.state != StalkerState::Emerging && s.state != StalkerState::Stunned) {
      float physical_disp = glm::distance(s.position, frame_start_pos);
      float physical_speed = physical_disp / std::max(0.0001f, dt);
      float speed = glm::length(glm::vec2(s.velocity.x, s.velocity.z));
      if (speed < 0.1f) {
        speed = glm::length(s.velocity);
      }

      // Check if entity is stuck, wedged against obstacles/walls, or lacking physical progress
      bool is_stuck = (s.stuck_timer > 0.15f || s.wall_stuck_timer > 0.15f);
      if (is_stuck) {
        s.footstep_timer = 0.0f;
        s.just_stepped = false;
      } else if (speed >= 0.25f) {
        bool is_running = (speed >= 3.8f || s.state == StalkerState::Stalking ||
                           s.state == StalkerState::Lunging || s.state == StalkerState::Charging ||
                           s.state == StalkerState::Fleeing);
        s.step_is_run = is_running;

        float interval = 0.40f;
        if (s.role == StalkerRole::ChitinGoliath) {
          interval = (s.state == StalkerState::Charging) ? 0.26f : std::clamp(0.55f / std::max(0.5f, speed / 2.5f), 0.30f, 0.65f);
        } else if (is_running) {
          interval = std::clamp(0.24f / std::max(0.6f, speed / 4.8f), 0.16f, 0.30f);
        } else {
          interval = std::clamp(0.42f / std::max(0.5f, speed / 2.2f), 0.28f, 0.50f);
        }

        s.footstep_timer += dt;
        if (s.footstep_timer >= interval) {
          s.footstep_timer = 0.0f;
          s.just_stepped = true;
          result.any_step = true;
          if (s.role == StalkerRole::ChitinGoliath) {
            result.any_goliath_step = true;
          } else if (is_running) {
            result.any_run_step = true;
          }

          // Adaptive Proximity & Directional Gain (louder when closer and approaching)
          float dist = glm::distance(s.position, player_pos);
          float prox = std::clamp(1.0f - (dist - 3.0f) / 27.0f, 0.0f, 1.0f);
          float gain_prox = 0.45f + 0.85f * std::pow(prox, 1.15f);

          glm::vec3 to_player = player_pos - s.position;
          float to_player_len = glm::length(to_player);
          float approach = 0.0f;
          if (to_player_len > 0.01f && speed > 0.1f) {
            approach = std::max(0.0f, glm::dot(glm::normalize(s.velocity), to_player / to_player_len));
          }
          float gain_approach = 1.0f + 0.20f * approach;

          // Stealth predator locomotion: quiet creeping chitin claw clicks and scuttle in the dark
          float base_vol = (s.role == StalkerRole::ChitinGoliath) ? 0.95f : (is_running ? 0.40f : 0.24f);
          float max_vol = (s.role == StalkerRole::ChitinGoliath) ? 1.25f : (is_running ? 0.58f : 0.38f);
          s.step_volume = std::clamp(base_vol * gain_prox * gain_approach, 0.10f, max_vol);

          float pitch_base = (s.role == StalkerRole::ChitinGoliath) ? 0.85f : (is_running ? 1.08f : 0.95f);
          float jitter = 0.95f + 0.10f * std::fmod(std::abs(s.position.x * 12.3f + s.position.z * 7.7f), 1.0f);
          s.step_pitch = pitch_base * jitter;
        }
      } else {
        s.footstep_timer = std::max(0.0f, s.footstep_timer - dt * 2.0f);
      }
    } else {
      s.footstep_timer = 0.0f;
    }
  }

  // ─── Pairwise Stalker-to-Stalker Depenetration (Hard Anti-Overlap) ───
  for (int pass = 0; pass < 2; ++pass) {
    for (size_t i = 0; i < m_stalkers.size(); ++i) {
      auto& s1 = m_stalkers[i];
      if (s1.state == StalkerState::Dead || s1.state == StalkerState::Dying || s1.state == StalkerState::Burrowing)
        continue;
      float r1 = (s1.role == StalkerRole::ChitinGoliath) ? 0.75f : 0.45f;
      float w1 = (s1.role == StalkerRole::ChitinGoliath) ? 4.0f : 1.0f;

      for (size_t j = i + 1; j < m_stalkers.size(); ++j) {
        auto& s2 = m_stalkers[j];
        if (s2.state == StalkerState::Dead || s2.state == StalkerState::Dying || s2.state == StalkerState::Burrowing)
          continue;
        float r2 = (s2.role == StalkerRole::ChitinGoliath) ? 0.75f : 0.45f;
        float w2 = (s2.role == StalkerRole::ChitinGoliath) ? 4.0f : 1.0f;

        if (AberrantAI::resolve_enemy_to_enemy_penetration(
                s1.position, s1.velocity, r1, w1,
                s2.position, s2.velocity, r2, w2,
                s1.id, s2.id)) {
          // Re-clamp both against voxels so separation does not push entities inside solid blocks
          AberrantAI::resolve_voxel_collision(s1.position, s1.velocity, r1,
              (s1.role == StalkerRole::ChitinGoliath) ? 1.4f : 0.85f,
              s1.surface_state, s1.contact_normal, world);
          AberrantAI::resolve_voxel_collision(s2.position, s2.velocity, r2,
              (s2.role == StalkerRole::ChitinGoliath) ? 1.4f : 0.85f,
              s2.surface_state, s2.contact_normal, world);
        }
      }
    }
  }

  // === Update Active Void Spine Projectiles ===
  for (auto &p : m_projectiles) {
    if (!p.active)
      continue;
    p.lifetime -= dt;
    if (p.lifetime <= 0.0f) {
      p.active = false;
      continue;
    }

    p.position += p.velocity * dt;

    // Voxel collision check (blocks or industrial bulkheads block spines!)
    int vx = static_cast<int>(std::floor(p.position.x));
    int vy = static_cast<int>(std::floor(p.position.y));
    int vz = static_cast<int>(std::floor(p.position.z));
    Voxel vox = world.get_voxel(vx, vy, vz);
    if (vox.material_id != MAT_AIR && vox.material_id != MAT_GAS &&
        vox.material_id != MAT_VOLATILE_SMOKE) {
      p.active = false;
      continue;
    }

    // Player collision check
    float p_dist =
        glm::distance(p.position, player_pos + glm::vec3(0.0f, 0.5f, 0.0f));
    if (p_dist < 1.1f) {
      p.active = false;
      result.total_damage += p.damage;
      VF_LOG_INFO("VoidStalker",
                  "Void spine IMPALED player (-" << p.damage << " HP)!");
    }
  }

  // Clean up inactive projectiles
  m_projectiles.erase(
      std::remove_if(m_projectiles.begin(), m_projectiles.end(),
                     [](const VoidSpikeProjectile &p) { return !p.active; }),
      m_projectiles.end());

  return result;
}

void VoidStalkerManager::apply_sonar_stun(const glm::vec3 &origin,
                                          float radius) {
  for (auto &s : m_stalkers) {
    if (s.state == StalkerState::Dead)
      continue;
    float dist = glm::distance(s.position, origin);
    if (dist < radius) {
      s.state = StalkerState::Stunned;
      s.state_timer = 0.0f;
      s.stun_timer = 3.0f; // 3 second stun
      s.velocity = glm::vec3(0.0f);
      VF_LOG_INFO("VoidStalker",
                  "Stalker " << s.id << " STUNNED by sonar pulse (dist=" << dist
                             << "m)");
    }
  }
}

int VoidStalkerManager::apply_melee_shove(const glm::vec3 &camera_pos,
                                          const glm::vec3 &camera_dir,
                                          float range, float min_cos,
                                          float damage) {
  int hit_count = 0;
  glm::vec3 norm_cam_dir = (glm::length(camera_dir) > 0.001f)
                               ? glm::normalize(camera_dir)
                               : glm::vec3(0.0f, 0.0f, -1.0f);

  for (auto &s : m_stalkers) {
    if (s.state == StalkerState::Dead || s.state == StalkerState::Dying)
      continue;

    glm::vec3 to_s = s.position - camera_pos;
    float dist = glm::length(to_s);
    if (dist > range || dist < 0.001f)
      continue;

    glm::vec3 dir_to_s = to_s / dist;
    float cos_angle = glm::dot(norm_cam_dir, dir_to_s);
    if (cos_angle > min_cos) {
      // Deals light damage (15 HP)
      s.hp -= damage;

      // Interrupts Enemy Attack: Cancels active stalker bite/claw attack
      // windups
      s.attack_cooldown = 0.8f;
      s.slash_fx_timer = 0.0f;

      // Heavy Knockback Impulse: applies strong repulsive velocity
      // v_enemy += dir_camera * 12.0 m/s + up * 3.0 m/s
      glm::vec3 knockback =
          norm_cam_dir * 12.0f + glm::vec3(0.0f, 1.0f, 0.0f) * 3.0f;
      s.velocity += knockback;

      // Stuns hit enemies for 0.6s
      s.state = StalkerState::Stunned;
      s.stun_timer = 0.6f;
      s.state_timer = 0.0f;

      if (s.hp <= 0.0f) {
        s.hp = 0.0f;
        s.state = StalkerState::Dying;
        s.just_died = true;
        s.hit_flash_timer = 0.0f;
        s.death_timer = 0.0f;
        s.state_timer = 0.0f;
      }

      hit_count++;
      VF_LOG_INFO("VoidStalker", "Melee shove struck Stalker "
                                     << s.id
                                     << "! Interrupted & stunned for 0.6s.");
    }
  }
  return hit_count;
}

bool VoidStalkerManager::damage_nearest(const glm::vec3 &origin, float radius,
                                        float damage, bool allow_crit,
                                        bool *out_is_crit,
                                        float *out_damage_dealt,
                                        const glm::vec3 *attacker_pos,
                                        const glm::vec3 *shot_direction,
                                        float boss_damage_multiplier) {
  float best_dist = radius;
  VoidStalker *nearest = nullptr;

  for (auto &s : m_stalkers) {
    if (s.state == StalkerState::Dead || s.state == StalkerState::Dying)
      continue;
    float dist = glm::distance(s.position, origin);
    if (dist < best_dist) {
      best_dist = dist;
      nearest = &s;
    }
  }

  if (nearest) {
    bool is_crit = false;
    float effective_damage = damage;

    // Apply boss slayer perk if attacking an apex boss
    if (nearest->is_boss() || nearest->role == StalkerRole::ChitinGoliath) {
      effective_damage *= std::max(1.0f, boss_damage_multiplier);
    }

    // Resolve actual position of attacker and flight vector of shot
    glm::vec3 actual_attacker_pos =
        (attacker_pos != nullptr) ? *attacker_pos : origin;
    if (attacker_pos == nullptr &&
        glm::distance(origin, nearest->position) < 0.5f) {
      // If origin is right on top of the enemy (e.g. projectile hit point) and
      // no attacker_pos was provided, construct an offset opposite to facing as
      // fallback
      glm::vec3 facing(std::cos(nearest->yaw), 0.0f, std::sin(nearest->yaw));
      actual_attacker_pos = nearest->position - facing * 4.0f;
    }

    glm::vec3 to_attacker_vec = actual_attacker_pos - nearest->position;
    glm::vec3 to_attacker = (glm::length(to_attacker_vec) > 0.001f)
                                ? glm::normalize(to_attacker_vec)
                                : glm::vec3(0.0f, 0.0f, 1.0f);

    glm::vec3 shot_dir;
    if (shot_direction != nullptr && glm::length(*shot_direction) > 0.001f) {
      shot_dir = glm::normalize(*shot_direction);
    } else {
      shot_dir = (glm::length(nearest->position - actual_attacker_pos) > 0.001f)
                     ? glm::normalize(nearest->position - actual_attacker_pos)
                     : glm::vec3(0.0f, 0.0f, 1.0f);
    }

    // Chitin Goliath Front Armor Plate Deflection (85% reduction on front arc)
    // EXCEPTION: When Stunned (e.g. wall crash or sonic stun), front armor plates are exposed/breached!
    if (nearest->role == StalkerRole::ChitinGoliath) {
      if (nearest->state == StalkerState::Stunned) {
        VF_LOG_INFO("VoidStalker",
                    "Chitin Goliath is STUNNED - Front armor breached! Full damage="
                        << effective_damage);
      } else {
        glm::vec3 facing(std::cos(nearest->yaw), 0.0f, std::sin(nearest->yaw));
        float front_dot = glm::dot(facing, to_attacker);
        if (front_dot > 0.1f) {
          // Front armor deflection: 85% damage reduction
          effective_damage *= 0.15f;
          VF_LOG_INFO(
              "VoidStalker",
              "Chitin Goliath front armor deflected attack! Reduced damage to "
                  << effective_damage);
        } else {
          VF_LOG_INFO("VoidStalker",
                      "Chitin Goliath rear weak point struck! Full damage="
                          << effective_damage);
        }
      }
    }

    if (allow_crit && nearest->is_unalerted()) {
      is_crit = true;
      effective_damage *= StealthSystem::SNEAK_ATTACK_CRIT_MULTIPLIER;
      VF_LOG_INFO("VoidStalker",
                  "SNEAK ATTACK CRITICAL HIT on unalerted Stalker "
                      << nearest->id << "! Base=" << damage
                      << " -> Crit=" << effective_damage);
    }
    if (out_is_crit)
      *out_is_crit = is_crit;
    if (out_damage_dealt)
      *out_damage_dealt = effective_damage;

    // Apply hit-flash pulse (0.08s white emissive pulse) and knockback impulse
    // along shot vector
    nearest->hit_flash_timer = 0.08f;

    if (effective_damage > 20.0f) {
      nearest->velocity += shot_dir * 6.5f;
    } else {
      float knockback_mag =
          (nearest->role == StalkerRole::ChitinGoliath) ? 1.0f : 3.5f;
      nearest->velocity += shot_dir * knockback_mag;
    }

    // Detach wall/ceiling gravity locks immediately and lock surface to FLOOR
    // to prevent jitter
    nearest->hit_surface_lock_timer = 0.8f;
    nearest->surface_cooldown = 0.8f;
    nearest->surface_state = StalkerSurfaceState::FLOOR;
    nearest->target_surface_state = StalkerSurfaceState::FLOOR;
    nearest->contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
    nearest->m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);

    nearest->hp -= effective_damage;
    if (nearest->hp <= 0.0f) {
      nearest->state = StalkerState::Dying;
      nearest->just_died = true;
      nearest->death_timer = 0.0f;
      nearest->hit_flash_timer =
          0.0f; // Fatal damage clears hit flash so enemies do not stay white
      VF_LOG_INFO("VoidStalker",
                  "Stalker " << nearest->id
                             << " FATAL DAMAGE -> DYING collapse (damage="
                             << effective_damage << ")");

      glm::vec3 lethal_dir = shot_dir;
      nearest->death_hit_dir = lethal_dir;
      glm::vec3 up_vec(0.0f, 1.0f, 0.0f);
      nearest->death_impulse = lethal_dir * 4.0f + up_vec * 1.5f;
      nearest->velocity = nearest->death_impulse;

      glm::vec3 torque_axis = glm::cross(lethal_dir, up_vec);
      if (glm::length(torque_axis) < 0.01f)
        torque_axis = glm::vec3(1.0f, 0.0f, 0.0f);
      else
        torque_axis = glm::normalize(torque_axis);
      nearest->death_angular_velocity = torque_axis * 6.0f;
    } else if (nearest->hp <= nearest->flee_hp_threshold) {
      nearest->state = StalkerState::Fleeing;
      nearest->state_timer = 0.0f;
      VF_LOG_INFO("VoidStalker", "Stalker " << nearest->id
                                            << " critically injured (HP="
                                            << nearest->hp << ") -> FLEEING!");
    } else {
      // Activate persistent retaliation pursuit against the attacking player
      nearest->is_pursuing_attacker = true;
      nearest->pursuit_lost_timer = 0.0f;
      nearest->target_pos = actual_attacker_pos;
      nearest->last_seen_player_pos = actual_attacker_pos;
      nearest->lost_los_timer = 0.0f;

      // Immediately turn to face the attacker
      if (glm::length(glm::vec2(to_attacker_vec.x, to_attacker_vec.z)) > 0.1f) {
        nearest->yaw = std::atan2(to_attacker_vec.x, to_attacker_vec.z);
      }

      float dist_to_att = glm::distance(nearest->position, actual_attacker_pos);
      if (nearest->role == StalkerRole::Melee) {
        // Close range (< 9.0m) -> immediate ENRAGED counter-lunge!
        // Mid-to-long range -> sprint in Stalking state directly at shooter!
        if (dist_to_att <= 9.0f) {
          if (nearest->state != StalkerState::Lunging) {
            nearest->state = StalkerState::Lunging;
            nearest->state_timer = 0.0f;
            nearest->just_lunged = true;
          }
          VF_LOG_INFO("VoidStalker",
                      "Melee Stalker "
                          << nearest->id << " ENRAGED by player damage (HP="
                          << nearest->hp << ") -> COUNTER-LUNGING!");
        } else {
          if (nearest->state != StalkerState::Stalking) {
            nearest->state = StalkerState::Stalking;
            nearest->state_timer = 0.0f;
          }
          VF_LOG_INFO(
              "VoidStalker",
              "Melee Stalker "
                  << nearest->id
                  << " ENRAGED by player damage -> CHARGING attacker at "
                  << dist_to_att << "m!");
        }
      } else {
        // Shooter -> Repositions to flanking circle and quickly returns fire
        // with void spines!
        nearest->state = StalkerState::Circling;
        nearest->state_timer = 0.0f;
        nearest->projectile_cooldown = 0.15f;
        VF_LOG_INFO(
            "VoidStalker",
            "Shooter Stalker "
                << nearest->id
                << " REPOSITIONS after damage to return fire and pursue!");
      }

      // Pack Alert: Alert at most 1 nearby packmate (2 in Sector 3+) within <= 8.0m with clear LOS
      int alerted_count = 0;
      int max_pack_alerts = (m_sector_level >= 3) ? 2 : 1;
      for (auto &other : m_stalkers) {
        if (&other != nearest && other.state != StalkerState::Dead &&
            other.state != StalkerState::Dying &&
            other.state != StalkerState::Fleeing) {
          float pack_dist = glm::distance(other.position, nearest->position);
          if (pack_dist <= 8.0f && (other.state == StalkerState::Roosting ||
                                     other.state == StalkerState::Idle)) {
            if (alerted_count < max_pack_alerts) {
              alerted_count++;
              other.surface_state = StalkerSurfaceState::FLOOR;
              other.target_surface_state = StalkerSurfaceState::FLOOR;
              other.contact_normal = glm::vec3(0.0f, 1.0f, 0.0f);
              other.m_targetUpVector = glm::vec3(0.0f, 1.0f, 0.0f);
              other.target_pos = actual_attacker_pos;
              other.last_seen_player_pos = actual_attacker_pos;
              other.lost_los_timer = 0.0f;
              if (can_engage_new_chaser()) {
                other.state = StalkerState::Stalking;
                other.is_pursuing_attacker = true;
                other.pursuit_lost_timer = 0.0f;
              } else {
                other.state = StalkerState::Investigating;
                other.investigation_timer = 3.5f;
                other.investigation_target = actual_attacker_pos;
                other.has_sound_target = true;
              }
              glm::vec3 to_att = actual_attacker_pos - other.position;
              if (glm::length(glm::vec2(to_att.x, to_att.z)) > 0.1f) {
                other.yaw = std::atan2(to_att.x, to_att.z);
              }
              VF_LOG_INFO("VoidStalker",
                          "Pack Stalker "
                              << other.id
                              << " ALERTED by distress of wounded Stalker "
                              << nearest->id << "!");
            }
          }
        }
      }
    }
    return true;
  }
  return false;
}

int VoidStalkerManager::active_count() const {
  int count = 0;
  for (const auto &s : m_stalkers) {
    if (s.state != StalkerState::Dead && s.state != StalkerState::Dying)
      ++count;
  }
  return count;
}

void VoidStalkerManager::remove_dead() {
  m_stalkers.erase(std::remove_if(m_stalkers.begin(), m_stalkers.end(),
                                  [](const VoidStalker &s) {
                                    return s.state == StalkerState::Dead;
                                  }),
                   m_stalkers.end());
}

void VoidStalkerManager::set_sector_level(int level) {
  m_sector_level = level;
  m_max_concurrent_chasers = (level <= 1) ? 2 : ((level == 2) ? 3 : 4);
}

int VoidStalkerManager::concurrent_chasers_count() const {
  int count = 0;
  for (const auto &s : m_stalkers) {
    if (s.state == StalkerState::Dead || s.state == StalkerState::Dying ||
        s.state == StalkerState::Fleeing)
      continue;
    if (s.state == StalkerState::Stalking || s.state == StalkerState::Lunging ||
        s.state == StalkerState::Circling || s.is_pursuing_attacker) {
      count++;
    }
  }
  return count;
}

bool VoidStalkerManager::can_engage_new_chaser() const {
  if (m_combat_recovery_timer > 0.0f) {
    return false;
  }
  return concurrent_chasers_count() < m_max_concurrent_chasers;
}

void VoidStalkerManager::trigger_combat_recovery(float duration) {
  m_combat_recovery_timer = std::max(m_combat_recovery_timer, duration);
}

void VoidStalkerManager::reset() {
  m_stalkers.clear();
  m_projectiles.clear();
  m_spatial_hash.clear();
  m_next_id = 1;
  m_next_proj_id = 1;
  m_combat_recovery_timer = 0.0f;
  m_sector_level = 1;
  m_max_concurrent_chasers = 2;
}

void VoidStalkerManager::assign_patrol_zones_from_world(const World& world) {
  if (!world.level_generator()) return;
  for (auto& s : m_stalkers) {
    const RoomPlacement* room = world.level_generator()->get_room_at_position(s.position);
    if (room) {
      PatrolZone zone;
      zone.zone_id = room->grid_x * 10 + room->grid_z;
      zone.center = glm::vec3(room->center);
      zone.bounds_min = glm::vec3(
          static_cast<float>(room->center.x - room->half_width + 1.5f),
          static_cast<float>(room->floor_y),
          static_cast<float>(room->center.z - room->half_depth + 1.5f)
      );
      zone.bounds_max = glm::vec3(
          static_cast<float>(room->center.x + room->half_width - 1.5f),
          static_cast<float>(room->ceiling_y),
          static_cast<float>(room->center.z + room->half_depth - 1.5f)
      );
      zone.has_bounds = true;
      zone.radius = static_cast<float>(std::max(room->half_width, room->half_depth));
      zone.floor_y = room->floor_y;
      zone.ceiling_y = room->ceiling_y;
      zone.waypoints = AberrantAI::generate_zone_waypoints(zone, world, 4);
      s.set_patrol_zone(zone);
    }
  }
}

void VoidStalkerManager::resolve_burrower_penetrations(std::vector<SeismicBurrower>& burrowers, const World& world) {
  for (int pass = 0; pass < 2; ++pass) {
    for (auto& s : m_stalkers) {
      if (s.state == StalkerState::Dead || s.state == StalkerState::Dying || s.state == StalkerState::Burrowing)
        continue;
      float r_s = (s.role == StalkerRole::ChitinGoliath) ? 0.75f : 0.45f;
      float w_s = (s.role == StalkerRole::ChitinGoliath) ? 4.0f : 1.0f;

      for (auto& b : burrowers) {
        if (b.is_dead() || b.is_dying())
          continue;
        float r_b = 0.95f;
        float w_b = 3.5f;

        if (AberrantAI::resolve_enemy_to_enemy_penetration(
                s.position, s.velocity, r_s, w_s,
                b.position, b.velocity, r_b, w_b,
                s.id, b.id)) {
          // Re-clamp stalker against voxels
          AberrantAI::resolve_voxel_collision(s.position, s.velocity, r_s,
              (s.role == StalkerRole::ChitinGoliath) ? 1.4f : 0.85f,
              s.surface_state, s.contact_normal, world);
        }
      }
    }
  }
}

} // namespace Voidfall
