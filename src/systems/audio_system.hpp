#pragma once

#include <vector>
#include <string>
#include <array>
#include <memory>
#include <mutex>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include "../audio/audio_engine.hpp"

// ── OpenAL Compatibility Tokens & Constants ──
#ifndef AL_NONE
#define AL_NONE 0
#define AL_FALSE 0
#define AL_TRUE 1
#define AL_SOURCE_STATE 0x1010
#define AL_INITIAL 0x1011
#define AL_PLAYING 0x1012
#define AL_PAUSED 0x1013
#define AL_STOPPED 0x1014
#define AL_BUFFER 0x1009
#define AL_GAIN 0x100A
#define AL_PITCH 0x1003
#define AL_LOOPING 0x1007
#define AL_POSITION 0x1004
#define AL_VELOCITY 0x1006
#define AL_DIRECTION 0x1005
#define AL_REFERENCE_DISTANCE 0x1020
#define AL_ROLLOFF_FACTOR 0x1021
#define AL_MAX_DISTANCE 0x1023
#define AL_INVERSE_DISTANCE_CLAMPED 0xD004
#define AL_EXPONENT_DISTANCE_CLAMPED 0xD006
#endif

namespace Voidfall {

// ── Voice Channel Type Constants ──
constexpr int VOICE_DEATH           = 0;
constexpr int VOICE_SUIT_PUNCTURE   = 1;
constexpr int VOICE_BREATHING       = 2;
constexpr int VOICE_DRILL           = 3;
constexpr int VOICE_AMBIENT_BED     = 4;
constexpr int VOICE_AMBIENT_STEM    = 5;
constexpr int VOICE_ENEMY           = 6;
constexpr int VOICE_SFX             = 7;
constexpr int VOICE_MAX             = 32;

// Calibrated 3D Spatial Audio Invariants
constexpr float DEFAULT_AL_REFERENCE_DISTANCE = 4.0f;
constexpr float DEFAULT_AL_MAX_DISTANCE       = 65.0f;
constexpr float DEFAULT_AL_ROLLOFF_FACTOR     = 1.2f;

/// Virtual OpenAL Source Model for zero-allocation headless testing & hardware emulation
struct VirtualALSource {
    int state{AL_STOPPED};
    int looping{AL_FALSE};
    float gain{1.0f};
    float pitch{1.0f};
    uint32_t bufferId{0};
    glm::vec3 position{0.0f};
    float referenceDistance{DEFAULT_AL_REFERENCE_DISTANCE};
    float maxDistance{DEFAULT_AL_MAX_DISTANCE};
    float rolloffFactor{DEFAULT_AL_ROLLOFF_FACTOR};
    int distanceModel{AL_INVERSE_DISTANCE_CLAMPED};
};

/// Active Audio Voice with release envelope, fade-out tracking, and cue reference
struct Voice {
    int id{0};
    int type{0};
    uint32_t source{0};
    uint32_t bufferId{0};
    bool active{false};
    bool looping{false};
    bool isFadingOut{false};
    float fadeSpeed{2.0f};
    float currentGain{1.0f};
    float targetGain{1.0f};
    float initialFadeGain{1.0f};
    float fadeTimer{0.0f};
    float fadeDuration{0.5f};
    float pitch{1.0f};
    float time{0.0f};
    SoundCue cue{SoundCue::UIBlip};
    std::string samplePath{""};
};

/// Multi-Sample Randomized Pool Entry
struct SamplePool {
    SoundCue cue{SoundCue::UIBlip};
    std::vector<std::string> samplePaths;
    std::vector<uint32_t> bufferIds;
    size_t lastIndex{0};
};

/// High-Level Audio Subsystem managing voice channels, randomized multi-sample pools,
/// smooth ambient crossfades, natural decay tails, and menu transition cleanup.
class AudioSystem {
public:
    static AudioSystem& instance();

    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    /// Attach underlying AudioEngine (optional, for synthesis & hardware mix output)
    void set_audio_engine(AudioEngine* engine) { m_engine = engine; }
    AudioEngine* audio_engine() { return m_engine; }

    // ── 1. Death Audio Single-Trigger Guard & Channel Teardown ──
    static void PlayDeathSound();
    void play_death_sound();

    static void StopAllGameplayVoices();
    void stop_all_gameplay_voices();

    static bool IsVoiceActive(int voiceType);
    bool is_voice_active(int voiceType) const;

    void reset_death_sound() { m_deathSoundPlayed = false; m_deathPlayCount = 0; }
    bool is_death_sound_played() const { return m_deathSoundPlayed; }
    int death_play_count() const { return m_deathPlayCount; }

    // ── 2. Voice Fade-Out & Stop Architecture ──
    static void StopVoice(int voiceId, float fadeDuration = 0.5f);
    void stop_voice(int voiceId, float fadeDuration = 0.5f);

    const Voice& GetVoice(int voiceId) const;
    Voice& GetVoiceMut(int voiceId);

    // ── 3. Sound Variation & Asset Pool Randomization ──
    uint32_t TriggerRandomizedCue(SoundCue cue, float volume = 1.0f, float* outPitch = nullptr, uint32_t* outBufferId = nullptr);

    // ── 4. Layered Ambient & Smooth Crossfade System ──
    void SwitchAmbientTrack(int newTrackId, float crossfadeDuration = 1.8f);
    void switch_ambient_track(int newTrackId, float crossfadeDuration = 1.8f) { SwitchAmbientTrack(newTrackId, crossfadeDuration); }

    int active_ambient_track() const { return m_activeAmbientTrack; }
    float get_outgoing_ambient_gain() const { return m_outgoingAmbientGain; }
    float get_incoming_ambient_gain() const { return m_incomingAmbientGain; }
    bool is_crossfading() const { return m_isCrossfading; }

    // ── 5. Per-Frame Update (Fades, Crossfades, Stochastic Micro-Ambiences) ──
    static void Update(float dt);
    void update(float dt);

    // ── Virtual OpenAL Accessors (Used for testing and hardware abstraction) ──
    VirtualALSource& get_al_source(uint32_t sourceId);
    const VirtualALSource& get_al_source(uint32_t sourceId) const;
    void al_sourcei(uint32_t sourceId, int param, int value);
    void al_sourcef(uint32_t sourceId, int param, float value);
    void al_source_stop(uint32_t sourceId);
    void al_source_play(uint32_t sourceId);
    void al_get_sourcei(uint32_t sourceId, int param, int* value) const;
    void al_get_sourcef(uint32_t sourceId, int param, float* value) const;

    /// Calculate distance attenuation based on AL_INVERSE_DISTANCE_CLAMPED with calibrated reference parameters
    float calculate_distance_attenuation(float distance, float refDist = DEFAULT_AL_REFERENCE_DISTANCE,
                                         float maxDist = DEFAULT_AL_MAX_DISTANCE, float rolloff = DEFAULT_AL_ROLLOFF_FACTOR) const;

private:
    void init_sample_pools();

    AudioEngine* m_engine{nullptr};
    mutable std::recursive_mutex m_mutex;

    // Single-trigger death guard & telemetry
    bool m_deathSoundPlayed{false};
    int m_deathPlayCount{0};

    // Voice storage & OpenAL source abstractions
    std::array<Voice, VOICE_MAX> m_voices;
    std::array<VirtualALSource, VOICE_MAX> m_alSources;

    // Multi-sample variation pools
    std::vector<SamplePool> m_pools;

    // Layered Ambience & Crossfade state
    int m_activeAmbientTrack{1};
    int m_outgoingAmbientTrack{-1};
    float m_crossfadeDuration{1.8f};
    float m_crossfadeTimer{0.0f};
    bool m_isCrossfading{false};
    float m_outgoingAmbientGain{0.0f};
    float m_incomingAmbientGain{1.0f};
    float m_outgoingInitialGain{1.0f};
    float m_incomingTargetGain{1.0f};

    // Stochastic Environmental Stem Generator (12–25 second randomized interval)
    float m_stochasticTimer{0.0f};
    float m_nextStochasticInterval{14.0f};
    uint32_t m_rngSeed{54321};
};

// Global OpenAL-Style Function Wrappers (for drop-in compatibility)
void alSourcei(uint32_t source, int param, int value);
void alSourcef(uint32_t source, int param, float value);
void alSourceStop(uint32_t source);
void alSourcePlay(uint32_t source);
void alGetSourcei(uint32_t source, int param, int* value);
void alGetSourcef(uint32_t source, int param, float* value);
void alDistanceModel(int model);

} // namespace Voidfall
