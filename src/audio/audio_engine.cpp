#include "audio_engine.hpp"
#include "../core/logger.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

namespace Voidfall {

namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;

// Lightweight deterministic pseudo-random generator (zero allocations)
inline float fast_rand(uint32_t& seed) {
    seed = (seed * 1664525u + 1013904223u);
    return static_cast<float>(seed & 0x00FFFFFF) / static_cast<float>(0x00FFFFFF);
}

// High-accuracy, branchless sinusoidal approximation from normalized phase [0..1)
// THD < 0.1%, ~8x faster than std::sin with zero trigonometric table lookups
inline float fast_sin_phase(float p) {
    p = p - static_cast<int>(p);
    if (p < 0.0f) p += 1.0f;
    float x = p * 2.0f - 1.0f;
    float y = 4.0f * x * (1.0f - std::abs(x));
    return 0.775f * y + 0.225f * y * std::abs(y);
}

// Fast sine from radian input
inline float fast_sin(float rad) {
    return fast_sin_phase(rad * (1.0f / TWO_PI));
}

// Exact 1-pole low-pass filter coefficient: alpha = omega / (1 + omega)
inline float calc_lp_alpha(float cutoff_hz, float sample_rate) {
    float omega = TWO_PI * std::clamp(cutoff_hz, 10.0f, sample_rate * 0.45f) / sample_rate;
    return omega / (1.0f + omega);
}

// 2-pole resonant bandpass filter (formant vocal cavities & acoustic cavern resonance)
inline float process_resonator(float input, float freq, float q, float sample_rate, float& y1, float& y2) {
    float omega = TWO_PI * std::clamp(freq, 20.0f, sample_rate * 0.45f) / sample_rate;
    float r = std::clamp(1.0f - (PI * freq / (std::max(q, 0.1f) * sample_rate)), 0.0f, 0.998f);
    float cos_w = fast_sin(omega + 0.5f * PI);
    float y = input + 2.0f * r * cos_w * y1 - (r * r) * y2;
    y2 = y1;
    y1 = y;
    return (1.0f - r) * y;
}

// Map SoundCue enum to WAV asset filename in assets/sounds/
const char* get_sound_cue_filename(SoundCue cue) {
    switch (cue) {
        case SoundCue::DrillLoop:             return "drill_loop.wav";
        case SoundCue::VoxelHit:              return "voxel_hit.wav";
        case SoundCue::VoxelBreakBasalt:      return "voxel_break_basalt.wav";
        case SoundCue::VoxelBreakTitanium:    return "voxel_break_titanium.wav";
        case SoundCue::VoxelBreakVoidite:     return "voxel_break_voidite.wav";
        case SoundCue::VoxelBreakBulkhead:    return "voxel_break_bulkhead.wav";
        case SoundCue::VoxelBreakRadioactive: return "voxel_break_radioactive.wav";
        case SoundCue::BulkheadDeploy:        return "bulkhead_deploy.wav";
        case SoundCue::BulkheadDismantle:     return "bulkhead_dismantle.wav";

        case SoundCue::StalkerSpotted:        return "stalker_spotted.wav";
        case SoundCue::StalkerLunge:          return "stalker_lunge.wav";
        case SoundCue::StalkerHit:            return "stalker_hit.wav";
        case SoundCue::StalkerDie:            return "stalker_die.wav";
        case SoundCue::StalkerEchoScreech:    return "stalker_echo_screech.wav";
        case SoundCue::StalkerChitter:        return "stalker_chitter.wav";
        case SoundCue::StalkerHiss:           return "stalker_hiss.wav";
        case SoundCue::BurrowerRoar:          return "burrower_roar.wav";
        case SoundCue::BurrowerGrind:         return "burrower_grind.wav";

        case SoundCue::AmbientCavern:         return "ambient_cavern.wav";
        case SoundCue::AmbientSector1:        return "ambient_sector1.wav";
        case SoundCue::AmbientSector2:        return "ambient_sector2.wav";
        case SoundCue::AmbientSector3:        return "ambient_sector3.wav";
        case SoundCue::SectorArrival1:        return "sector_arrival_1.wav";
        case SoundCue::SectorArrival2:        return "sector_arrival_2.wav";
        case SoundCue::SectorArrival3:        return "sector_arrival_3.wav";
        case SoundCue::CavernDrip:            return "cavern_drip.wav";
        case SoundCue::CavernGroan:           return "cavern_groan.wav";
        case SoundCue::CrystalChime:          return "crystal_chime.wav";
        case SoundCue::GeothermalVent:        return "geothermal_vent.wav";
        case SoundCue::VoidDistortion:        return "void_distortion.wav";
        case SoundCue::SeismicTremor:         return "seismic_tremor.wav";
        case SoundCue::GeigerClick:           return "geiger_click.wav";
        case SoundCue::BeaconSiren:           return "beacon_siren.wav";
        case SoundCue::EvacTouchdown:         return "evac_touchdown.wav";

        case SoundCue::PlasmaFire:            return "plasma_fire.wav";
        case SoundCue::PlasmaHit:             return "plasma_hit.wav";
        case SoundCue::ScattergunFire:        return "scattergun_fire.wav";
        case SoundCue::RailgunFire:           return "railgun_fire.wav";

        case SoundCue::Footstep:              return "footstep.wav";
        case SoundCue::Jump:                  return "jump.wav";
        case SoundCue::Land:                  return "land.wav";
        case SoundCue::SonarPulse:            return "sonar_pulse.wav";
        case SoundCue::ExplosiveBlast:        return "explosive_blast.wav";
        case SoundCue::TacticalBarricade:     return "tactical_barricade.wav";
        case SoundCue::TacticalOvercharge:    return "tactical_overcharge.wav";

        case SoundCue::UIBlip:                return "ui_blip.wav";
        case SoundCue::UIUpgrade:             return "ui_upgrade.wav";
        case SoundCue::DamageWarning:         return "damage_warning.wav";

        case SoundCue::LavaBubble:            return "lava_bubble.wav";
        case SoundCue::ThermalHiss:           return "thermal_hiss.wav";
        case SoundCue::RadioactiveHum:        return "radioactive_hum.wav";
        case SoundCue::VoidWind:              return "void_wind.wav";
        case SoundCue::GravityDistortion:     return "gravity_distortion.wav";
        case SoundCue::SporePlop:             return "spore_plop.wav";
        case SoundCue::OrganicCreak:          return "organic_creak.wav";
        case SoundCue::IndustrialHum:         return "industrial_hum.wav";
        case SoundCue::HydraulicExhaust:      return "hydraulic_exhaust.wav";
        case SoundCue::PebbleSkitter:         return "pebble_skitter.wav";
        case SoundCue::SpikeRattle:           return "spike_rattle.wav";

        default: return nullptr;
    }
}

// Zero-allocation-in-audio-thread WAV loader supporting 16-bit, 24-bit, and 32-bit float PCM
static bool load_wav_file(const std::string& path, AudioEngine::SoundSample& out_sample) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    char riff[4];
    file.read(riff, 4);
    if (file.gcount() < 4 || std::memcmp(riff, "RIFF", 4) != 0) return false;

    uint32_t file_size = 0;
    file.read(reinterpret_cast<char*>(&file_size), 4);

    char wave[4];
    file.read(wave, 4);
    if (file.gcount() < 4 || std::memcmp(wave, "WAVE", 4) != 0) return false;

    uint16_t audio_format = 0;
    uint16_t num_channels = 0;
    uint32_t sample_rate = 0;
    uint32_t byte_rate = 0;
    uint16_t block_align = 0;
    uint16_t bits_per_sample = 0;
    bool found_fmt = false;
    bool found_data = false;
    std::vector<char> raw_pcm;

    while (file && (!found_fmt || !found_data)) {
        char chunk_id[4];
        file.read(chunk_id, 4);
        if (file.gcount() < 4) break;

        uint32_t chunk_size = 0;
        file.read(reinterpret_cast<char*>(&chunk_size), 4);
        if (file.gcount() < 4) break;

        if (std::memcmp(chunk_id, "fmt ", 4) == 0) {
            file.read(reinterpret_cast<char*>(&audio_format), 2);
            file.read(reinterpret_cast<char*>(&num_channels), 2);
            file.read(reinterpret_cast<char*>(&sample_rate), 4);
            file.read(reinterpret_cast<char*>(&byte_rate), 4);
            file.read(reinterpret_cast<char*>(&block_align), 2);
            file.read(reinterpret_cast<char*>(&bits_per_sample), 2);

            if (chunk_size > 16) {
                file.seekg(chunk_size - 16, std::ios::cur);
            }
            found_fmt = true;
        } else if (std::memcmp(chunk_id, "data", 4) == 0) {
            raw_pcm.resize(chunk_size);
            file.read(raw_pcm.data(), chunk_size);
            found_data = true;
            break;
        } else {
            file.seekg(chunk_size, std::ios::cur);
        }
    }

    if (!found_fmt || !found_data || num_channels == 0 || sample_rate == 0) {
        return false;
    }

    size_t bytes_per_sample = bits_per_sample / 8;
    if (bytes_per_sample == 0) return false;
    size_t total_samples = raw_pcm.size() / bytes_per_sample;
    size_t total_frames = total_samples / num_channels;
    if (total_frames == 0) return false;

    out_sample.data.clear();
    out_sample.data.resize(total_frames * 2);
    out_sample.frame_count = total_frames;
    out_sample.duration_seconds = static_cast<float>(total_frames) / static_cast<float>(sample_rate);

    for (size_t f = 0; f < total_frames; ++f) {
        float l = 0.0f;
        float r = 0.0f;

        if (bits_per_sample == 16) {
            const int16_t* pcm16 = reinterpret_cast<const int16_t*>(raw_pcm.data());
            if (num_channels == 1) {
                l = r = pcm16[f] / 32768.0f;
            } else {
                l = pcm16[f * num_channels] / 32768.0f;
                r = pcm16[f * num_channels + 1] / 32768.0f;
            }
        } else if (bits_per_sample == 24) {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(raw_pcm.data()) + f * num_channels * 3;
            auto decode24 = [](const uint8_t* b) -> float {
                int32_t val = static_cast<int32_t>((b[0]) | (b[1] << 8) | (b[2] << 16));
                if (val & 0x800000) val |= ~0xFFFFFF; // Sign extend
                return static_cast<float>(val) / 8388608.0f;
            };
            if (num_channels == 1) {
                l = r = decode24(p);
            } else {
                l = decode24(p);
                r = decode24(p + 3);
            }
        } else if (bits_per_sample == 32) {
            if (audio_format == 3) { // IEEE float
                const float* pcm_f = reinterpret_cast<const float*>(raw_pcm.data());
                if (num_channels == 1) {
                    l = r = pcm_f[f];
                } else {
                    l = pcm_f[f * num_channels];
                    r = pcm_f[f * num_channels + 1];
                }
            } else {
                const int32_t* pcm32 = reinterpret_cast<const int32_t*>(raw_pcm.data());
                if (num_channels == 1) {
                    l = r = pcm32[f] / 2147483648.0f;
                } else {
                    l = pcm32[f * num_channels] / 2147483648.0f;
                    r = pcm32[f * num_channels + 1] / 2147483648.0f;
                }
            }
        }

        out_sample.data[f * 2] = l;
        out_sample.data[f * 2 + 1] = r;
    }

    out_sample.loaded = true;
    return true;
}

float get_cue_default_duration(SoundCue cue) {
    switch (cue) {
        case SoundCue::UIBlip:                return 0.08f;
        case SoundCue::UIUpgrade:             return 0.35f;
        case SoundCue::VoxelHit:              return 0.06f;
        case SoundCue::VoxelBreakBasalt:      return 0.22f;
        case SoundCue::VoxelBreakTitanium:    return 0.45f;
        case SoundCue::VoxelBreakVoidite:     return 0.50f;
        case SoundCue::VoxelBreakBulkhead:    return 0.38f;
        case SoundCue::VoxelBreakRadioactive: return 0.30f;
        case SoundCue::BulkheadDeploy:        return 0.25f;
        case SoundCue::BulkheadDismantle:     return 0.20f;
        case SoundCue::StalkerSpotted:        return 0.32f;
        case SoundCue::StalkerLunge:          return 0.40f;
        case SoundCue::StalkerHit:            return 0.18f;
        case SoundCue::StalkerDie:            return 0.55f;
        case SoundCue::StalkerEchoScreech:    return 0.85f;
        case SoundCue::StalkerChitter:        return 0.28f;
        case SoundCue::StalkerHiss:           return 0.45f;
        case SoundCue::BurrowerRoar:          return 0.95f;
        case SoundCue::BurrowerGrind:         return 0.70f;
        case SoundCue::AmbientCavern:         return 10.0f;
        case SoundCue::AmbientSector1:        return 10.0f;
        case SoundCue::AmbientSector2:        return 10.0f;
        case SoundCue::AmbientSector3:        return 10.0f;
        case SoundCue::SectorArrival1:        return 3.0f;
        case SoundCue::SectorArrival2:        return 3.0f;
        case SoundCue::SectorArrival3:        return 3.5f;
        case SoundCue::CavernDrip:            return 0.45f;
        case SoundCue::CavernGroan:           return 0.70f;
        case SoundCue::CrystalChime:          return 1.2f;
        case SoundCue::GeothermalVent:        return 1.5f;
        case SoundCue::VoidDistortion:        return 1.8f;
        case SoundCue::SeismicTremor:         return 1.10f;
        case SoundCue::GeigerClick:           return 0.01f;
        case SoundCue::BeaconSiren:           return 0.35f;
        case SoundCue::EvacTouchdown:         return 1.60f;
        case SoundCue::Footstep:              return 0.07f;
        case SoundCue::Jump:                  return 0.12f;
        case SoundCue::Land:                  return 0.15f;
        case SoundCue::SonarPulse:            return 0.85f;
        case SoundCue::ExplosiveBlast:        return 1.05f;
        case SoundCue::TacticalBarricade:     return 0.40f;
        case SoundCue::TacticalOvercharge:    return 0.35f;
        case SoundCue::PlasmaFire:            return 0.16f;
        case SoundCue::PlasmaHit:             return 0.12f;
        case SoundCue::ScattergunFire:        return 0.28f;
        case SoundCue::RailgunFire:           return 0.22f;
        case SoundCue::DamageWarning:         return 0.40f;
        case SoundCue::LavaBubble:            return 0.70f;
        case SoundCue::ThermalHiss:           return 0.95f;
        case SoundCue::RadioactiveHum:        return 1.20f;
        case SoundCue::VoidWind:              return 1.50f;
        case SoundCue::GravityDistortion:     return 1.30f;
        case SoundCue::SporePlop:             return 0.45f;
        case SoundCue::OrganicCreak:          return 0.85f;
        case SoundCue::IndustrialHum:         return 1.40f;
        case SoundCue::HydraulicExhaust:      return 0.75f;
        case SoundCue::PebbleSkitter:         return 0.65f;
        case SoundCue::SpikeRattle:           return 0.50f;
        default:                              return 0.20f;
    }
}

} // namespace

#ifdef _WIN32
struct AudioEngine::PlatformData {
    HWAVEOUT h_wave_out{nullptr};
    HANDLE h_event{nullptr};
    std::thread audio_thread;
    std::atomic<bool> running{false};

    static constexpr size_t BUFFER_FRAMES = 1024;
    static constexpr size_t NUM_BUFFERS = 4;
    std::array<WAVEHDR, NUM_BUFFERS> headers{};
    std::array<std::vector<int16_t>, NUM_BUFFERS> buffers;
};
#else
struct AudioEngine::PlatformData {
    std::atomic<bool> running{false};
};
#endif

AudioEngine::AudioEngine() {
    m_platform = std::make_unique<PlatformData>();
    for (auto& voice : m_voices) {
        voice.active = false;
    }
}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::init() {
    load_sound_samples();
    init_platform_audio();
    // Start continuous subterranean cavern ambience at initial volume (quiet, dreary negative space)
    play_sound_2d(SoundCue::AmbientCavern, 0.24f, 1.0f, true);
    return true;
}

bool AudioEngine::load_sound_samples(const std::string& directory_path) {
    std::lock_guard<std::recursive_mutex> lock(m_voice_mutex);

    std::vector<std::string> search_dirs;
    if (!directory_path.empty()) {
        search_dirs.push_back(directory_path);
    }
    search_dirs.push_back("assets/sounds");
    search_dirs.push_back("../assets/sounds");
    search_dirs.push_back("../../assets/sounds");

    std::string valid_dir;
    for (const auto& d : search_dirs) {
        if (std::filesystem::exists(d) && std::filesystem::is_directory(d)) {
            valid_dir = d;
            break;
        }
    }

    if (valid_dir.empty()) {
        VF_LOG_INFO("AudioEngine", "No assets/sounds directory found; operating in full procedural synthesis mode.");
        return false;
    }

    int loaded_count = 0;
    for (size_t i = 0; i < static_cast<size_t>(SoundCue::Count); ++i) {
        SoundCue cue = static_cast<SoundCue>(i);
        const char* fname = get_sound_cue_filename(cue);
        if (!fname) continue;

        std::filesystem::path full_path = std::filesystem::path(valid_dir) / fname;
        if (std::filesystem::exists(full_path)) {
            if (load_wav_file(full_path.string(), m_samples[i])) {
                loaded_count++;
            }
        }
    }

    VF_LOG_INFO("AudioEngine", "Loaded " << loaded_count << "/" << static_cast<size_t>(SoundCue::Count)
                << " audio asset samples from " << valid_dir << " (procedural fallback active for remaining).");
    return loaded_count > 0;
}

bool AudioEngine::has_sample(SoundCue cue) const {
    size_t idx = static_cast<size_t>(cue);
    if (idx >= m_samples.size()) return false;
    return m_samples[idx].loaded;
}

const AudioEngine::SoundSample* AudioEngine::get_sample(SoundCue cue) const {
    size_t idx = static_cast<size_t>(cue);
    if (idx >= m_samples.size()) return nullptr;
    return m_samples[idx].loaded ? &m_samples[idx] : nullptr;
}

void AudioEngine::trigger_ducking(float target_attenuation, float hold_seconds, float recovery_rate) {
    m_ducking_target = std::clamp(target_attenuation, 0.05f, 1.0f);
    m_ducking_attenuation = std::min(m_ducking_attenuation, m_ducking_target);
    m_ducking_timer = std::max(m_ducking_timer, hold_seconds);
    m_ducking_recovery_rate = std::clamp(recovery_rate, 0.2f, 5.0f);
}

void AudioEngine::shutdown() {
    stop_all();
    shutdown_platform_audio();
}

void AudioEngine::set_master_volume(float volume) {
    m_master_volume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioEngine::set_sfx_volume(float volume) {
    m_sfx_volume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioEngine::set_enemy_volume(float volume) {
    m_enemy_volume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioEngine::set_ambient_volume(float volume) {
    m_ambient_volume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioEngine::set_ui_volume(float volume) {
    m_ui_volume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioEngine::set_channel_volume(SoundCategory category, float volume) {
    float clamped = std::clamp(volume, 0.0f, 1.0f);
    switch (category) {
        case SoundCategory::Master: set_master_volume(clamped); break;
        case SoundCategory::SFX: m_sfx_volume = clamped; break;
        case SoundCategory::Enemy: m_enemy_volume = clamped; break;
        case SoundCategory::Ambience: m_ambient_volume = clamped; break;
        case SoundCategory::UI: m_ui_volume = clamped; break;
    }
}

float AudioEngine::channel_volume(SoundCategory category) const {
    switch (category) {
        case SoundCategory::Master: return m_master_volume;
        case SoundCategory::SFX: return m_sfx_volume;
        case SoundCategory::Enemy: return m_enemy_volume;
        case SoundCategory::Ambience: return m_ambient_volume;
        case SoundCategory::UI: return m_ui_volume;
    }
    return 1.0f;
}

void AudioEngine::set_mute_all(bool mute) {
    m_mute_all = mute;
}

float AudioEngine::get_cue_volume_factor(SoundCue cue) const {
    if (m_mute_all) return 0.0f;
    SoundCategory cat = get_sound_category(cue);
    switch (cat) {
        case SoundCategory::SFX: return m_sfx_volume;
        case SoundCategory::Enemy: return m_enemy_volume;
        case SoundCategory::Ambience: return m_ambient_volume;
        case SoundCategory::UI: return m_ui_volume;
        default: return 1.0f;
    }
}

void AudioEngine::set_listener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) {
    std::lock_guard<std::recursive_mutex> lock(m_voice_mutex);
    m_listener.position = position;
    m_listener.forward = (glm::length(forward) > 0.001f) ? glm::normalize(forward) : glm::vec3(0, 0, -1);
    m_listener.up = (glm::length(up) > 0.001f) ? glm::normalize(up) : glm::vec3(0, 1, 0);

    for (auto& voice : m_voices) {
        if (voice.active && voice.is_3d) {
            update_spatial_pan(voice);
        }
    }
}

void AudioEngine::update_spatial_pan(AudioVoice& voice) {
    if (!voice.is_3d) {
        voice.pan_left = 0.707f;
        voice.pan_right = 0.707f;
        voice.distance_lp_alpha = 1.0f;
        return;
    }

    glm::vec3 rel_pos = voice.world_pos - m_listener.position;
    float dist = glm::length(rel_pos);

    // Inverse distance attenuation with reference distance and cavern roll-off
    // Monster echoing screeches and subterranean roars carry further down cavern tunnels (up to 58m)
    float ref_dist = (voice.cue == SoundCue::StalkerEchoScreech || voice.cue == SoundCue::BurrowerRoar || voice.cue == SoundCue::BurrowerGrind) ? 4.0f : 2.5f;
    float max_dist = (voice.cue == SoundCue::StalkerEchoScreech || voice.cue == SoundCue::BurrowerRoar || voice.cue == SoundCue::BurrowerGrind) ? 58.0f : 45.0f;
    float att = 1.0f;
    if (dist > ref_dist) {
        att = ref_dist / (ref_dist + (dist - ref_dist) * 1.15f);
    }
    if (dist >= max_dist) {
        att = 0.0f;
    }

    // Distance-Based Low-Pass Filter (Acoustic Air Absorption & Cave Wall Muffling)
    // As distance increases, high-frequency presence drops, making far threats sound dark, muffled and distant
    float cutoff = std::clamp(6200.0f - std::max(0.0f, dist - ref_dist) * 135.0f, 750.0f, 6500.0f);
    voice.distance_lp_alpha = calc_lp_alpha(cutoff, static_cast<float>(SAMPLE_RATE));

    // Binaural azimuth calculation
    glm::vec3 right = glm::normalize(glm::cross(m_listener.forward, m_listener.up));
    glm::vec3 dir = (dist > 0.001f) ? (rel_pos / dist) : m_listener.forward;
    float dot_right = glm::dot(dir, right); // [-1.0 (full left) .. +1.0 (full right)]

    // Smooth constant-power panning curve
    float angle = (dot_right + 1.0f) * 0.25f * PI; // [0 .. PI/2]
    voice.pan_left = std::cos(angle) * att;
    voice.pan_right = std::sin(angle) * att;
}

void AudioEngine::play_sound_2d(SoundCue cue, float volume, float pitch, bool loop) {
    std::lock_guard<std::recursive_mutex> lock(m_voice_mutex);

    // Find free voice or steal oldest non-looping voice
    int best_slot = -1;
    float oldest_time = -1.0f;

    for (size_t i = 0; i < MAX_VOICES; ++i) {
        if (!m_voices[i].active) {
            best_slot = static_cast<int>(i);
            break;
        }
        if (!m_voices[i].loop && m_voices[i].time > oldest_time) {
            oldest_time = m_voices[i].time;
            best_slot = static_cast<int>(i);
        }
    }

    if (best_slot < 0) best_slot = 0;

    auto& v = m_voices[best_slot];
    v.cue = cue;
    v.active = true;
    v.loop = loop;
    v.time = 0.0f;
    v.volume = std::clamp(volume, 0.0f, 1.5f);
    v.pitch = std::clamp(pitch, 0.25f, 4.0f);
    v.is_3d = false;
    v.pan_left = 0.707f;
    v.pan_right = 0.707f;
    v.current_gain = 0.0f; // Smooth fade-in to eliminate pops
    v.seed = 98765 + static_cast<uint32_t>(best_slot * 1337);

    for (int k = 0; k < 8; ++k) {
        v.phase[k] = 0.0f;
        v.filter_state[k] = 0.0f;
    }
    for (int k = 0; k < 4; ++k) {
        v.param[k] = 0.0f;
    }

    v.sample_cursor = 0.0f;

    // Set duration based on loaded sample or fallback cue synthesis
    size_t cue_idx = static_cast<size_t>(cue);
    float default_dur = get_cue_default_duration(cue);
    if (cue_idx < m_samples.size() && m_samples[cue_idx].loaded) {
        v.duration = std::min(m_samples[cue_idx].duration_seconds, default_dur) / std::max(0.1f, v.pitch);
    } else {
        v.duration = default_dur / std::max(0.1f, v.pitch);
    }

    // Dynamic negative space: Threat cues duck background ambience for visceral contrast
    if (cue == SoundCue::StalkerEchoScreech || cue == SoundCue::StalkerLunge ||
        cue == SoundCue::BurrowerRoar || cue == SoundCue::SeismicTremor ||
        cue == SoundCue::ExplosiveBlast) {
        trigger_ducking(0.28f, 0.75f, 1.35f);
    }
}

void AudioEngine::play_sound_3d(SoundCue cue, const glm::vec3& world_pos, float volume, float pitch) {
    std::lock_guard<std::recursive_mutex> lock(m_voice_mutex);

    int best_slot = -1;
    float oldest_time = -1.0f;

    for (size_t i = 0; i < MAX_VOICES; ++i) {
        if (!m_voices[i].active) {
            best_slot = static_cast<int>(i);
            break;
        }
        if (!m_voices[i].loop && m_voices[i].time > oldest_time) {
            oldest_time = m_voices[i].time;
            best_slot = static_cast<int>(i);
        }
    }

    if (best_slot < 0) best_slot = 0;

    auto& v = m_voices[best_slot];
    v.cue = cue;
    v.active = true;
    v.loop = false;
    v.time = 0.0f;
    v.volume = std::clamp(volume, 0.0f, 1.5f);
    v.pitch = std::clamp(pitch, 0.25f, 4.0f);
    v.is_3d = true;
    v.world_pos = world_pos;
    v.current_gain = 0.0f;
    v.seed = 54321 + static_cast<uint32_t>(best_slot * 333);

    for (int k = 0; k < 8; ++k) {
        v.phase[k] = 0.0f;
        v.filter_state[k] = 0.0f;
    }
    for (int k = 0; k < 4; ++k) {
        v.param[k] = 0.0f;
    }

    v.sample_cursor = 0.0f;

    size_t cue_idx = static_cast<size_t>(cue);
    float default_dur = get_cue_default_duration(cue);
    if (cue_idx < m_samples.size() && m_samples[cue_idx].loaded) {
        v.duration = std::min(m_samples[cue_idx].duration_seconds, default_dur) / std::max(0.1f, v.pitch);
    } else {
        v.duration = default_dur / std::max(0.1f, v.pitch);
    }

    if (cue == SoundCue::StalkerEchoScreech || cue == SoundCue::StalkerLunge ||
        cue == SoundCue::BurrowerRoar || cue == SoundCue::SeismicTremor ||
        cue == SoundCue::ExplosiveBlast) {
        trigger_ducking(0.28f, 0.75f, 1.35f);
    }

    update_spatial_pan(v);
}

void AudioEngine::stop_sound(SoundCue cue) {
    std::lock_guard<std::recursive_mutex> lock(m_voice_mutex);
    for (auto& v : m_voices) {
        if (v.active && v.cue == cue) {
            v.active = false;
        }
    }
}

void AudioEngine::stop_all() {
    std::lock_guard<std::recursive_mutex> lock(m_voice_mutex);
    for (auto& v : m_voices) {
        v.active = false;
    }
    for (int ch = 0; ch < 2; ++ch) {
        m_dc_block_x1[ch] = 0.0f;
        m_dc_block_y1[ch] = 0.0f;
        m_lp_state[ch] = 0.0f;
        m_prev_sample[ch] = 0.0f;
    }
    m_limiter_envelope = 0.0f;
    m_hazard_phase = 0;
    m_hazard_timer = 0.0f;
    m_micro_ambience_timer = 0.0f;
    m_drill_active = false;
    m_ducking_attenuation = 1.0f;
    m_ducking_timer = 0.0f;
}

void AudioEngine::set_current_sector(int sector) {
    m_current_sector = std::clamp(sector, 1, 3);
}

void AudioEngine::set_current_room_type(int room_type) {
    m_current_room_type = room_type;
}

void AudioEngine::trigger_room_micro_event(int room_shape, float volume) {
    trigger_cavern_micro_event(m_current_sector, room_shape, volume);
}

void AudioEngine::trigger_cavern_micro_event(int sector, float volume) {
    trigger_cavern_micro_event(sector, m_current_room_type, volume);
}

void AudioEngine::trigger_cavern_micro_event(int sector, int room_shape, float volume) {
    if (m_ambient_volume <= 0.001f || m_mute_all) return;

    SoundCue cue = SoundCue::CavernDrip;
    float roll = fast_rand(m_ambience_seed);

    if (room_shape >= 0) {
        // Room-specific environmental tailored sound palette matching each archetype
        switch (room_shape) {
            case 9: // MagmaCalderaLake (volcanic molten slag lake, bubbling lava geysers)
                if (roll < 0.45f) cue = SoundCue::LavaBubble;
                else if (roll < 0.80f) cue = SoundCue::ThermalHiss;
                else cue = SoundCue::GeothermalVent;
                break;
            case 5: // FaultLineCrevasse (tectonic rift, sulfurous fissure)
                if (roll < 0.40f) cue = SoundCue::ThermalHiss;
                else if (roll < 0.70f) cue = SoundCue::LavaBubble;
                else cue = SoundCue::CavernGroan;
                break;
            case 7: // RadioactiveCoreSanctuary (irradiated toxic moat & altar)
                if (roll < 0.45f) cue = SoundCue::RadioactiveHum;
                else if (roll < 0.75f) cue = SoundCue::GeigerClick;
                else cue = SoundCue::CrystalChime;
                break;
            case 11: // VoidSingularityRift (zero-g chasm, bottomless void)
                if (roll < 0.45f) cue = SoundCue::GravityDistortion;
                else if (roll < 0.75f) cue = SoundCue::VoidDistortion;
                else cue = SoundCue::VoidWind;
                break;
            case 6: // AbyssalVerticalChasm (24m vertical drop shaft)
                if (roll < 0.45f) cue = SoundCue::VoidWind;
                else if (roll < 0.75f) cue = SoundCue::CavernGroan;
                else cue = SoundCue::VoidDistortion;
                break;
            case 12: // FungoidBioGrotto (bioluminescent alien mushroom grotto)
                if (roll < 0.45f) cue = SoundCue::SporePlop;
                else if (roll < 0.75f) cue = SoundCue::OrganicCreak;
                else cue = SoundCue::CavernDrip;
                break;
            case 13: // LaserDefenseFoundry (automated smelting vats & crane gantries)
                if (roll < 0.40f) cue = SoundCue::IndustrialHum;
                else if (roll < 0.75f) cue = SoundCue::HydraulicExhaust;
                else cue = SoundCue::PebbleSkitter;
                break;
            case 4: // IndustrialVaultBunker (reinforced blast gates & mezzanine catwalks)
                if (roll < 0.45f) cue = SoundCue::IndustrialHum;
                else if (roll < 0.75f) cue = SoundCue::HydraulicExhaust;
                else cue = SoundCue::CavernGroan;
                break;
            case 14: // CrumblingArchCanyon (fragile natural stone arches, falling debris)
                if (roll < 0.45f) cue = SoundCue::PebbleSkitter;
                else if (roll < 0.75f) cue = SoundCue::VoidWind;
                else cue = SoundCue::CavernGroan;
                break;
            case 10: // SpikeTrenchArena (crystalline punji spike beds & high catwalks)
                if (roll < 0.45f) cue = SoundCue::SpikeRattle;
                else if (roll < 0.75f) cue = SoundCue::PebbleSkitter;
                else cue = SoundCue::VoidWind;
                break;
            case 2: // CrystallineGeode (emissive Voidite crystals & stepping stones)
                if (roll < 0.60f) cue = SoundCue::CrystalChime;
                else if (roll < 0.85f) cue = SoundCue::CavernDrip;
                else cue = SoundCue::VoidDistortion;
                break;
            case 1: // MiningPillarHall (massive extraction columns & vaulted ceiling)
                if (roll < 0.40f) cue = SoundCue::CavernGroan;
                else if (roll < 0.75f) cue = SoundCue::CrystalChime;
                else cue = SoundCue::PebbleSkitter;
                break;
            case 3: // TerracedQuarry (stepped quarry pit, loose rubble)
                if (roll < 0.45f) cue = SoundCue::PebbleSkitter;
                else if (roll < 0.75f) cue = SoundCue::CavernGroan;
                else cue = SoundCue::CavernDrip;
                break;
            case 0: // SpawnStagingCavern
            case 8: // ExtractionLandingBay
            default:
                if (roll < 0.40f) cue = SoundCue::CavernDrip;
                else if (roll < 0.75f) cue = SoundCue::IndustrialHum;
                else cue = SoundCue::CavernGroan;
                break;
        }
    } else {
        // Fallback to sector distribution if in corridor / transition tunnels
        if (sector == 2) {
            if (roll < 0.48f) cue = SoundCue::GeothermalVent;
            else if (roll < 0.82f) cue = SoundCue::CavernGroan;
            else cue = SoundCue::CavernDrip;
        } else if (sector >= 3) {
            if (roll < 0.48f) cue = SoundCue::VoidDistortion;
            else if (roll < 0.82f) cue = SoundCue::CavernGroan;
            else cue = SoundCue::CavernDrip;
        } else {
            if (roll < 0.45f) cue = SoundCue::CavernDrip;
            else if (roll < 0.82f) cue = SoundCue::CrystalChime;
            else cue = SoundCue::CavernGroan;
        }
    }

    // Spatialized around listener (4m to 12m radius)
    float angle = fast_rand(m_ambience_seed) * TWO_PI;
    float dist = 4.0f + fast_rand(m_ambience_seed) * 8.0f;
    float y_off = (fast_rand(m_ambience_seed) * 2.0f - 1.0f) * 2.2f;

    glm::vec3 offset(std::cos(angle) * dist, y_off, std::sin(angle) * dist);
    glm::vec3 event_pos = m_listener.position + offset;

    float event_vol = volume * 0.50f;
    if (cue == SoundCue::CrystalChime) event_vol *= 0.80f;
    if (cue == SoundCue::GeothermalVent) event_vol *= 0.85f;
    if (cue == SoundCue::VoidDistortion) event_vol *= 0.75f;
    if (cue == SoundCue::RadioactiveHum) event_vol *= 0.70f;
    if (cue == SoundCue::LavaBubble) event_vol *= 0.85f;

    play_sound_3d(cue, event_pos, event_vol, 0.92f + fast_rand(m_ambience_seed) * 0.16f);
}

void AudioEngine::update_hazard_proximity_audio(float dt, float lava_dist, float rad_level, float spike_dist, float void_dist) {
    if (m_ambient_volume <= 0.001f || m_mute_all) return;

    m_lava_proximity = std::max(0.0f, 1.0f - (lava_dist / 10.0f));
    m_spike_proximity = std::max(0.0f, 1.0f - (spike_dist / 7.0f));
    m_void_proximity = std::max(0.0f, 1.0f - (void_dist / 8.0f));

    m_hazard_audio_timer += dt;
    if (m_hazard_audio_timer < 0.45f) return;
    m_hazard_audio_timer = 0.0f;

    // 1. Molten Lava proximity (< 10m): thermal bubbling pops & hiss
    if (lava_dist < 10.0f) {
        float lava_factor = 1.0f - (lava_dist / 10.0f);
        if (fast_rand(m_ambience_seed) < (0.25f + lava_factor * 0.45f)) {
            SoundCue c = (fast_rand(m_ambience_seed) < 0.60f) ? SoundCue::LavaBubble : SoundCue::ThermalHiss;
            float pan_angle = fast_rand(m_ambience_seed) * TWO_PI;
            glm::vec3 pos = m_listener.position + glm::vec3(std::cos(pan_angle) * lava_dist, -1.0f, std::sin(pan_angle) * lava_dist);
            play_sound_3d(c, pos, 0.40f * lava_factor, 0.90f + fast_rand(m_ambience_seed) * 0.20f);
        }
    }

    // 2. Spike Trench proximity (< 7m): eerie hollow rattle
    if (spike_dist < 7.0f) {
        float spike_factor = 1.0f - (spike_dist / 7.0f);
        if (fast_rand(m_ambience_seed) < (0.20f + spike_factor * 0.40f)) {
            float pan_angle = fast_rand(m_ambience_seed) * TWO_PI;
            glm::vec3 pos = m_listener.position + glm::vec3(std::cos(pan_angle) * spike_dist, -1.2f, std::sin(pan_angle) * spike_dist);
            play_sound_3d(SoundCue::SpikeRattle, pos, 0.32f * spike_factor);
        }
    }

    // 3. Void Abyss proximity (< 8m): gravity warp & hollow wind
    if (void_dist < 8.0f) {
        float void_factor = 1.0f - (void_dist / 8.0f);
        if (fast_rand(m_ambience_seed) < (0.22f + void_factor * 0.45f)) {
            SoundCue c = (fast_rand(m_ambience_seed) < 0.55f) ? SoundCue::GravityDistortion : SoundCue::VoidWind;
            float pan_angle = fast_rand(m_ambience_seed) * TWO_PI;
            glm::vec3 pos = m_listener.position + glm::vec3(std::cos(pan_angle) * void_dist, -2.5f, std::sin(pan_angle) * void_dist);
            play_sound_3d(c, pos, 0.35f * void_factor);
        }
    }

    // 4. Radiation proximity: update radiation level for Geiger clicks
    set_radiation_proximity(rad_level);
}

void AudioEngine::update_biometrics(float health_pct, float threat_proximity) {
    // Health below 35% triggers escalating biometric stress (heartbeat and ragged breathing)
    float hp_stress = (health_pct < 0.35f) ? (0.35f - health_pct) / 0.35f : 0.0f;
    float threat_stress = std::clamp(threat_proximity, 0.0f, 1.0f) * 0.4f;
    m_biometric_stress = std::clamp(hp_stress + threat_stress, 0.0f, 1.0f);
}

void AudioEngine::set_drill_active(bool active, float progress, const glm::vec3& pos) {
    std::lock_guard<std::recursive_mutex> lock(m_voice_mutex);
    m_drill_active = active;
    m_drill_progress = std::clamp(progress, 0.0f, 1.0f);
    m_drill_pos = pos;

    bool found = false;
    for (auto& v : m_voices) {
        if (v.active && v.cue == SoundCue::DrillLoop) {
            found = true;
            if (!active) {
                v.active = false;
            } else {
                v.world_pos = pos;
                v.pitch = 0.90f + m_drill_progress * 0.35f;
                update_spatial_pan(v);
            }
            break;
        }
    }

    if (active && !found) {
        // Trigger drill loop voice
        for (auto& v : m_voices) {
            if (!v.active) {
                v.cue = SoundCue::DrillLoop;
                v.active = true;
                v.loop = true;
                v.time = 0.0f;
                v.duration = 9999.0f;
                v.volume = 0.70f;
                v.pitch = 0.90f + m_drill_progress * 0.35f;
                v.is_3d = true;
                v.world_pos = pos;
                v.current_gain = 0.0f;
                v.sample_cursor = 0.0f;
                update_spatial_pan(v);
                break;
            }
        }
    }
}

void AudioEngine::set_ambient_intensity(float intensity) {
    m_ambient_intensity = std::clamp(intensity, 0.0f, 1.0f);
}

void AudioEngine::set_hazard_phase(int phase) {
    m_hazard_phase = std::clamp(phase, 0, 4);
}

void AudioEngine::set_seismic_rumble(float intensity) {
    m_tremor_intensity = std::clamp(intensity, 0.0f, 1.0f);
    if (intensity > 0.05f) {
        bool playing = false;
        std::lock_guard<std::recursive_mutex> lock(m_voice_mutex);
        for (const auto& v : m_voices) {
            if (v.active && v.cue == SoundCue::SeismicTremor) {
                playing = true;
                break;
            }
        }
        if (!playing) {
            play_sound_2d(SoundCue::SeismicTremor, 0.85f * intensity, 0.85f);
        }
    }
}

void AudioEngine::set_radiation_proximity(float level) {
    m_radiation_proximity = std::clamp(level, 0.0f, 1.5f);
}

int AudioEngine::active_voice_count() const {
    std::lock_guard<std::recursive_mutex> lock(m_voice_mutex);
    int count = 0;
    for (const auto& v : m_voices) {
        if (v.active) count++;
    }
    return count;
}

// ── Voice Procedural Synthesis Implementations ──
float AudioEngine::synth_sample(AudioVoice& voice, float dt) {
    float sample = 0.0f;
    float t = voice.time;
    float dur = voice.duration;

    // Smooth Cosine Envelope Window (guarantees click-free starts and ends)
    float envelope = 1.0f;
    constexpr float ATTACK_TIME = 0.005f; // 5ms attack ramp
    constexpr float RELEASE_TIME = 0.020f;// 20ms release ramp

    if (t < ATTACK_TIME) {
        envelope = 0.5f * (1.0f - std::cos(PI * (t / ATTACK_TIME)));
    } else if (!voice.loop && t > (dur - RELEASE_TIME)) {
        float rel_t = (dur - t) / RELEASE_TIME;
        envelope = 0.5f * (1.0f - std::cos(PI * std::clamp(rel_t, 0.0f, 1.0f)));
    }

    switch (voice.cue) {
        case SoundCue::DrillLoop: {
            // Mechanical rotary motor: base tone + torque load sag + teeth chatter + rock friction
            float load_factor = m_drill_progress;
            float base_freq = (98.0f - 16.0f * load_factor) * voice.pitch;
            // Harmonic motor vibration
            voice.phase[0] += base_freq * dt;
            voice.phase[1] += (base_freq * 2.01f) * dt;
            voice.phase[2] += (base_freq * 3.52f) * dt;
            if (voice.phase[0] > 1.0f) voice.phase[0] -= 1.0f;
            if (voice.phase[1] > 1.0f) voice.phase[1] -= 1.0f;
            if (voice.phase[2] > 1.0f) voice.phase[2] -= 1.0f;

            // Motor body with torque flutter
            float flutter = 1.0f + 0.05f * fast_sin(TWO_PI * 14.0f * t);
            float motor = (0.50f * fast_sin_phase(voice.phase[0]) +
                           0.25f * fast_sin_phase(voice.phase[1]) +
                           0.12f * fast_sin_phase(voice.phase[2])) * flutter;

            // Mechanical teeth cutting pulses (32 Hz chatter with smooth half-sine profile)
            voice.phase[3] += (32.0f + 12.0f * load_factor) * dt;
            if (voice.phase[3] > 1.0f) voice.phase[3] -= 1.0f;
            float chatter_pulse = (voice.phase[3] < 0.35f) ? fast_sin(PI * (voice.phase[3] / 0.35f)) : 0.0f;

            // Granular rock friction noise filtered at 950 Hz (stone grind)
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(950.0f + 350.0f * load_factor, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);

            // High-frequency rock cutting hiss (calibrated to 1750 Hz for warm, non-fatiguing ear comfort)
            float hiss_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float hiss_alpha = calc_lp_alpha(1750.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[1] += hiss_alpha * (hiss_noise - voice.filter_state[1]);

            sample = motor * 0.38f +
                     voice.filter_state[0] * (0.34f + 0.18f * load_factor) +
                     voice.filter_state[1] * (0.05f + 0.08f * load_factor) +
                     chatter_pulse * 0.12f * load_factor;
            break;
        }

        case SoundCue::VoxelHit: {
            // Rapid chisel impact: sharp kinetic strike transient + resonant rock body thud + chip scatter
            float sweep = 380.0f * voice.pitch * std::exp(-t * 30.0f) + 120.0f;
            voice.phase[0] += sweep * dt;
            float body = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 40.0f);
            float click = (fast_rand(voice.seed) * 2.0f - 1.0f) * std::exp(-t * 90.0f);
            // High-frequency acoustic contact transient
            voice.phase[1] += 1400.0f * voice.pitch * dt;
            float transient = std::sin(TWO_PI * voice.phase[1]) * std::exp(-t * 110.0f);
            sample = body * 0.55f + click * 0.25f + transient * 0.20f;
            break;
        }

        case SoundCue::VoxelBreakBasalt: {
            // Dense stone fracture: deep 52Hz concussive thud + multi-tap crumbling gravel spray
            voice.phase[0] += 52.0f * voice.pitch * dt;
            float sub_thud = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 13.0f);

            // Layered granular crumble: primary and secondary debris bursts
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha1 = calc_lp_alpha(550.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha1 * (noise - voice.filter_state[0]);
            float crumble1 = voice.filter_state[0] * std::exp(-t * 9.0f);

            float noise2 = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha2 = calc_lp_alpha(920.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[1] += alpha2 * (noise2 - voice.filter_state[1]);
            float crumble2 = (t > 0.04f) ? voice.filter_state[1] * std::exp(-(t - 0.04f) * 11.0f) * 0.45f : 0.0f;

            sample = sub_thud * 0.50f + crumble1 * 0.35f + crumble2 * 0.20f;
            break;
        }

        case SoundCue::VoxelBreakTitanium: {
            // Industrial metal clang: sharp contact transient + dual inharmonic bell overtones + metallic shimmer
            voice.phase[0] += 480.0f * voice.pitch * dt;
            voice.phase[1] += 1140.0f * voice.pitch * dt;
            voice.phase[2] += 2380.0f * voice.pitch * dt;

            float tone1 = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 4.8f);
            float tone2 = std::sin(TWO_PI * voice.phase[1]) * std::exp(-t * 8.2f);
            float tone3 = std::sin(TWO_PI * voice.phase[2]) * std::exp(-t * 16.0f);

            // Impact snap
            float click = (fast_rand(voice.seed) * 2.0f - 1.0f) * std::exp(-t * 60.0f);
            sample = tone1 * 0.42f + tone2 * 0.32f + tone3 * 0.12f + click * 0.18f;
            break;
        }

        case SoundCue::VoxelBreakVoidite: {
            // Crystalline harmonic shatter: pristine, ethereal chord triad (D5, D6, F#6, A6) with chorused shimmer
            voice.phase[0] += 587.33f * voice.pitch * dt;
            voice.phase[1] += 1174.66f * voice.pitch * dt;
            voice.phase[2] += 1479.98f * voice.pitch * dt;
            voice.phase[3] += 1760.00f * voice.pitch * dt;

            // Slow chorusing vibrato
            float chorus = 1.0f + 0.015f * std::sin(TWO_PI * 6.5f * t);

            float c0 = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 3.5f);
            float c1 = std::sin(TWO_PI * voice.phase[1] * chorus) * std::exp(-t * 4.5f);
            float c2 = std::sin(TWO_PI * voice.phase[2]) * std::exp(-t * 6.0f);
            float c3 = std::sin(TWO_PI * voice.phase[3] * chorus) * std::exp(-t * 7.5f);

            // High shimmer sparkle
            float sparkle_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float sp_alpha = calc_lp_alpha(3800.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += sp_alpha * (sparkle_noise - voice.filter_state[0]);
            float sparkle = voice.filter_state[0] * std::exp(-t * 12.0f) * 0.15f;

            sample = c0 * 0.28f + c1 * 0.32f + c2 * 0.22f + c3 * 0.15f + sparkle;
            break;
        }

        case SoundCue::VoxelBreakBulkhead: {
            // Heavy plate buckle: deep 58Hz punch + 240Hz metallic stress + hydraulic snap
            voice.phase[0] += 58.0f * voice.pitch * dt;
            voice.phase[1] += 240.0f * voice.pitch * dt;
            float punch = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 7.5f);
            float ring = std::sin(TWO_PI * voice.phase[1]) * std::exp(-t * 13.0f);

            // Hydraulic seal decompression hiss
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(1100.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);
            float hiss = voice.filter_state[0] * std::exp(-t * 15.0f);

            sample = punch * 0.55f + ring * 0.30f + hiss * 0.22f;
            break;
        }

        case SoundCue::VoxelBreakRadioactive: {
            // Radioactive mineral shatter with ionizing crackle burst
            voice.phase[0] += 175.0f * voice.pitch * dt;
            float body = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 14.0f);
            // Concentrated Geiger crackle burst
            float crackle = (fast_rand(voice.seed) > 0.88f) ? (fast_rand(voice.seed) * 2.0f - 1.0f) : 0.0f;
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(1800.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);
            sample = body * 0.50f + crackle * 0.38f + voice.filter_state[0] * std::exp(-t * 10.0f) * 0.20f;
            break;
        }

        case SoundCue::BulkheadDeploy: {
            // Hydraulic seal hiss and magnetic clamp thud
            voice.phase[0] += (220.0f - t * 400.0f) * voice.pitch * dt;
            float tone = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 10.0f);
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(900.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);
            sample = tone * 0.5f + voice.filter_state[0] * 0.5f * std::exp(-t * 8.0f);
            break;
        }

        case SoundCue::BulkheadDismantle: {
            // Spring latch release click
            voice.phase[0] += 420.0f * voice.pitch * dt;
            sample = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 25.0f);
            break;
        }

        case SoundCue::StalkerSpotted: {
            // Organic predatory insectoid chatter: 24Hz click-train through dual-formant vocal tract (540Hz, 1450Hz) + FM growl
            voice.phase[0] += 24.0f * dt;
            if (voice.phase[0] > 1.0f) voice.phase[0] -= 1.0f;
            float click_impulse = (voice.phase[0] < 0.12f) ? 1.0f : 0.0f;
            float excitation = click_impulse + (fast_rand(voice.seed) * 2.0f - 1.0f) * 0.25f;

            // Formant 1 (540 Hz pharyngeal cavity)
            float f1 = process_resonator(excitation, 540.0f * voice.pitch, 8.0f, static_cast<float>(SAMPLE_RATE),
                                         voice.filter_state[0], voice.filter_state[1]);
            // Formant 2 (1450 Hz oral/chitin cavity)
            float f2 = process_resonator(excitation, 1450.0f * voice.pitch, 10.0f, static_cast<float>(SAMPLE_RATE),
                                         voice.filter_state[2], voice.filter_state[3]);

            // Low guttural FM carrier growl
            voice.phase[1] += 38.0f * dt;
            float fm_mod = std::sin(TWO_PI * voice.phase[1]) * 55.0f;
            voice.phase[2] += (175.0f + fm_mod) * voice.pitch * dt;
            float growl = std::sin(TWO_PI * voice.phase[2]) * 0.35f;

            float decay = std::exp(-t * 4.2f);
            sample = (f1 * 0.50f + f2 * 0.40f + growl) * decay;
            break;
        }

        case SoundCue::StalkerLunge: {
            // Visceral predatory leap screech: Dissonant two-operator FM shriek + ring-modulated throat rasp + chitin hiss
            float pitch_sweep = 260.0f + 420.0f * std::sin(std::clamp(t / 0.40f, 0.0f, 1.0f) * PI);
            // Throat rasp modulator (88 Hz)
            voice.phase[1] += 88.0f * dt;
            float rasp = std::sin(TWO_PI * voice.phase[1]);

            // FM Carrier
            voice.phase[0] += (pitch_sweep * (1.0f + 0.28f * rasp)) * voice.pitch * dt;
            float screech = std::sin(TWO_PI * voice.phase[0]);

            // Chitin screech noise (strictly low-passed at 2400 Hz for ear protection)
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(2400.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);

            float env = std::exp(-t * 3.4f);
            sample = (screech * 0.58f + voice.filter_state[0] * 0.42f) * env;
            break;
        }

        case SoundCue::StalkerHit: {
            // Claws striking player exo-armor + chitin carapace fracture + wet alien impact
            voice.phase[0] += (120.0f - t * 250.0f) * dt;
            float thud = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 18.0f);

            // Armor scrape and chitin crack
            float scrape_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha1 = calc_lp_alpha(1800.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha1 * (scrape_noise - voice.filter_state[0]);

            // Visceral wet impact squelch (formant around 480 Hz)
            float squelch = process_resonator(scrape_noise, 480.0f, 6.0f, static_cast<float>(SAMPLE_RATE),
                                              voice.filter_state[1], voice.filter_state[2]);

            sample = thud * 0.55f + voice.filter_state[0] * std::exp(-t * 22.0f) * 0.25f + squelch * std::exp(-t * 16.0f) * 0.25f;
            break;
        }

        case SoundCue::StalkerDie: {
            // Sinking void entity collapse: downward frequency drop into sub-bass dissipation hum with particulate vapor hiss
            float fade = std::clamp(1.0f - (t / 0.55f), 0.0f, 1.0f);
            float freq = std::max(28.0f, 380.0f * std::exp(-t * 5.5f));
            voice.phase[0] += freq * dt;
            float tone = std::sin(TWO_PI * voice.phase[0]);

            // Sub-harmonic void hum
            voice.phase[1] += (freq * 0.5f) * dt;
            float sub = std::sin(TWO_PI * voice.phase[1]) * 0.35f;

            // Dissipating particulate hiss (softened to avoid harsh screech)
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(std::max(180.0f, 1600.0f - t * 2800.0f), static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);

            sample = (tone * 0.40f + sub * 0.35f + voice.filter_state[0] * 0.25f) * std::exp(-t * 4.0f) * fade;
            break;
        }

        case SoundCue::StalkerEchoScreech: {
            // Predatory void stalker echo screech: 0.85s duration
            // Rapid pitch sweep (620Hz -> 280Hz) with FM throat rasp and distinct cavern slapback echoes
            float sweep = 620.0f * std::exp(-t * 3.2f) + 280.0f;
            voice.phase[1] += 42.0f * dt;
            float fm_mod = fast_sin(TWO_PI * voice.phase[1]) * 85.0f;
            voice.phase[0] += (sweep + fm_mod) * voice.pitch * dt;
            float direct_shriek = fast_sin(TWO_PI * voice.phase[0]);

            // Filtered chitin hiss noise (LP 2400 Hz for ear safety)
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(2400.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);
            float direct = (direct_shriek * 0.60f + voice.filter_state[0] * 0.35f) * std::exp(-t * 4.2f);

            // Cavern echo reflection tap 1 (140ms delay) with acoustic wall damping
            float echo1 = 0.0f;
            if (t > 0.14f) {
                float te1 = t - 0.14f;
                voice.phase[2] += (sweep * 0.72f) * voice.pitch * dt;
                float e1_tone = fast_sin(TWO_PI * voice.phase[2]);
                float e1_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
                float a1 = calc_lp_alpha(1600.0f, static_cast<float>(SAMPLE_RATE));
                voice.filter_state[1] += a1 * (e1_noise - voice.filter_state[1]);
                echo1 = (e1_tone * 0.55f + voice.filter_state[1] * 0.35f) * std::exp(-te1 * 3.6f) * 0.30f;
            }
            // Cavern echo reflection tap 2 (300ms delay)
            float echo2 = 0.0f;
            if (t > 0.30f) {
                float te2 = t - 0.30f;
                voice.phase[3] += (sweep * 0.50f) * voice.pitch * dt;
                float e2_tone = fast_sin(TWO_PI * voice.phase[3]);
                float e2_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
                float a2 = calc_lp_alpha(1000.0f, static_cast<float>(SAMPLE_RATE));
                voice.filter_state[2] += a2 * (e2_noise - voice.filter_state[2]);
                echo2 = (e2_tone * 0.50f + voice.filter_state[2] * 0.30f) * std::exp(-te2 * 3.0f) * 0.16f;
            }
            sample = direct * 0.70f + echo1 + echo2;
            break;
        }

        case SoundCue::StalkerChitter: {
            // Subtle, terrifying chitinous mandible clicking in the dark (0.28s duration)
            // Multi-burst discrete clicks with bone-dry hollow acoustic resonance
            float click_times[3] = {0.03f, 0.10f, 0.18f};
            float click_tone = 0.0f;
            for (int i = 0; i < 3; ++i) {
                float dt_click = t - click_times[i];
                if (dt_click >= 0.0f && dt_click < 0.045f) {
                    float click_pitch = (3200.0f + static_cast<float>(i * 350)) * voice.pitch;
                    voice.phase[i] += click_pitch * dt;
                    float c_sine = fast_sin(TWO_PI * voice.phase[i]);
                    float c_env = std::exp(-dt_click * 120.0f);
                    click_tone += c_sine * c_env * 0.70f;
                }
            }
            // Filtered mandible scrape noise (bandpass around 3400 Hz)
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float bp = process_resonator(raw_noise, 3400.0f, 4.5f, static_cast<float>(SAMPLE_RATE),
                                         voice.filter_state[0], voice.filter_state[1]);
            sample = (click_tone * 0.75f + bp * 0.25f * std::exp(-t * 3.5f)) * 0.45f;
            break;
        }

        case SoundCue::StalkerHiss: {
            // Low menacing predatory throat hiss (0.65s duration)
            // Shaped breath noise filtered through a 1600 Hz lowpass with 32 Hz throat tremor
            voice.phase[0] += 32.0f * dt;
            float tremolo = 0.75f + 0.25f * fast_sin(TWO_PI * voice.phase[0]);
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(1600.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (raw_noise - voice.filter_state[0]);
            float env = std::min(1.0f, t / 0.05f) * std::exp(-t * 3.8f);
            sample = voice.filter_state[0] * tremolo * env * 0.65f;
            break;
        }

        case SoundCue::BurrowerRoar: {
            // Subterranean tectonic borer roar: tight 0.95s duration
            // Undulating sub-bass (38-54 Hz) + guttural tooth grind + rock displacement
            float roar_freq = 38.0f + 14.0f * fast_sin(t * 7.5f);
            voice.phase[0] += roar_freq * voice.pitch * dt;
            float sub_roar = fast_sin(TWO_PI * voice.phase[0]);

            // Visceral borer tooth overtone (82 Hz)
            voice.phase[1] += (roar_freq * 2.15f) * voice.pitch * dt;
            float overtone = fast_sin(TWO_PI * voice.phase[1]) * 0.40f;

            // Infrasound bedrock punch (24 Hz)
            voice.phase[2] += (roar_freq * 0.58f) * voice.pitch * dt;
            float infrasound = fast_sin(TWO_PI * voice.phase[2]) * 0.30f;

            // Chitin/rock friction noise filtered at 280 Hz
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(280.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);

            // Swelling roar envelope: quick 0.08s rise, punchy 0.95s decay
            float env = (t < 0.08f) ? (t / 0.08f) : std::exp(-(t - 0.08f) * 2.6f);
            sample = std::tanh((sub_roar * 0.45f + overtone * 0.25f + infrasound * 0.22f + voice.filter_state[0] * 0.35f) * 1.25f) * env;
            break;
        }

        case SoundCue::BurrowerGrind: {
            // Mechanical and chitinous rock-grinding friction (115-185 Hz with FM chatter)
            voice.phase[1] += 26.0f * dt;
            float mod = std::sin(TWO_PI * voice.phase[1]) * 40.0f;
            float freq = (140.0f + mod) * voice.pitch;
            voice.phase[0] += freq * dt;
            float tooth_grind = std::sin(TWO_PI * voice.phase[0]);

            // Secondary tooth harmonic
            voice.phase[2] += (freq * 1.85f) * dt;
            float tooth_harmonic = std::sin(TWO_PI * voice.phase[2]) * 0.35f;

            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(880.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);

            // Quick 8ms smooth attack ramp to eliminate initial step slew, decaying cleanly
            float attack = std::min(1.0f, t / 0.008f);
            float env = attack * std::exp(-t * 2.8f);
            sample = (tooth_grind * 0.38f + tooth_harmonic * 0.20f + voice.filter_state[0] * 0.32f) * env;
            break;
        }

        case SoundCue::AmbientCavern: {
            // Atmospheric Subterranean Voidscape (Dredge & Dead Space negative space):
            // 1. Slow 1.5Hz binaural sub-bass breathing bed (30.0 Hz, 31.5 Hz)
            float base_sub = (m_hazard_phase >= 3) ? 26.0f : 30.0f;
            voice.phase[0] += base_sub * dt;
            voice.phase[1] += (base_sub + 1.5f) * dt;
            if (voice.phase[0] > 1.0f) voice.phase[0] -= 1.0f;
            if (voice.phase[1] > 1.0f) voice.phase[1] -= 1.0f;

            float sub1 = fast_sin_phase(voice.phase[0]);
            float sub2 = fast_sin_phase(voice.phase[1]);
            float infrasonic_bed = (sub1 * 0.50f + sub2 * 0.50f) * 0.28f;

            // 2. Tunnel void draft wind: Pink-filtered noise sweeping through a hollow resonant bandpass (280 - 540 Hz)
            voice.phase[3] += 0.08f * dt; // Slow 12.5-second abyssal draft LFO
            if (voice.phase[3] > 1.0f) voice.phase[3] -= 1.0f;
            float draft_freq = 320.0f + 180.0f * fast_sin_phase(voice.phase[3]);

            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            voice.filter_state[0] += 0.12f * (raw_noise - voice.filter_state[0]);
            float draft_wind = process_resonator(voice.filter_state[0], draft_freq, 3.8f,
                                                 static_cast<float>(SAMPLE_RATE),
                                                 voice.filter_state[1], voice.filter_state[2]) * 0.32f;

            // 3. Subtle sub-floor cavern vibration
            float alpha_rumble = calc_lp_alpha(120.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[3] += alpha_rumble * (raw_noise - voice.filter_state[3]);
            float rumble = voice.filter_state[3] * 0.18f;

            // 4. Hazard escalation: minor third dissonant drone fades in during high hazard
            float hazard_drone = 0.0f;
            if (m_hazard_phase >= 2) {
                voice.phase[4] += (base_sub * 1.20f) * dt;
                if (voice.phase[4] > 1.0f) voice.phase[4] -= 1.0f;
                hazard_drone = fast_sin_phase(voice.phase[4]) * (0.05f * m_hazard_phase);
            }

            sample = (infrasonic_bed + draft_wind + rumble + hazard_drone) * m_ambient_intensity;
            break;
        }

        case SoundCue::AmbientSector1: {
            // Sector 1 (Perimeter Drift): Ethereal Crystalline Subterranean Airflow
            // Shimmering quartz harmonic chord: F# minor (185Hz, 277Hz, 440Hz)
            voice.phase[0] += (185.0f + 0.5f * fast_sin_phase(voice.phase[4])) * dt;
            voice.phase[1] += (277.2f + 0.8f * fast_sin_phase(voice.phase[4] + 0.33f)) * dt;
            voice.phase[2] += (440.0f + 1.1f * fast_sin_phase(voice.phase[4] + 0.66f)) * dt;
            voice.phase[4] += 0.05f * dt; // Slow 20s shimmering phase LFO
            if (voice.phase[0] > 1.0f) voice.phase[0] -= 1.0f;
            if (voice.phase[1] > 1.0f) voice.phase[1] -= 1.0f;
            if (voice.phase[2] > 1.0f) voice.phase[2] -= 1.0f;
            if (voice.phase[4] > 1.0f) voice.phase[4] -= 1.0f;

            float crystal_bed = (fast_sin_phase(voice.phase[0]) * 0.40f +
                                 fast_sin_phase(voice.phase[1]) * 0.35f +
                                 fast_sin_phase(voice.phase[2]) * 0.25f) * 0.16f;

            // Porous cavern wind draft (resonant bandpass 520Hz)
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            voice.filter_state[0] += 0.10f * (raw_noise - voice.filter_state[0]);
            float wind = process_resonator(voice.filter_state[0], 520.0f, 4.2f, static_cast<float>(SAMPLE_RATE),
                                           voice.filter_state[1], voice.filter_state[2]) * 0.24f;

            // Gentle quartz crystal ringing overtone (2400Hz resonant peak)
            float crystal_shimmer = process_resonator(voice.filter_state[0], 2400.0f, 12.0f, static_cast<float>(SAMPLE_RATE),
                                                      voice.filter_state[3], voice.filter_state[4]) * 0.08f;

            sample = (crystal_bed + wind + crystal_shimmer) * m_ambient_intensity;
            break;
        }

        case SoundCue::AmbientSector2: {
            // Sector 2 (Volatile Fault): Heavy Geothermal Magma & Basalt Tectonic Strain
            // Binaural boiling magma sub-bass (42.0 Hz, 44.5 Hz) with 2.5Hz pulsating thrum
            voice.phase[0] += 42.0f * dt;
            voice.phase[1] += 44.5f * dt;
            if (voice.phase[0] > 1.0f) voice.phase[0] -= 1.0f;
            if (voice.phase[1] > 1.0f) voice.phase[1] -= 1.0f;
            float sub_thrum = (fast_sin_phase(voice.phase[0]) * 0.50f + fast_sin_phase(voice.phase[1]) * 0.50f) * 0.32f;

            // Pressurized volcanic vent hiss: lowpass noise with periodic 0.3Hz pressure release
            voice.phase[2] += 0.30f * dt;
            if (voice.phase[2] > 1.0f) voice.phase[2] -= 1.0f;
            float vent_pulse = 0.70f + 0.30f * fast_sin_phase(voice.phase[2]);

            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha_vent = calc_lp_alpha(1100.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha_vent * (raw_noise - voice.filter_state[0]);
            float vent_hiss = voice.filter_state[0] * vent_pulse * 0.22f;

            // Tectonic basalt stratum compression (low rumble at 65Hz)
            float alpha_rumble = calc_lp_alpha(85.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[1] += alpha_rumble * (raw_noise - voice.filter_state[1]);
            float basalt_rumble = voice.filter_state[1] * 0.26f;

            sample = (sub_thrum + vent_hiss + basalt_rumble) * m_ambient_intensity;
            break;
        }

        case SoundCue::AmbientSector3: {
            // Sector 3 (Void Cradle): Deep Abyssal Mantle & Gravitational Void Pulse
            // Infrasonic void pulse (26.0 Hz) with ominous 0.15Hz breathing LFO
            voice.phase[3] += 0.15f * dt;
            if (voice.phase[3] > 1.0f) voice.phase[3] -= 1.0f;
            float pulse_env = 0.60f + 0.40f * fast_sin_phase(voice.phase[3]);

            voice.phase[0] += 26.0f * dt;
            if (voice.phase[0] > 1.0f) voice.phase[0] -= 1.0f;
            float void_infrasound = fast_sin_phase(voice.phase[0]) * pulse_env * 0.35f;

            // Dissonant void tritone harmonic drone (36.0 Hz and 50.9 Hz)
            voice.phase[1] += 36.0f * dt;
            voice.phase[2] += 50.9f * dt;
            if (voice.phase[1] > 1.0f) voice.phase[1] -= 1.0f;
            if (voice.phase[2] > 1.0f) voice.phase[2] -= 1.0f;
            float tritone_dread = (fast_sin_phase(voice.phase[1]) * 0.55f + fast_sin_phase(voice.phase[2]) * 0.45f) * 0.20f;

            // Dark dimensional void draft (swept bandpass 240 Hz)
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            voice.filter_state[0] += 0.08f * (raw_noise - voice.filter_state[0]);
            float void_wind = process_resonator(voice.filter_state[0], 240.0f, 6.0f, static_cast<float>(SAMPLE_RATE),
                                                voice.filter_state[1], voice.filter_state[2]) * 0.28f;

            sample = (void_infrasound + tritone_dread + void_wind) * m_ambient_intensity;
            break;
        }

        case SoundCue::SectorArrival1: {
            // Sector 1 Arrival Stinger: Ethereal crystalline descent triad (3.0s duration)
            float note1 = fast_sin(TWO_PI * (440.0f * t)) * std::exp(-t * 1.5f);
            float note2 = (t > 0.25f) ? fast_sin(TWO_PI * (554.4f * (t - 0.25f))) * std::exp(-(t - 0.25f) * 1.4f) : 0.0f;
            float note3 = (t > 0.50f) ? fast_sin(TWO_PI * (659.3f * (t - 0.50f))) * std::exp(-(t - 0.50f) * 1.2f) : 0.0f;
            float sub_bell = fast_sin(TWO_PI * (110.0f * t)) * std::exp(-t * 2.0f) * 0.35f;
            sample = (note1 * 0.35f + note2 * 0.35f + note3 * 0.35f + sub_bell) * std::min(1.0f, t / 0.02f) * 0.70f;
            break;
        }

        case SoundCue::SectorArrival2: {
            // Sector 2 Arrival Stinger: Heavy industrial seismic brass swell (3.0s duration)
            float freq = 110.0f + 45.0f * std::min(1.0f, t / 0.8f);
            voice.phase[0] += freq * dt;
            float brass = fast_sin(TWO_PI * voice.phase[0]) + 0.4f * fast_sin(TWO_PI * voice.phase[0] * 2.0f);
            float sub_punch = fast_sin(TWO_PI * (48.0f * t)) * std::exp(-t * 2.2f) * 0.5f;
            float env = (t < 0.6f) ? (t / 0.6f) : std::exp(-(t - 0.6f) * 1.3f);
            sample = (brass * 0.55f + sub_punch) * env * 0.75f;
            break;
        }

        case SoundCue::SectorArrival3: {
            // Sector 3 Arrival Stinger: Abyssal void strike & sub-bass tension impact (3.5s duration)
            float sub_drop = fast_sin(TWO_PI * (std::max(28.0f, 65.0f - t * 18.0f) * t)) * std::exp(-t * 1.2f);
            float void_strike = fast_sin(TWO_PI * (180.0f * t)) * std::exp(-t * 2.8f);
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(450.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (raw_noise - voice.filter_state[0]);
            float dread_tail = voice.filter_state[0] * std::exp(-t * 0.9f) * 0.35f;
            sample = (sub_drop * 0.55f + void_strike * 0.30f + dread_tail) * 0.80f;
            break;
        }

        case SoundCue::CrystalChime: {
            // Sector 1 Micro-Event: Delicate quartz crystalline harmonic ping (1.2s duration)
            voice.phase[0] += 2640.0f * voice.pitch * dt;
            voice.phase[1] += 3960.0f * voice.pitch * dt;
            float p1 = fast_sin(TWO_PI * voice.phase[0]);
            float p2 = fast_sin(TWO_PI * voice.phase[1]);
            float ping = (p1 * 0.65f + p2 * 0.35f) * std::exp(-t * 4.8f);
            float echo = (t > 0.16f) ? fast_sin(TWO_PI * (2640.0f * 0.75f * (t - 0.16f))) * std::exp(-(t - 0.16f) * 3.5f) * 0.30f : 0.0f;
            sample = (ping * 0.80f + echo * 0.20f) * 0.60f;
            break;
        }

        case SoundCue::GeothermalVent: {
            // Sector 2 Micro-Event: Pressurized volcanic steam / gas exhaust release (1.5s duration)
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float bp = process_resonator(raw_noise, 1250.0f, 3.2f, static_cast<float>(SAMPLE_RATE),
                                         voice.filter_state[0], voice.filter_state[1]);
            float env = (t < 0.10f) ? (t / 0.10f) : std::exp(-(t - 0.10f) * 2.4f);
            sample = bp * env * 0.65f;
            break;
        }

        case SoundCue::VoidDistortion: {
            // Sector 3 Micro-Event: Dimensional acoustic phase-warp / abyssal whisper (1.8s duration)
            float sweep_freq = 150.0f + 35.0f * fast_sin(t * 5.2f);
            voice.phase[0] += sweep_freq * voice.pitch * dt;
            float sine_warp = fast_sin(TWO_PI * voice.phase[0]);
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(800.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (raw_noise - voice.filter_state[0]);
            float env = fast_sin(std::clamp(t / 1.8f, 0.0f, 1.0f) * PI);
            sample = (sine_warp * 0.55f + voice.filter_state[0] * 0.45f) * env * 0.60f;
            break;
        }

        case SoundCue::LavaBubble: {
            // Bubbling viscous thermite lava & volcanic churn (0.70s duration)
            float bubble_pitch = std::max(60.0f, 220.0f * std::exp(-t * 18.0f)) * voice.pitch;
            voice.phase[0] += bubble_pitch * dt;
            float pop = fast_sin(TWO_PI * voice.phase[0]);
            float sub_thrum = fast_sin(TWO_PI * (45.0f * voice.pitch) * t) * 0.35f;
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float bp = process_resonator(raw_noise, 340.0f, 2.5f, static_cast<float>(SAMPLE_RATE),
                                         voice.filter_state[0], voice.filter_state[1]);
            float env = std::min(1.0f, t / 0.02f) * std::exp(-t * 5.5f);
            sample = (pop * 0.55f + sub_thrum + bp * 0.30f) * env * 0.70f;
            break;
        }

        case SoundCue::ThermalHiss: {
            // Searing thermal heat & sulfurous gas release (0.95s duration)
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float bp = process_resonator(raw_noise, 2600.0f * voice.pitch, 3.8f, static_cast<float>(SAMPLE_RATE),
                                         voice.filter_state[0], voice.filter_state[1]);
            float flutter = 0.85f + 0.15f * fast_sin(TWO_PI * 18.0f * t);
            float env = std::min(1.0f, t / 0.05f) * std::exp(-t * 3.8f);
            sample = bp * flutter * env * 0.65f;
            break;
        }

        case SoundCue::RadioactiveHum: {
            // Resonant ionizing electromagnetic radiation hum (1.20s duration)
            voice.phase[0] += 50.0f * voice.pitch * dt;
            voice.phase[1] += 100.0f * voice.pitch * dt;
            voice.phase[2] += 150.0f * voice.pitch * dt;
            float hum = fast_sin(TWO_PI * voice.phase[0]) * 0.45f +
                        fast_sin(TWO_PI * voice.phase[1]) * 0.30f +
                        fast_sin(TWO_PI * voice.phase[2]) * 0.15f;
            float spark = 0.0f;
            if (fast_rand(voice.seed) < 0.08f) {
                spark = (fast_rand(voice.seed) * 2.0f - 1.0f) * 0.35f;
            }
            float env = fast_sin(std::clamp(t / 1.20f, 0.0f, 1.0f) * PI);
            sample = (hum + spark) * env * 0.60f;
            break;
        }

        case SoundCue::VoidWind: {
            // Eerie howling draft echoing through chasms and rifts (1.50s duration)
            float sweep = 240.0f + 70.0f * fast_sin(t * 3.4f);
            voice.phase[0] += sweep * voice.pitch * dt;
            float whistle = fast_sin(TWO_PI * voice.phase[0]) * 0.35f;
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float bp = process_resonator(raw_noise, sweep * 1.5f, 2.8f, static_cast<float>(SAMPLE_RATE),
                                         voice.filter_state[0], voice.filter_state[1]);
            float env = fast_sin(std::clamp(t / 1.50f, 0.0f, 1.0f) * PI);
            sample = (whistle + bp * 0.65f) * env * 0.55f;
            break;
        }

        case SoundCue::GravityDistortion: {
            // Gravitational phase-warp pulse near singularity (1.30s duration)
            float g_freq = std::max(22.0f, 85.0f * std::exp(-t * 2.2f)) * voice.pitch;
            voice.phase[0] += g_freq * dt;
            float sub = fast_sin(TWO_PI * voice.phase[0]);
            voice.phase[1] += (g_freq * 1.52f) * dt;
            float overtone = fast_sin(TWO_PI * voice.phase[1]) * 0.35f;
            float phaser = 0.70f + 0.30f * fast_sin(TWO_PI * 4.2f * t);
            float env = (t < 0.15f) ? (t / 0.15f) : std::exp(-(t - 0.15f) * 2.5f);
            sample = (sub * 0.60f + overtone * 0.40f) * phaser * env * 0.70f;
            break;
        }

        case SoundCue::SporePlop: {
            // Organic fungal spore drop & damp bio-grotto squelch (0.45s duration)
            float drop = 110.0f + 330.0f * std::exp(-t * 35.0f);
            voice.phase[0] += drop * voice.pitch * dt;
            float plink = fast_sin(TWO_PI * voice.phase[0]);
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(650.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (raw_noise - voice.filter_state[0]);
            float env = std::min(1.0f, t / 0.01f) * std::exp(-t * 11.0f);
            sample = (plink * 0.60f + voice.filter_state[0] * 0.40f) * env * 0.65f;
            break;
        }

        case SoundCue::OrganicCreak: {
            // Creaking fibrous mycelium & subterranean bio-stalks (0.85s duration)
            voice.phase[1] += 14.0f * dt;
            float mod = fast_sin(TWO_PI * voice.phase[1]) * 15.0f;
            float freq = (80.0f + mod) * voice.pitch;
            voice.phase[0] += freq * dt;
            float fiber = fast_sin(TWO_PI * voice.phase[0]);
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(420.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (raw_noise - voice.filter_state[0]);
            float env = fast_sin(std::clamp(t / 0.85f, 0.0f, 1.0f) * PI);
            sample = (fiber * 0.50f + voice.filter_state[0] * 0.50f) * env * 0.55f;
            break;
        }

        case SoundCue::IndustrialHum: {
            // 60Hz electrical transformer hum & generator whir (1.40s duration)
            voice.phase[0] += 60.0f * voice.pitch * dt;
            voice.phase[1] += 120.0f * voice.pitch * dt;
            voice.phase[2] += 180.0f * voice.pitch * dt;
            float transformer = fast_sin(TWO_PI * voice.phase[0]) * 0.50f +
                                fast_sin(TWO_PI * voice.phase[1]) * 0.32f +
                                fast_sin(TWO_PI * voice.phase[2]) * 0.18f;
            float env = fast_sin(std::clamp(t / 1.40f, 0.0f, 1.0f) * PI);
            sample = transformer * env * 0.55f;
            break;
        }

        case SoundCue::HydraulicExhaust: {
            // Heavy pneumatic piston & steam relief exhaust (0.75s duration)
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float bp = process_resonator(raw_noise, 1850.0f * voice.pitch, 3.0f, static_cast<float>(SAMPLE_RATE),
                                         voice.filter_state[0], voice.filter_state[1]);
            float env = std::min(1.0f, t / 0.03f) * std::exp(-t * 4.5f);
            sample = bp * env * 0.65f;
            break;
        }

        case SoundCue::PebbleSkitter: {
            // Loose stone gravel & debris skittering from ceiling (0.65s duration)
            float taps = 0.0f;
            float tap_times[4] = {0.05f, 0.16f, 0.32f, 0.48f};
            for (int i = 0; i < 4; ++i) {
                float dt_tap = t - tap_times[i];
                if (dt_tap >= 0.0f && dt_tap < 0.04f) {
                    float tap_pitch = (950.0f + static_cast<float>(i * 320)) * voice.pitch;
                    voice.phase[i] += tap_pitch * dt;
                    taps += fast_sin(TWO_PI * voice.phase[i]) * std::exp(-dt_tap * 90.0f) * 0.45f;
                }
            }
            float raw_noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(1400.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (raw_noise - voice.filter_state[0]);
            float env = std::exp(-t * 4.2f);
            sample = (taps + voice.filter_state[0] * 0.25f) * env * 0.65f;
            break;
        }

        case SoundCue::SpikeRattle: {
            // Hollow bone/metal rattle echoing through spike trench (0.50s duration)
            float click1 = (t < 0.04f) ? fast_sin(TWO_PI * 1450.0f * voice.pitch * t) * std::exp(-t * 60.0f) : 0.0f;
            float click2 = (t > 0.08f && t < 0.15f) ? fast_sin(TWO_PI * 1150.0f * voice.pitch * (t - 0.08f)) * std::exp(-(t - 0.08f) * 55.0f) : 0.0f;
            float click3 = (t > 0.20f && t < 0.30f) ? fast_sin(TWO_PI * 1750.0f * voice.pitch * (t - 0.20f)) * std::exp(-(t - 0.20f) * 60.0f) : 0.0f;
            float env = std::exp(-t * 5.0f);
            sample = (click1 * 0.45f + click2 * 0.35f + click3 * 0.30f) * env * 0.65f;
            break;
        }

        case SoundCue::CavernDrip: {
            // Procedural subterranean moisture droplet acoustic ping:
            // High-frequency mineral drop chirp (1850Hz -> 1200Hz in 30ms) + cave slapback tap
            float chirp = 1200.0f + 650.0f * std::exp(-t * 70.0f);
            voice.phase[0] += chirp * voice.pitch * dt;
            float ping = fast_sin(TWO_PI * voice.phase[0]) * std::exp(-t * 22.0f);

            // Cavern reflection tap (130ms delay) with low-pass damping
            float echo = 0.0f;
            if (t > 0.13f) {
                float te = t - 0.13f;
                voice.phase[1] += 1150.0f * voice.pitch * dt;
                echo = fast_sin(TWO_PI * voice.phase[1]) * std::exp(-te * 16.0f) * 0.28f;
            }
            sample = (ping * 0.75f + echo * 0.25f);
            break;
        }

        case SoundCue::CavernGroan: {
            // Chilling subterranean rock stress creak / micro-fracture release (0.70s duration):
            // High-tension brittle crack transient (0.04s) + hollow acoustic stone groan (56Hz -> 36Hz)
            float crack_transient = (t < 0.04f) ? (fast_rand(voice.seed) * 2.0f - 1.0f) * (1.0f - t / 0.04f) * 0.40f : 0.0f;
            float groan_freq = std::max(34.0f, 56.0f - t * 30.0f);
            voice.phase[0] += groan_freq * voice.pitch * dt;
            float groan = fast_sin(TWO_PI * voice.phase[0]);

            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(220.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);

            // Rapid exponential decay: crisp 0.70s release eliminates lingering drone
            float env = std::exp(-t * 5.2f);
            sample = (crack_transient + groan * 0.52f + voice.filter_state[0] * 0.32f) * env;
            break;
        }

        case SoundCue::SeismicTremor: {
            // Tectonic fault slip & bedrock fracture (1.10s duration):
            // Concussive stratum shear snap (0.08s) + tight subterranean ground shudder (38Hz -> 26Hz)
            float fault_snap = (t < 0.08f) ? (fast_rand(voice.seed) * 2.0f - 1.0f) * (1.0f - t / 0.08f) * 0.55f : 0.0f;
            float rumble_freq = std::max(24.0f, 38.0f - 12.0f * (t / 1.10f));
            voice.phase[0] += rumble_freq * dt;
            float rumble = fast_sin(TWO_PI * voice.phase[0]);

            // Bedrock sub-oscillator (22 Hz)
            voice.phase[1] += 22.0f * dt;
            float sub = fast_sin(TWO_PI * voice.phase[1]) * 0.35f;

            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(170.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);

            // Smooth 1.10s decay without endless drone
            float env = std::exp(-t * 2.85f);
            sample = (fault_snap + rumble * 0.50f + sub + voice.filter_state[0] * 0.35f) * env;
            break;
        }

        case SoundCue::GeigerClick: {
            // Ionizing gas discharge pulse: sharp micro-transient (0.3ms attack) followed by damped 3.2kHz acoustic crackle
            if (t < 0.0003f) {
                sample = (t / 0.0003f);
            } else if (t < 0.004f) {
                float decay = std::exp(-(t - 0.0003f) * 1900.0f);
                float crackle = (fast_rand(voice.seed) * 2.0f - 1.0f) * 0.40f;
                voice.phase[0] += 3200.0f * dt;
                float ping = fast_sin(TWO_PI * voice.phase[0]) * 0.60f;
                sample = (ping + crackle) * decay;
            } else {
                sample = 0.0f;
            }
            break;
        }

        case SoundCue::BeaconSiren: {
            // Harmonic emergency extraction beacon acoustic ping (0.35s duration):
            // Eerie sub-surface industrial sonar chime (520Hz fundamental + 780Hz harmonic overtone) with gentle ring-off
            voice.phase[0] += 520.0f * dt;
            voice.phase[1] += 780.0f * dt;
            float tone1 = fast_sin(TWO_PI * voice.phase[0]);
            float tone2 = fast_sin(TWO_PI * voice.phase[1]) * 0.45f;

            // Soft cavern acoustic ping envelope
            float env = std::exp(-t * 9.5f);
            sample = (tone1 * 0.65f + tone2 * 0.35f) * env;
            break;
        }

        case SoundCue::EvacTouchdown: {
            // Heavy evacuation pod arrival: 1.60s duration
            // Compressed retro-thruster hiss burst + hydraulic clamp locking clang at t = 0.5s
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(420.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);

            voice.phase[0] += 52.0f * dt;
            float low_thud = fast_sin(TWO_PI * voice.phase[0]);

            // Industrial locking clamp clang at t = 0.5s
            float clamp_clang = 0.0f;
            if (t > 0.50f) {
                float tc = t - 0.50f;
                voice.phase[1] += 440.0f * dt;
                voice.phase[2] += 880.0f * dt;
                clamp_clang = (fast_sin(TWO_PI * voice.phase[1]) * 0.60f +
                               fast_sin(TWO_PI * voice.phase[2]) * 0.35f) * std::exp(-tc * 16.0f) * 0.55f;
            }

            sample = (voice.filter_state[0] * 0.60f + low_thud * 0.35f) * std::exp(-t * 1.6f) + clamp_clang;
            break;
        }

        case SoundCue::Footstep: {
            // Subterranean boot strike: Dual-phase transient (heel thump at 68Hz + toe grit crunch)
            voice.phase[0] += 68.0f * dt;
            float heel_thump = fast_sin(TWO_PI * voice.phase[0]) * std::exp(-t * 32.0f);

            // Toe grit noise starting slightly after heel
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(520.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);
            float grit = voice.filter_state[0] * std::exp(-t * 40.0f);

            sample = heel_thump * 0.65f + grit * 0.35f;
            break;
        }

        case SoundCue::Jump: {
            // Pneumatic suit thruster hiss puff
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(1500.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);
            sample = voice.filter_state[0] * std::exp(-t * 14.0f);
            break;
        }

        case SoundCue::Land: {
            // Heavy boots landing on cavern floor: mass shockwave + suit suspension hiss
            voice.phase[0] += 65.0f * dt;
            float thud = fast_sin(TWO_PI * voice.phase[0]) * std::exp(-t * 15.0f);

            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(750.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);
            float suspension = voice.filter_state[0] * std::exp(-t * 18.0f);

            sample = thud * 0.70f + suspension * 0.30f;
            break;
        }

        case SoundCue::SonarPulse: {
            // Sonar chirp sweep (640Hz -> 250Hz) with multi-tap cavern reverb reflections
            float sweep = std::max(240.0f, 640.0f - t * 470.0f);
            voice.phase[0] += sweep * dt;
            float ping = fast_sin(TWO_PI * voice.phase[0]) * std::exp(-t * 3.4f);

            // Reverb echo simulation (tap 1 at 150ms, tap 2 at 320ms)
            voice.phase[1] += (sweep * 0.52f) * dt;
            float echo1 = (t > 0.15f) ? (fast_sin(TWO_PI * voice.phase[1]) * std::exp(-(t - 0.15f) * 2.6f) * 0.32f) : 0.0f;
            voice.phase[2] += (sweep * 0.36f) * dt;
            float echo2 = (t > 0.32f) ? (fast_sin(TWO_PI * voice.phase[2]) * std::exp(-(t - 0.32f) * 2.0f) * 0.18f) : 0.0f;

            sample = ping * 0.70f + echo1 + echo2;
            break;
        }

        case SoundCue::ExplosiveBlast: {
            // Massive concussive explosion: supersonic shockwave transient + sub-bass displacement thump + saturated cavern rumble
            float shockwave = (t < 0.003f) ? (1.0f - t / 0.003f) : 0.0f;

            voice.phase[0] += 42.0f * std::exp(-t * 1.8f) * dt;
            float sub = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 2.2f);

            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(480.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);
            float rumble = voice.filter_state[0] * std::exp(-t * 1.6f);

            sample = std::tanh(shockwave * 0.60f + sub * 1.25f + rumble * 1.05f);
            break;
        }

        case SoundCue::TacticalBarricade: {
            // Vanguard deployable fortress hiss and hydraulic anchor lock
            voice.phase[0] += 140.0f * dt;
            float punch = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 10.0f);
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(1100.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);
            sample = punch * 0.6f + voice.filter_state[0] * std::exp(-t * 12.0f) * 0.4f;
            break;
        }

        case SoundCue::TacticalOvercharge: {
            // Sonic kinetic dash release: sweeping jet pulse
            float sweep = 280.0f + t * 900.0f;
            voice.phase[0] += sweep * dt;
            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(2200.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);
            sample = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 7.5f) * 0.7f + voice.filter_state[0] * std::exp(-t * 10.0f) * 0.3f;
            break;
        }

        case SoundCue::UIBlip: {
            // Pentatonic navigation tone (D5 - 587.33 Hz)
            voice.phase[0] += 587.33f * dt;
            sample = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 35.0f);
            break;
        }

        case SoundCue::UIUpgrade: {
            // Ascending major chord triad (C5 -> E5 -> G5)
            float freq = (t < 0.10f) ? 523.25f : (t < 0.20f) ? 659.25f : 783.99f;
            voice.phase[0] += freq * dt;
            sample = std::sin(TWO_PI * voice.phase[0]) * std::exp(-std::fmod(t, 0.10f) * 12.0f);
            break;
        }

        case SoundCue::DamageWarning: {
            // Visceral cardiovascular alarm: dual-pulse heartbeat thump (systolic 52Hz + diastolic 44Hz)
            float beat_phase = std::fmod(t, 0.40f);
            float beat_sample = 0.0f;
            if (beat_phase < 0.12f) {
                voice.phase[0] += 52.0f * dt;
                beat_sample = std::sin(TWO_PI * voice.phase[0]) * std::exp(-beat_phase * 24.0f);
            } else if (beat_phase >= 0.15f && beat_phase < 0.28f) {
                float p2 = beat_phase - 0.15f;
                voice.phase[1] += 44.0f * dt;
                beat_sample = std::sin(TWO_PI * voice.phase[1]) * std::exp(-p2 * 26.0f) * 0.70f;
            }
            sample = beat_sample;
            break;
        }

        case SoundCue::PlasmaFire: {
            // Coherent plasma bolt shot: fast punchy pitch sweep (1400Hz -> 180Hz) + sub thump + magnetic coil sizzle
            float sweep = 180.0f + 1200.0f * std::exp(-t * 38.0f);
            voice.phase[0] += sweep * voice.pitch * dt;
            float bolt = std::sin(TWO_PI * voice.phase[0]);

            voice.phase[1] += (sweep * 0.45f) * voice.pitch * dt;
            float sub = std::sin(TWO_PI * voice.phase[1]) * 0.35f;

            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(3200.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);

            sample = (bolt * 0.62f + sub * 0.38f + voice.filter_state[0] * 0.22f) * std::exp(-t * 18.0f);
            break;
        }

        case SoundCue::PlasmaHit: {
            // High-energy thermal plasma impact on alien armor or rock: high-frequency impact ping + thermal sizzle
            voice.phase[0] += 420.0f * voice.pitch * dt;
            float ping = std::sin(TWO_PI * voice.phase[0]) * std::exp(-t * 26.0f);

            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(2400.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);

            sample = ping * 0.52f + voice.filter_state[0] * std::exp(-t * 30.0f) * 0.48f;
            break;
        }

        case SoundCue::ScattergunFire: {
            // Demolitionist heavy magma scattergun: concussive deep thump (240Hz -> 48Hz) + gritty shrapnel burst
            float sweep = 48.0f + 200.0f * std::exp(-t * 24.0f);
            voice.phase[0] += sweep * voice.pitch * dt;
            float boom = std::sin(TWO_PI * voice.phase[0]) * 0.65f;

            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(1800.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);

            // Add mechanical ratchet click at tail with smooth cosine envelope
            float click = 0.0f;
            if (t > 0.15f && t < 0.22f) {
                float click_t = (t - 0.15f) / 0.07f;
                voice.phase[1] += 1200.0f * dt;
                float click_window = fast_sin(PI * click_t);
                click = std::sin(TWO_PI * voice.phase[1]) * 0.28f * click_window;
            }

            sample = (boom + voice.filter_state[0] * 0.45f) * std::exp(-t * 11.0f) + click;
            break;
        }

        case SoundCue::RailgunFire: {
            // Scout needler railgun: supersonic electromagnetic whip crack (2800Hz -> 440Hz) + high-frequency inductive hum
            float sweep = 440.0f + 2400.0f * std::exp(-t * 45.0f);
            voice.phase[0] += sweep * voice.pitch * dt;
            float crack = std::sin(TWO_PI * voice.phase[0]);

            // Inductive harmonic coil ring at 880Hz
            voice.phase[1] += 880.0f * voice.pitch * dt;
            float hum = std::sin(TWO_PI * voice.phase[1]) * std::exp(-t * 8.0f) * 0.30f;

            float noise = (fast_rand(voice.seed) * 2.0f - 1.0f);
            float alpha = calc_lp_alpha(4200.0f, static_cast<float>(SAMPLE_RATE));
            voice.filter_state[0] += alpha * (noise - voice.filter_state[0]);

            sample = (crack * 0.60f + hum + voice.filter_state[0] * 0.25f) * std::exp(-t * 14.0f);
            break;
        }

        default:
            sample = 0.0f;
            break;
    }

    voice.time += dt;
    if (!voice.loop && voice.time >= dur) {
        voice.active = false;
    }

    return sample * envelope * voice.volume;
}

void AudioEngine::render_mix(float* output_interleaved, size_t num_frames) {
    std::fill_n(output_interleaved, num_frames * NUM_CHANNELS, 0.0f);

    if (m_mute_all || m_master_volume <= 0.0001f) {
        return; // Silent mute: zero CPU synthesis overhead
    }

    constexpr float dt = 1.0f / static_cast<float>(SAMPLE_RATE);

    // Dynamic Hazard & Proximity Geiger Counter Click Generation (Phases 1 to 4 and Proximity to Radioactive Ores)
    float phase_rad = (m_hazard_phase == 1) ? 0.20f :
                      (m_hazard_phase == 2) ? 0.45f :
                      (m_hazard_phase == 3) ? 0.75f :
                      (m_hazard_phase == 4) ? 0.95f : 0.0f;
    float effective_radiation = std::clamp(std::max(m_radiation_proximity, phase_rad), 0.0f, 1.0f);

    if (effective_radiation > 0.015f && m_ambient_volume > 0.001f) {
        float buffer_seconds = static_cast<float>(num_frames) * dt;
        m_geiger_timer += buffer_seconds;

        // Poisson click rate lambda: from 1.5 Hz (sporadic clicks) to 85 Hz (frantic chattering crackle)
        float lambda = 1.5f + effective_radiation * 80.0f;
        if (m_next_geiger_interval <= 0.0001f) {
            float u = std::max(0.002f, fast_rand(m_geiger_seed));
            m_next_geiger_interval = std::clamp(-std::log(u) / lambda, 0.006f, 2.5f / lambda);
        }

        int clicks_generated = 0;
        while (m_geiger_timer >= m_next_geiger_interval && clicks_generated < 8) {
            m_geiger_timer -= m_next_geiger_interval;
            clicks_generated++;

            float click_vol = 0.35f + effective_radiation * 0.45f;
            float click_pitch = 0.93f + fast_rand(m_geiger_seed) * 0.20f;
            play_sound_2d(SoundCue::GeigerClick, click_vol, click_pitch);

            float u = std::max(0.002f, fast_rand(m_geiger_seed));
            m_next_geiger_interval = std::clamp(-std::log(u) / lambda, 0.006f, 2.5f / lambda);
        }
    } else {
        m_geiger_timer = 0.0f;
    }

    // Dynamic Cavern Micro-Events (Procedural Room-Specific Ambience & Tectonic Groans)
    if (m_ambient_volume > 0.001f && m_ambient_intensity > 0.05f) {
        m_micro_ambience_timer += static_cast<float>(num_frames) * dt;
        float amb_interval = (m_hazard_phase >= 2) ? 6.5f : 9.5f;
        if (m_micro_ambience_timer >= amb_interval) {
            m_micro_ambience_timer = 0.0f;
            trigger_cavern_micro_event(m_current_sector, m_current_room_type, 0.38f * m_ambient_intensity);
        }
    }

    // Dynamic Threat Ducking (Dead Space / Alien Isolation negative space)
    float buffer_dt = dt * static_cast<float>(num_frames);
    if (m_ducking_timer > 0.0f) {
        if (buffer_dt <= m_ducking_timer) {
            m_ducking_timer -= buffer_dt;
            m_ducking_attenuation += (m_ducking_target - m_ducking_attenuation) * std::min(1.0f, 16.0f * buffer_dt);
        } else {
            float remain_dt = buffer_dt - m_ducking_timer;
            m_ducking_timer = 0.0f;
            m_ducking_attenuation += (1.0f - m_ducking_attenuation) * std::min(1.0f, m_ducking_recovery_rate * remain_dt);
            if (m_ducking_attenuation > 0.999f) m_ducking_attenuation = 1.0f;
        }
    } else if (m_ducking_attenuation < 1.0f) {
        m_ducking_attenuation += (1.0f - m_ducking_attenuation) * std::min(1.0f, m_ducking_recovery_rate * buffer_dt);
        if (m_ducking_attenuation > 0.999f) m_ducking_attenuation = 1.0f;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(m_voice_mutex);

        for (auto& voice : m_voices) {
            if (!voice.active) continue;

            float category_mult = get_cue_volume_factor(voice.cue);
            if (category_mult <= 0.0001f) {
                // Channel is muted/turned down: advance timeline without rendering
                voice.time += dt * static_cast<float>(num_frames);
                if (!voice.loop && voice.time >= voice.duration) {
                    voice.active = false;
                }
                continue;
            }

            // Duck ambient / drill noise when threat stings occur
            if (get_sound_category(voice.cue) == SoundCategory::Ambience || voice.cue == SoundCue::DrillLoop) {
                category_mult *= m_ducking_attenuation;
            }

            size_t cue_idx = static_cast<size_t>(voice.cue);
            bool use_sample = (cue_idx < m_samples.size() && m_samples[cue_idx].loaded);
            const auto* sample_ptr = use_sample ? &m_samples[cue_idx] : nullptr;

            for (size_t f = 0; f < num_frames; ++f) {
                float sample_l = 0.0f;
                float sample_r = 0.0f;

                if (sample_ptr && sample_ptr->frame_count > 0) {
                    float pos = voice.sample_cursor;
                    size_t idx0 = static_cast<size_t>(pos);
                    size_t idx1 = idx0 + 1;
                    float frac = pos - static_cast<float>(idx0);

                    if (idx0 < sample_ptr->frame_count) {
                        if (idx1 >= sample_ptr->frame_count) {
                            idx1 = voice.loop ? 0 : idx0;
                        }
                        sample_l = sample_ptr->data[idx0 * 2] * (1.0f - frac) + sample_ptr->data[idx1 * 2] * frac;
                        sample_r = sample_ptr->data[idx0 * 2 + 1] * (1.0f - frac) + sample_ptr->data[idx1 * 2 + 1] * frac;
                    }

                    voice.time += dt;
                    float envelope = 1.0f;
                    constexpr float ATTACK_TIME = 0.003f;
                    if (voice.time < ATTACK_TIME) {
                        envelope = 0.5f * (1.0f - std::cos(PI * (voice.time / ATTACK_TIME)));
                    } else if (!voice.loop && voice.time > voice.duration - 0.008f) {
                        float tail = std::max(0.0f, (voice.duration - voice.time) / 0.008f);
                        envelope = 0.5f * (1.0f - std::cos(PI * tail));
                    }
                    sample_l *= voice.volume * envelope;
                    sample_r *= voice.volume * envelope;

                    voice.sample_cursor += voice.pitch;
                    if (voice.sample_cursor >= static_cast<float>(sample_ptr->frame_count) || (!voice.loop && voice.time >= voice.duration)) {
                        if (voice.loop) {
                            voice.sample_cursor = std::fmod(voice.sample_cursor, static_cast<float>(sample_ptr->frame_count));
                        } else {
                            voice.active = false;
                        }
                    }
                } else {
                    float s = synth_sample(voice, dt);
                    sample_l = s;
                    sample_r = s;
                }

                // Distance low-pass filter for 3D sounds (air absorption & cavern wall muffling)
                if (voice.is_3d && voice.distance_lp_alpha < 0.99f) {
                    voice.distance_filter_state[0] += voice.distance_lp_alpha * (sample_l - voice.distance_filter_state[0]);
                    sample_l = voice.distance_filter_state[0];
                    voice.distance_filter_state[1] += voice.distance_lp_alpha * (sample_r - voice.distance_filter_state[1]);
                    sample_r = voice.distance_filter_state[1];
                }

                output_interleaved[f * 2 + 0] += sample_l * voice.pan_left * category_mult;
                output_interleaved[f * 2 + 1] += sample_r * voice.pan_right * category_mult;

                if (!voice.active) break;
            }
        }
    }

    // Real-Time Biometric Audio: Heartbeat & Ragged Helmet Breathing under low health or high stress
    if (m_biometric_stress > 0.02f && !m_mute_all && m_sfx_volume > 0.01f) {
        float bpm = 68.0f + 82.0f * m_biometric_stress; // 68 BPM to 150 BPM
        float beat_freq = bpm / 60.0f;
        float breath_freq = 0.28f + 0.24f * m_biometric_stress; // 17 to 31 breaths/min
        float breath_lp = calc_lp_alpha(650.0f, static_cast<float>(SAMPLE_RATE));

        float bio_gain = m_biometric_stress * m_sfx_volume * 0.28f;

        for (size_t f = 0; f < num_frames; ++f) {
            m_heartbeat_phase += beat_freq * dt;
            if (m_heartbeat_phase >= 1.0f) {
                m_heartbeat_phase -= 1.0f;
            }

            // Dual-pulse "lub-dub" cardiac envelope:
            // Pulse 1 ("Lub"): phase in [0.0, 0.16] of cycle
            // Pulse 2 ("Dub"): phase in [0.22, 0.36] of cycle
            float heart_sample = 0.0f;
            if (m_heartbeat_phase < 0.16f) {
                float t_pulse = m_heartbeat_phase / 0.16f;
                float env = std::sin(PI * t_pulse);
                float tone = std::sin(TWO_PI * 46.0f * m_heartbeat_phase * (60.0f / bpm)) +
                             0.35f * std::sin(TWO_PI * 92.0f * m_heartbeat_phase * (60.0f / bpm));
                heart_sample += tone * env * 0.85f;
            } else if (m_heartbeat_phase >= 0.22f && m_heartbeat_phase < 0.36f) {
                float t_pulse = (m_heartbeat_phase - 0.22f) / 0.14f;
                float env = std::sin(PI * t_pulse);
                float tone = std::sin(TWO_PI * 58.0f * (m_heartbeat_phase - 0.22f) * (60.0f / bpm)) +
                             0.25f * std::sin(TWO_PI * 116.0f * (m_heartbeat_phase - 0.22f) * (60.0f / bpm));
                heart_sample += tone * env * 0.65f;
            }

            // Respiration: ragged air turbulence in suit helmet
            m_breath_phase += breath_freq * dt;
            if (m_breath_phase >= 1.0f) {
                m_breath_phase -= 1.0f;
            }
            float breath_env = 0.5f * (1.0f - std::cos(TWO_PI * m_breath_phase)); // smooth in-out wave
            float breath_noise = (fast_rand(m_breath_seed) * 2.0f - 1.0f);
            m_breath_filter_state += breath_lp * (breath_noise - m_breath_filter_state);
            float breath_sample = m_breath_filter_state * breath_env * 0.35f;

            float bio_total = (heart_sample + breath_sample) * bio_gain;
            output_interleaved[f * 2 + 0] += bio_total;
            output_interleaved[f * 2 + 1] += bio_total;
        }
    }

    // Apply the Ear Safety & Hearing Protection Mastering Chain
    apply_mastering_chain(output_interleaved, num_frames);
}

// ── Multi-Stage Ear Safety & Hearing Protection Mastering Chain ──
void AudioEngine::apply_mastering_chain(float* buffer, size_t num_frames) {
    constexpr float LP_CUTOFF = 6500.0f; // Damps harsh frequencies above 6.5 kHz
    float lp_alpha = calc_lp_alpha(LP_CUTOFF, static_cast<float>(SAMPLE_RATE));

    constexpr float PEAK_LIMIT_THRESHOLD = 0.891f; // -1.0 dBFS ceiling
    constexpr float MAX_ALLOWED_SLEW = 0.15f;      // Max sample-to-sample jump (click elimination)

    float local_peak = 0.0f;
    float max_slew = 0.0f;
    int clipped = 0;
    float hf_energy = 0.0f;
    float total_energy = 0.0f;

    for (size_t f = 0; f < num_frames; ++f) {
        for (int ch = 0; ch < 2; ++ch) {
            float s = buffer[f * 2 + ch] * m_master_volume;

            // Sanity check: eliminate any accidental NaN or Inf values immediately
            if (std::isnan(s) || std::isinf(s)) {
                s = 0.0f;
            }

            // 1. DC Blocking High-Pass Filter (eliminates speaker offset pop)
            float dc_in = s;
            s = dc_in - m_dc_block_x1[ch] + 0.995f * m_dc_block_y1[ch];
            m_dc_block_x1[ch] = dc_in;
            m_dc_block_y1[ch] = s;

            // 2. High-Frequency De-Harshing Low-Pass Filter
            m_lp_state[ch] += lp_alpha * (s - m_lp_state[ch]);
            float filtered = m_lp_state[ch];
            float diff = s - filtered;
            hf_energy += diff * diff;
            total_energy += s * s;
            s = filtered;

            // 3. Analog-style Soft Saturation (Hyperbolic Tangent Curve)
            // Even if 10 explosions and 20 stalkers stack simultaneously,
            // this smoothly compresses into warm saturation rather than digital harshness
            s = std::tanh(s * 0.90f);

            // 4. Fast-Attack Master Peak Limiter with smooth release
            float abs_s = std::abs(s);
            if (abs_s > m_limiter_envelope) {
                m_limiter_envelope += 0.08f * (abs_s - m_limiter_envelope); // Fast 1ms attack
            } else {
                m_limiter_envelope += 0.0003f * (abs_s - m_limiter_envelope); // 100ms release
            }

            if (m_limiter_envelope > PEAK_LIMIT_THRESHOLD) {
                s *= (PEAK_LIMIT_THRESHOLD / m_limiter_envelope);
            }

            // 5. Absolute Master Safety Hard Clamp at -1.0 dBFS (0.92 max)
            if (s > 0.92f) { s = 0.92f; clipped++; }
            if (s < -0.92f) { s = -0.92f; clipped++; }

            // 6. Final Slew-Rate Limiter (Prevents sudden instantaneous step clicks / pops)
            float delta = s - m_prev_sample[ch];
            if (std::abs(delta) > MAX_ALLOWED_SLEW) {
                s = m_prev_sample[ch] + std::copysign(MAX_ALLOWED_SLEW, delta);
            }
            max_slew = std::max(max_slew, std::abs(s - m_prev_sample[ch]));
            m_prev_sample[ch] = s;

            local_peak = std::max(local_peak, std::abs(s));
            buffer[f * 2 + ch] = s;
        }
    }

    // Telemetry updates
    m_safety_metrics.peak_amplitude = std::max(m_safety_metrics.peak_amplitude, local_peak);
    m_safety_metrics.max_slew_rate = std::max(m_safety_metrics.max_slew_rate, max_slew);
    m_safety_metrics.clipped_samples += clipped;
    if (local_peak > 0.00001f) {
        m_safety_metrics.peak_dbfs = 20.0f * std::log10(local_peak);
    }
    if (total_energy > 0.00001f) {
        m_safety_metrics.high_frequency_ratio = hf_energy / total_energy;
    }
}

void AudioEngine::render_mix_i16(int16_t* output_interleaved, size_t num_frames) {
    constexpr size_t STACK_FRAMES = 1024;
    float float_buf[STACK_FRAMES * 2];

    size_t frames_rendered = 0;
    while (frames_rendered < num_frames) {
        size_t chunk = std::min(num_frames - frames_rendered, STACK_FRAMES);
        render_mix(float_buf, chunk);

        for (size_t i = 0; i < chunk * 2; ++i) {
            float val = float_buf[i] * 32767.0f;
            val = std::clamp(val, -32767.0f, 32767.0f);
            output_interleaved[(frames_rendered * 2) + i] = static_cast<int16_t>(val);
        }
        frames_rendered += chunk;
    }
}

AudioEngine::SafetyMetrics AudioEngine::get_and_reset_safety_metrics() {
    SafetyMetrics copy = m_safety_metrics;
    m_safety_metrics = SafetyMetrics{};
    return copy;
}

std::vector<float> AudioEngine::render_offline_samples(float duration_seconds) {
    size_t total_frames = static_cast<size_t>(duration_seconds * static_cast<float>(SAMPLE_RATE));
    std::vector<float> buffer(total_frames * NUM_CHANNELS, 0.0f);
    render_mix(buffer.data(), total_frames);
    return buffer;
}

// ── Platform Hardware Integration (Windows waveOut) ──
#ifdef _WIN32
void AudioEngine::init_platform_audio() {
    WAVEFORMATEX wfx{};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = NUM_CHANNELS;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    m_platform->h_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_platform->h_event) {
        VF_LOG_INFO("AudioEngine", "Headless/mock audio mode active (Event creation failed).");
        m_hardware_active = false;
        return;
    }

    MMRESULT res = waveOutOpen(&m_platform->h_wave_out, WAVE_MAPPER, &wfx,
                              (DWORD_PTR)m_platform->h_event, 0, CALLBACK_EVENT);
    if (res != MMSYSERR_NOERROR) {
        VF_LOG_INFO("AudioEngine", "Headless/mock audio mode active (waveOutOpen error).");
        CloseHandle(m_platform->h_event);
        m_platform->h_event = nullptr;
        m_hardware_active = false;
        return;
    }

    // Allocate ring buffers
    for (size_t i = 0; i < PlatformData::NUM_BUFFERS; ++i) {
        m_platform->buffers[i].resize(PlatformData::BUFFER_FRAMES * NUM_CHANNELS, 0);
        auto& hdr = m_platform->headers[i];
        hdr.lpData = reinterpret_cast<LPSTR>(m_platform->buffers[i].data());
        hdr.dwBufferLength = static_cast<DWORD>(m_platform->buffers[i].size() * sizeof(int16_t));
        hdr.dwFlags = 0;
        hdr.dwLoops = 0;
        waveOutPrepareHeader(m_platform->h_wave_out, &hdr, sizeof(WAVEHDR));
    }

    m_platform->running = true;
    m_hardware_active = true;

    // Launch dedicated low-latency audio worker thread
    m_platform->audio_thread = std::thread([this]() {
        size_t current_buffer = 0;

        // Pre-fill and prime initial buffers
        for (size_t i = 0; i < PlatformData::NUM_BUFFERS; ++i) {
            render_mix_i16(m_platform->buffers[i].data(), PlatformData::BUFFER_FRAMES);
            waveOutWrite(m_platform->h_wave_out, &m_platform->headers[i], sizeof(WAVEHDR));
        }

        while (m_platform->running) {
            DWORD wait_res = WaitForSingleObject(m_platform->h_event, 50);
            if (!m_platform->running) break;

            if (wait_res == WAIT_OBJECT_0) {
                // Find completed buffer headers and refill them
                for (size_t i = 0; i < PlatformData::NUM_BUFFERS; ++i) {
                    if (m_platform->headers[i].dwFlags & WHDR_DONE) {
                        render_mix_i16(m_platform->buffers[i].data(), PlatformData::BUFFER_FRAMES);
                        waveOutWrite(m_platform->h_wave_out, &m_platform->headers[i], sizeof(WAVEHDR));
                    }
                }
            }
        }
    });

    VF_LOG_INFO("AudioEngine", "Hardware audio device initialized (44100Hz 16-bit Stereo PCM, waveOut).");
}

void AudioEngine::shutdown_platform_audio() {
    if (m_platform->running) {
        m_platform->running = false;
        if (m_platform->h_event) {
            SetEvent(m_platform->h_event);
        }
        if (m_platform->audio_thread.joinable()) {
            m_platform->audio_thread.join();
        }
    }

    if (m_platform->h_wave_out) {
        waveOutReset(m_platform->h_wave_out);
        for (size_t i = 0; i < PlatformData::NUM_BUFFERS; ++i) {
            waveOutUnprepareHeader(m_platform->h_wave_out, &m_platform->headers[i], sizeof(WAVEHDR));
        }
        waveOutClose(m_platform->h_wave_out);
        m_platform->h_wave_out = nullptr;
    }

    if (m_platform->h_event) {
        CloseHandle(m_platform->h_event);
        m_platform->h_event = nullptr;
    }

    m_hardware_active = false;
}
#else
void AudioEngine::init_platform_audio() {
    m_hardware_active = false;
}
void AudioEngine::shutdown_platform_audio() {}
#endif

} // namespace Voidfall
