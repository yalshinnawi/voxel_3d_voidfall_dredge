#pragma once

#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <array>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>

namespace Voidfall {

/// Comprehensive Sound Cue identifiers across gameplay subsystems
enum class SoundCue : uint16_t {
    // ── Mining & Tools ──
    DrillLoop = 0,         // Rotary drill motor and mechanical tooth grinding
    VoxelHit,              // Tool impact ping/thud on solid surface
    VoxelBreakBasalt,      // Dense rock/basalt crumble and shatter
    VoxelBreakTitanium,    // Resonant metallic clang & ringing overtone
    VoxelBreakVoidite,     // High-energy ethereal crystalline harmonic shatter
    VoxelBreakBulkhead,    // Heavy industrial plate buckle and snap
    VoxelBreakRadioactive, // Sizzling brittle fracture with ionizing crackle
    BulkheadDeploy,        // Hydraulic lock and magnetic seal
    BulkheadDismantle,     // Mechanical latch release and salvage catch

    // ── Hostile Organisms (Void Stalker & Seismic Burrower) ──
    StalkerSpotted,        // Chilling alien chitter / guttural resonant click
    StalkerLunge,          // Aggressive predatory attack screech (safely limited)
    StalkerHit,            // Claws striking player exo-armor
    StalkerDie,            // Dissolving void entity dissipation hum
    StalkerEchoScreech,    // Rare predatory echo screech resonating down cavern tunnels
    StalkerChitter,        // Subtle chitinous clicking & mandible twitch in the dark (stealth cue)
    StalkerHiss,           // Low menacing predatory threat hiss when provoked
    BurrowerRoar,          // Deep tectonic subterranean borer roar and earth rumble
    BurrowerGrind,         // High-torque borer teeth grinding through stone
    MonsterDigging,        // High-torque claws and borer teeth drilling/digging into rock walls

    // ── Ambience, Hazards & Biome Atmosphere ──
    AmbientCavern,         // Legacy/fallback deep subterranean sub-bass drone and void wind
    AmbientSector1,        // Perimeter Drift: Crystalline resonant airflow, quartz harmonics
    AmbientSector2,        // Volatile Fault: Geothermal vents, subterranean magma hum, basalt stress
    AmbientSector3,        // Void Cradle: Deep abyssal mantle drone, gravitational void pulse
    SectorArrival1,        // Sector 1 arrival stinger: Ethereal crystalline descent chord
    SectorArrival2,        // Sector 2 arrival stinger: Heavy industrial seismic brass swell
    SectorArrival3,        // Sector 3 arrival stinger: Abyssal void strike & sub-bass tension impact
    SectorArrival4,        // Sector arrival stinger 4: Distant cavern hunter screech & reflection
    SectorArrival5,        // Sector arrival stinger 5: Swarm infrasound alien howl & choral dissonance
    CavernDrip,            // Procedural subterranean moisture droplet acoustic ping
    CavernGroan,           // Tectonic rock stratum stress and structural groan
    CrystalChime,          // Sector 1: Delicate harmonic quartz crystalline resonance
    GeothermalVent,        // Sector 2: Pressurized volcanic steam / gas exhaust release
    VoidDistortion,        // Sector 3: Dimensional acoustic phase-warp / abyssal whisper
    SeismicTremor,         // Low-frequency tectonic earthquake rumble
    GeigerClick,           // Poisson ionizing radiation detection clicks
    BeaconSiren,           // Pulsing dual-tone evacuation beacon alarm
    EvacTouchdown,         // Heavy retro-thruster roar and pod locking clamps

    // ── Combat Weapons (Class-Specific) ──
    PlasmaFire,            // Vanguard: Coherent plasma bolt shot discharge
    PlasmaHit,             // Kinetic thermal plasma impact on alien armor or rock
    ScattergunFire,        // Demolitionist: Heavy kinetic boom + mechanical flechette spread
    RailgunFire,           // Scout: Supersonic electromagnetic whip crack + inductive discharge
    PlasmaCarbineReload,   // Vanguard: Thermal cell eject, plasma canister slam & charge chirp
    ScattergunReload,      // Demolitionist: Heavy drum latch release, cylinder ratchet & solid clamp
    RailgunReload,         // Scout: Pneumatic needle rack eject, magnetic cell insert & coil whine

    // ── Player & Abilities ──
    Footstep,              // Subterranean boot strike on stone floor
    Jump,                  // Pneumatic thruster boost puff
    Land,                  // Kinetic boot impact thud
    SonarPulse,            // Surveying acoustic echolocation sweep with cavern reverb
    ExplosiveBlast,        // Demolition shaped-charge concussive blast
    TacticalRepulsor,      // Vanguard kinetic repulsor pulse and magnetic discharge
    TacticalBarricade = TacticalRepulsor, // Backwards compatibility alias
    TacticalOvercharge,    // Scout kinetic dash sonic release

    // ── Interface & Audio Feedback ──
    UIBlip,                // Clean pentatonic navigation blip
    UIUpgrade,             // Ascending harmonic triad for terminal upgrades
    DamageWarning,         // Muffled low-pass cardiovascular alarm for critical health

    // ── Room Archetype & Environmental Hazard Atmosphere ──
    LavaBubble,            // Bubbling viscous thermite lava & volcanic churn (Magma Caldera)
    ThermalHiss,           // Searing thermal heat & sulfurous gas release (Fault Crevasse / Lava)
    RadioactiveHum,        // Resonant ionizing electromagnetic radiation hum (Radioactive Core)
    VoidWind,              // Eerie howling draft echoing through chasms and rifts (Abyssal Chasm / Void Rift)
    GravityDistortion,     // Gravitational phase-warp pulse near singularity (Void Rift)
    SporePlop,             // Organic fungal spore drop & damp bio-grotto squelch (Fungoid Grotto)
    OrganicCreak,          // Creaking fibrous mycelium & subterranean bio-stalks (Fungoid Grotto)
    IndustrialHum,         // 60Hz electrical transformer hum & generator whir (Foundry / Vault)
    HydraulicExhaust,      // Heavy pneumatic piston & steam relief exhaust (Foundry / Vault)
    PebbleSkitter,         // Loose stone gravel & debris skittering from ceiling (Crumbling Canyon / Quarry)
    SpikeRattle,           // Hollow bone/metal rattle echoing through spike trench (Spike Arena)

    // ── Multi-Sample Variants & Subterranean Atmosphere ──
    VoidStalkerRoar,       // Vocalization variants for aberrant stalkers
    CavernSettling,        // Deep earth tectonic settling
    RockSlide,             // Echoing loose rock slide and scree fall
    DrillStrataTitanium,   // Dense metallic resonant drilling
    DrillStrataVoidite,    // Brittle crystalline harmonic drilling
    DrillStrataBasalt,     // Heavy dull stone drill scraping
    PlayerDeath,           // Critical suit failure and collapse sequence
    SuitPuncture,          // Atmospheric decompression & puncture hiss
    PlayerBreathing,       // Strained Delver respiration under stress
    PlayerGroan,           // Delver vocal pain groan & strain under radiation sickness / systemic damage
    PlayerAsphyxiation,    // Delver violent coughing, gasping & choking spasms from toxic gas inhalation
    ToxicGasHiss,          // Pressurized escaping toxic gas & caustic chemical vent hiss
    BoneCrack,             // Brutal bone snapping fracture crunch on high-speed fall impact
    EnemyFleshHit,         // Visceral flesh tearing / claw laceration impact on player from alien attack
    DebrisArmorImpact,     // Crashing stone rubble & armor deflecting heavy falling debris
    CritHit,               // High-impact sneak attack / critical strike sound cue (crunch punch + resonant ring)
    JetpackLoop,           // Exo-suit rocket hover thruster continuous combustion burn
    GrappleFire,           // Pneumatic grapple anchor launch & cable shoot
    GrappleReel,           // Electric motorized winch cable tension reel

    Count
};

/// Sound category buses for independent sound volume control
enum class SoundCategory : uint8_t {
    Master = 0,
    SFX,        // Tools, mining, voxel breaks, player kinetics, explosions
    Enemy,      // Hostile creatures, stalkers
    Ambience,   // Cavern wind, seismic tremors, geiger counter, beacons
    UI          // Menu blips, upgrades, alarms
};

inline SoundCategory get_sound_category(SoundCue cue) {
    switch (cue) {
        case SoundCue::StalkerSpotted:
        case SoundCue::StalkerLunge:
        case SoundCue::StalkerHit:
        case SoundCue::StalkerDie:
        case SoundCue::StalkerEchoScreech:
        case SoundCue::StalkerChitter:
        case SoundCue::StalkerHiss:
        case SoundCue::VoidStalkerRoar:
        case SoundCue::BurrowerRoar:
        case SoundCue::BurrowerGrind:
        case SoundCue::MonsterDigging:
            return SoundCategory::Enemy;

        case SoundCue::AmbientCavern:
        case SoundCue::AmbientSector1:
        case SoundCue::AmbientSector2:
        case SoundCue::AmbientSector3:
        case SoundCue::CavernDrip:
        case SoundCue::CavernGroan:
        case SoundCue::CavernSettling:
        case SoundCue::RockSlide:
        case SoundCue::CrystalChime:
        case SoundCue::GeothermalVent:
        case SoundCue::VoidDistortion:
        case SoundCue::SeismicTremor:
        case SoundCue::GeigerClick:
        case SoundCue::BeaconSiren:
        case SoundCue::EvacTouchdown:
        case SoundCue::LavaBubble:
        case SoundCue::ThermalHiss:
        case SoundCue::RadioactiveHum:
        case SoundCue::VoidWind:
        case SoundCue::GravityDistortion:
        case SoundCue::SporePlop:
        case SoundCue::OrganicCreak:
        case SoundCue::IndustrialHum:
        case SoundCue::HydraulicExhaust:
        case SoundCue::PebbleSkitter:
        case SoundCue::SpikeRattle:
        case SoundCue::ToxicGasHiss:
            return SoundCategory::Ambience;

        case SoundCue::UIBlip:
        case SoundCue::UIUpgrade:
        case SoundCue::DamageWarning:
            return SoundCategory::UI;

        default:
            return SoundCategory::SFX;
    }
}

/// 3D Audio Listener state
struct AudioListener {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
};

/// Active audio voice for procedural synthesis
struct AudioVoice {
    SoundCue cue{SoundCue::UIBlip};
    bool active{false};
    bool loop{false};
    float time{0.0f};
    float duration{1.0f};
    float volume{1.0f};
    float pitch{1.0f};
    bool is_3d{false};
    glm::vec3 world_pos{0.0f};
    float pan_left{0.707f};
    float pan_right{0.707f};
    float current_gain{0.0f};
    bool fading_out{false};
    float fade_out_remaining{0.0f};
    float fade_out_total{0.030f};

    // Distance low-pass filter (simulates air absorption and cavern wall muffling for distant sounds)
    float distance_lp_alpha{1.0f};
    float distance_filter_state[2]{0.0f, 0.0f};

    // Sample playback cursor (fractional frame index for linear interpolation)
    float sample_cursor{0.0f};

    // Voice internal synthesis state (phase accumulators, formant/comb filters, params)
    float phase[8]{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float filter_state[8]{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float param[4]{0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t seed{12345};
};

/// Real-time audio engine with procedural synthesis, 3D spatialization,
/// and rigorous Ear Safety / Hearing Protection mastering chains.
class AudioEngine {
public:
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int NUM_CHANNELS = 2; // Stereo
    static constexpr size_t MAX_VOICES = 32;

    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    /// Initialize audio hardware (or fallback to virtual headless mode)
    bool init(bool enable_hardware = true);

    /// Shutdown audio subsystem
    void shutdown();

    /// Set master volume [0.0 .. 1.0]
    void set_master_volume(float volume);
    float master_volume() const { return m_master_volume; }

    /// Category volume controls [0.0 .. 1.0] for turning down specific sound channels
    void set_sfx_volume(float volume);
    float sfx_volume() const { return m_sfx_volume; }

    void set_enemy_volume(float volume);
    float enemy_volume() const { return m_enemy_volume; }

    void set_ambient_volume(float volume);
    float ambient_volume() const { return m_ambient_volume; }

    void set_ui_volume(float volume);
    float ui_volume() const { return m_ui_volume; }

    void set_channel_volume(SoundCategory category, float volume);
    float channel_volume(SoundCategory category) const;

    /// Master mute toggle (instant digital silence)
    void set_mute_all(bool mute);
    bool is_muted() const { return m_mute_all; }

    /// Helper to get effective channel scale for a cue
    float get_cue_volume_factor(SoundCue cue) const;

    /// Update listener transform for 3D spatial audio
    void set_listener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);

    /// Trigger 2D sound cue (UI, ambient, HUD)
    void play_sound_2d(SoundCue cue, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

    /// Trigger 3D sound cue positioned in world space
    void play_sound_3d(SoundCue cue, const glm::vec3& world_pos, float volume = 1.0f, float pitch = 1.0f);

    /// Stop specific sound cue (with smooth anti-click fade-out by default)
    void stop_sound(SoundCue cue, bool instant = false);

    /// Stop all active sound voices (instant cutoff by default for tests & scene transitions)
    void stop_all(bool instant = true);

    /// Play non-repetitive sector arrival stinger with organic pitch modulation
    void play_arrival_stinger(int sector);

    /// Play class-specific weapon reload sound with organic pitch modulation
    void play_weapon_reload(int archetype);

    /// Modulate active continuous sounds
    void set_drill_active(bool active, float progress = 0.0f, const glm::vec3& pos = glm::vec3(0.0f));
    void set_thruster_active(bool active, const glm::vec3& pos = glm::vec3(0.0f));
    void set_grapple_active(bool active, bool reeling = false, const glm::vec3& pos = glm::vec3(0.0f));
    void set_ambient_intensity(float intensity);
    void set_hazard_phase(int phase);
    void set_seismic_rumble(float intensity);
    void set_radiation_proximity(float level);
    float radiation_proximity() const { return m_radiation_proximity; }

    /// Set and get the active expedition sector (1: Perimeter Drift, 2: Volatile Fault, 3: Void Cradle)
    void set_current_sector(int sector);
    int current_sector() const { return m_current_sector; }

    /// Set and get active room archetype (RoomShapeType cast to int, or -1 for generic corridor)
    void set_current_room_type(int room_type);
    int current_room_type() const { return m_current_room_type; }

    /// Trigger procedural room-specific micro-ambience (magma pops, spore drops, industrial vents, etc.)
    void trigger_room_micro_event(int room_shape, float volume = 0.4f);

    /// Trigger random procedural cavern micro-ambience (water drip, crystal ping, vent hiss, or void distortion)
    void trigger_cavern_micro_event(int sector = 1, float volume = 0.4f);
    void trigger_cavern_micro_event(int sector, int room_shape, float volume = 0.4f);

    /// Update dynamic environmental hazard proximity (smooth distance-based modulation)
    void update_hazard_proximity_audio(float dt, float lava_dist, float rad_level, float spike_dist, float void_dist, float gas_dist = 999.0f);

    /// Environmental hazard acoustic trigger helpers
    void trigger_player_groan(float volume = 0.85f);
    void trigger_player_asphyxiation(float volume = 0.90f);
    void trigger_toxic_gas_hiss(float volume = 0.65f);
    void trigger_bone_crack(float volume = 1.0f);
    void trigger_enemy_flesh_hit(float volume = 1.0f);
    void trigger_debris_impact(float volume = 1.0f);

    /// Trigger dynamic ducking of ambient / background noise (Dead Space / Alien Isolation negative space)
    void trigger_ducking(float target_attenuation = 0.28f, float hold_seconds = 0.8f, float recovery_rate = 1.35f);
    float ducking_factor() const { return m_ducking_attenuation; }

    /// Update player biometric audio (visceral cardiac heartbeat pulse under low health < 35% or extreme stress)
    void update_biometrics(float health_pct, float threat_proximity = 0.0f);
    float biometric_stress() const { return m_biometric_stress; }

    /// Synthesize and mix audio into a stereo buffer (used by hardware callback and test harness)
    /// Zero heap allocations inside this function.
    void render_mix(float* output_interleaved, size_t num_frames);

    /// Render 16-bit PCM stereo samples (hardware/WAV format)
    void render_mix_i16(int16_t* output_interleaved, size_t num_frames);

    /// Is hardware audio device active?
    bool is_hardware_active() const { return m_hardware_active; }

    /// Count of currently active playing voices
    int active_voice_count() const;

    /// Test / telemetry inspection helpers
    const std::vector<SoundCue>& recent_arrival_stingers() const { return m_recent_arrival_stingers; }
    const std::vector<SoundCue>& recent_micro_cues() const { return m_recent_micro_cues; }
    float seismic_rumble_cooldown() const { return m_seismic_rumble_cooldown; }
    float void_hazard_cooldown() const { return m_void_hazard_cooldown; }
    const std::array<AudioVoice, MAX_VOICES>& voices() const { return m_voices; }

    /// Ear Safety & Hearing Protection Metrics
    struct SafetyMetrics {
        float peak_amplitude{0.0f};       // Maximum absolute sample amplitude [0..1]
        float peak_dbfs{-96.0f};           // Peak in decibels relative to full scale
        float max_slew_rate{0.0f};         // Maximum sample-to-sample delta (click detector)
        int clipped_samples{0};            // Count of samples reaching hard limit
        float high_frequency_ratio{0.0f};  // Ratio of energy above 8 kHz vs total energy
    };
    SafetyMetrics get_and_reset_safety_metrics();

    /// Memory-cached audio sample buffer
    struct SoundSample {
        std::vector<float> data; // Interleaved stereo float samples [-1.0 .. 1.0]
        size_t frame_count{0};
        float duration_seconds{0.0f};
        bool loaded{false};
    };

    /// Load WAV sound samples from a directory (assets/sounds/)
    bool load_sound_samples(const std::string& directory_path = "");

    /// Check if a cue has an imported audio file sample loaded
    bool has_sample(SoundCue cue) const;

    /// Get sample details for inspection / debugging
    const SoundSample* get_sample(SoundCue cue) const;

    /// Diagnostic test helper: render exact N milliseconds into an allocated vector for offline analysis
    std::vector<float> render_offline_samples(float duration_seconds);

private:
    void init_platform_audio();
    void shutdown_platform_audio();

    // Voice synthesis generators
    float synth_sample(AudioVoice& voice, float dt);
    void update_spatial_pan(AudioVoice& voice);

    // Audio Mastering Chain (Ear Protection)
    void apply_mastering_chain(float* buffer, size_t num_frames);

    // State
    float m_master_volume{1.0f};
    float m_sfx_volume{1.0f};
    float m_enemy_volume{1.0f};
    float m_ambient_volume{1.0f};
    float m_ui_volume{1.0f};
    bool m_mute_all{false};

    AudioListener m_listener;
    std::array<AudioVoice, MAX_VOICES> m_voices;
    std::array<SoundSample, static_cast<size_t>(SoundCue::Count)> m_samples;
    mutable std::recursive_mutex m_voice_mutex;

    // Sustained sound tracking
    bool m_drill_active{false};
    float m_drill_progress{0.0f};
    glm::vec3 m_drill_pos{0.0f};
    bool m_thruster_active{false};
    glm::vec3 m_thruster_pos{0.0f};
    bool m_grapple_reeling{false};
    glm::vec3 m_grapple_pos{0.0f};
    float m_ambient_intensity{0.5f};
    int m_current_sector{1};
    int m_current_room_type{-1};
    float m_lava_proximity{0.0f};
    float m_spike_proximity{0.0f};
    float m_void_proximity{0.0f};
    float m_hazard_audio_timer{0.0f};
    int m_hazard_phase{0};
    float m_hazard_timer{0.0f};
    float m_tremor_intensity{0.0f};
    float m_radiation_proximity{0.0f};
    float m_geiger_timer{0.0f};
    float m_next_geiger_interval{0.25f};
    uint32_t m_geiger_seed{98765};
    float m_micro_ambience_timer{0.0f};
    uint32_t m_ambience_seed{77711};
    std::vector<SoundCue> m_recent_arrival_stingers;
    std::vector<SoundCue> m_recent_micro_cues;

    // Environmental Hazard & Rumble Spacing Cooldowns (prevents repetitive sound washes)
    float m_void_hazard_cooldown{0.0f};
    float m_lava_hazard_cooldown{0.0f};
    float m_spike_hazard_cooldown{0.0f};
    float m_gas_hazard_cooldown{0.0f};
    float m_seismic_rumble_cooldown{0.0f};

    // Dynamic Threat Ducking (creates negative space for horror stings)
    float m_ducking_attenuation{1.0f};
    float m_ducking_timer{0.0f};
    float m_ducking_recovery_rate{1.35f};
    float m_ducking_target{0.28f};

    // Biometric stress & low-health vitals synthesis
    float m_biometric_stress{0.0f};
    float m_heartbeat_phase{0.0f};

    // Ear Safety & Mastering Filters
    float m_dc_block_x1[2]{0.0f, 0.0f};
    float m_dc_block_y1[2]{0.0f, 0.0f};
    float m_lp_state[2]{0.0f, 0.0f};
    float m_limiter_envelope{0.0f};
    float m_prev_sample[2]{0.0f, 0.0f};

    SafetyMetrics m_safety_metrics;
    bool m_hardware_active{false};

    // Platform-specific handle (opaque pointer to hide Windows headers from header file)
    struct PlatformData;
    std::unique_ptr<PlatformData> m_platform;
};

} // namespace Voidfall
