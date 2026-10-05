#include "audio_system.hpp"
#include <iostream>
#include <algorithm>
#include <random>

namespace Voidfall {

namespace {
static AudioSystem* s_audioSystemInstance = nullptr;

// Thread-safe fast pseudo-random number generator
inline float get_random_float_01() {
    static thread_local std::mt19937 rng(13375);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng);
}
} // namespace

AudioSystem& AudioSystem::instance() {
    if (!s_audioSystemInstance) {
        static AudioSystem s_instance;
        s_audioSystemInstance = &s_instance;
    }
    return *s_audioSystemInstance;
}

AudioSystem::AudioSystem() {
    s_audioSystemInstance = this;

    for (int i = 0; i < VOICE_MAX; ++i) {
        m_voices[i].id = i;
        m_voices[i].type = i;
        m_voices[i].source = static_cast<uint32_t>(i);
        m_voices[i].bufferId = 0;
        m_voices[i].active = false;
        m_voices[i].looping = false;
        m_voices[i].isFadingOut = false;
        m_voices[i].fadeSpeed = 2.0f;
        m_voices[i].currentGain = 1.0f;
        m_voices[i].targetGain = 1.0f;
        m_voices[i].initialFadeGain = 1.0f;
        m_voices[i].fadeTimer = 0.0f;
        m_voices[i].fadeDuration = 0.5f;
        m_voices[i].pitch = 1.0f;
        m_voices[i].time = 0.0f;

        m_alSources[i].state = AL_STOPPED;
        m_alSources[i].looping = AL_FALSE;
        m_alSources[i].gain = 1.0f;
        m_alSources[i].pitch = 1.0f;
        m_alSources[i].bufferId = 0;
        m_alSources[i].position = glm::vec3(0.0f);
        m_alSources[i].referenceDistance = DEFAULT_AL_REFERENCE_DISTANCE;
        m_alSources[i].maxDistance = DEFAULT_AL_MAX_DISTANCE;
        m_alSources[i].rolloffFactor = DEFAULT_AL_ROLLOFF_FACTOR;
        m_alSources[i].distanceModel = AL_INVERSE_DISTANCE_CLAMPED;
    }

    init_sample_pools();
}

AudioSystem::~AudioSystem() {
    stop_all_gameplay_voices();
    if (s_audioSystemInstance == this) {
        s_audioSystemInstance = nullptr;
    }
}

void AudioSystem::init_sample_pools() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_pools.clear();

    // 1. Void Stalker Roars (4 variants)
    SamplePool stalkerRoar;
    stalkerRoar.cue = SoundCue::VoidStalkerRoar;
    stalkerRoar.samplePaths = {
        "assets/sounds/void_stalker_roar_01.wav",
        "assets/sounds/void_stalker_roar_02.wav",
        "assets/sounds/void_stalker_roar_03.wav",
        "assets/sounds/void_stalker_roar_04.wav"
    };
    stalkerRoar.bufferIds = { 1001, 1002, 1003, 1004 };
    stalkerRoar.lastIndex = 0;
    m_pools.push_back(stalkerRoar);

    // Also alias StalkerEchoScreech to the roar pool for legacy code
    SamplePool stalkerEcho = stalkerRoar;
    stalkerEcho.cue = SoundCue::StalkerEchoScreech;
    m_pools.push_back(stalkerEcho);

    // 2. Void Stalker Chitter (3 variants)
    SamplePool stalkerChitter;
    stalkerChitter.cue = SoundCue::StalkerChitter;
    stalkerChitter.samplePaths = {
        "assets/sounds/stalker_chitter_01.wav",
        "assets/sounds/stalker_chitter_02.wav",
        "assets/sounds/stalker_chitter_03.wav"
    };
    stalkerChitter.bufferIds = { 1011, 1012, 1013 };
    stalkerChitter.lastIndex = 0;
    m_pools.push_back(stalkerChitter);

    // 3. Void Stalker Hiss (3 variants)
    SamplePool stalkerHiss;
    stalkerHiss.cue = SoundCue::StalkerHiss;
    stalkerHiss.samplePaths = {
        "assets/sounds/stalker_hiss_01.wav",
        "assets/sounds/stalker_hiss_02.wav",
        "assets/sounds/stalker_hiss_03.wav"
    };
    stalkerHiss.bufferIds = { 1021, 1022, 1023 };
    stalkerHiss.lastIndex = 0;
    m_pools.push_back(stalkerHiss);

    // 4. Subterranean Cavern Settling (3 variants)
    SamplePool cavernSettling;
    cavernSettling.cue = SoundCue::CavernSettling;
    cavernSettling.samplePaths = {
        "assets/sounds/cavern_settling_01.wav",
        "assets/sounds/cavern_settling_02.wav",
        "assets/sounds/cavern_settling_03.wav"
    };
    cavernSettling.bufferIds = { 1031, 1032, 1033 };
    cavernSettling.lastIndex = 0;
    m_pools.push_back(cavernSettling);

    // 5. Tectonic Cavern Groan (4 variants)
    SamplePool cavernGroan;
    cavernGroan.cue = SoundCue::CavernGroan;
    cavernGroan.samplePaths = {
        "assets/sounds/cavern_groan_01.wav",
        "assets/sounds/cavern_groan_02.wav",
        "assets/sounds/cavern_groan_03.wav",
        "assets/sounds/cavern_groan_04.wav"
    };
    cavernGroan.bufferIds = { 1041, 1042, 1043, 1044 };
    cavernGroan.lastIndex = 0;
    m_pools.push_back(cavernGroan);

    // 6. Subterranean Cavern Drips (4 variants)
    SamplePool cavernDrip;
    cavernDrip.cue = SoundCue::CavernDrip;
    cavernDrip.samplePaths = {
        "assets/sounds/cavern_drip_01.wav",
        "assets/sounds/cavern_drip_02.wav",
        "assets/sounds/cavern_drip_03.wav",
        "assets/sounds/cavern_drip_04.wav"
    };
    cavernDrip.bufferIds = { 1051, 1052, 1053, 1054 };
    cavernDrip.lastIndex = 0;
    m_pools.push_back(cavernDrip);

    // 7. Echoing Rock Slide (3 variants)
    SamplePool rockSlide;
    rockSlide.cue = SoundCue::RockSlide;
    rockSlide.samplePaths = {
        "assets/sounds/rock_slide_01.wav",
        "assets/sounds/rock_slide_02.wav",
        "assets/sounds/rock_slide_03.wav"
    };
    rockSlide.bufferIds = { 1061, 1062, 1063 };
    rockSlide.lastIndex = 0;
    m_pools.push_back(rockSlide);

    // 8. Drill Strata Titanium (3 variants)
    SamplePool drillTitanium;
    drillTitanium.cue = SoundCue::DrillStrataTitanium;
    drillTitanium.samplePaths = {
        "assets/sounds/drill_strata_titanium_01.wav",
        "assets/sounds/drill_strata_titanium_02.wav",
        "assets/sounds/drill_strata_titanium_03.wav"
    };
    drillTitanium.bufferIds = { 1071, 1072, 1073 };
    drillTitanium.lastIndex = 0;
    m_pools.push_back(drillTitanium);

    // 9. Drill Strata Voidite (3 variants)
    SamplePool drillVoidite;
    drillVoidite.cue = SoundCue::DrillStrataVoidite;
    drillVoidite.samplePaths = {
        "assets/sounds/drill_strata_voidite_01.wav",
        "assets/sounds/drill_strata_voidite_02.wav",
        "assets/sounds/drill_strata_voidite_03.wav"
    };
    drillVoidite.bufferIds = { 1081, 1082, 1083 };
    drillVoidite.lastIndex = 0;
    m_pools.push_back(drillVoidite);

    // 10. Drill Strata Basalt (3 variants)
    SamplePool drillBasalt;
    drillBasalt.cue = SoundCue::DrillStrataBasalt;
    drillBasalt.samplePaths = {
        "assets/sounds/drill_strata_basalt_01.wav",
        "assets/sounds/drill_strata_basalt_02.wav",
        "assets/sounds/drill_strata_basalt_03.wav"
    };
    drillBasalt.bufferIds = { 1091, 1092, 1093 };
    drillBasalt.lastIndex = 0;
    m_pools.push_back(drillBasalt);

    // 11. Player Death Cue
    SamplePool playerDeath;
    playerDeath.cue = SoundCue::PlayerDeath;
    playerDeath.samplePaths = { "assets/sounds/player_death_01.wav" };
    playerDeath.bufferIds = { 1101 };
    playerDeath.lastIndex = 0;
    m_pools.push_back(playerDeath);

    // 12. Suit Puncture Cue
    SamplePool suitPuncture;
    suitPuncture.cue = SoundCue::SuitPuncture;
    suitPuncture.samplePaths = { "assets/sounds/suit_puncture_01.wav" };
    suitPuncture.bufferIds = { 1111 };
    suitPuncture.lastIndex = 0;
    m_pools.push_back(suitPuncture);

    // 13. Player Breathing Cue
    SamplePool playerBreathing;
    playerBreathing.cue = SoundCue::PlayerBreathing;
    playerBreathing.samplePaths = { "assets/sounds/player_breathing_01.wav" };
    playerBreathing.bufferIds = { 1121 };
    playerBreathing.lastIndex = 0;
    m_pools.push_back(playerBreathing);
}

void AudioSystem::PlayDeathSound() {
    instance().play_death_sound();
}

void AudioSystem::play_death_sound() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // Guard playback behind m_deathSoundPlayed: play strictly once per run
    if (m_deathSoundPlayed) {
        return;
    }
    m_deathSoundPlayed = true;
    m_deathPlayCount++;

    const int voiceIdx = VOICE_DEATH;
    Voice& voice = m_voices[voiceIdx];
    voice.active = true;
    voice.type = VOICE_DEATH;
    voice.looping = false;
    voice.isFadingOut = false;
    voice.currentGain = 1.0f;
    voice.targetGain = 1.0f;
    voice.source = static_cast<uint32_t>(voiceIdx);
    voice.bufferId = 1101;
    voice.cue = SoundCue::PlayerDeath;
    voice.time = 0.0f;

    // Enforce non-looping playback on one-shots
    alSourcei(voice.source, AL_BUFFER, voice.bufferId);
    alSourcei(voice.source, AL_LOOPING, AL_FALSE);
    alSourcef(voice.source, AL_GAIN, 1.0f);
    alSourcef(voice.source, AL_PITCH, 1.0f);
    alSourcePlay(voice.source);

    if (m_engine) {
        m_engine->play_sound_2d(SoundCue::DamageWarning, 1.0f);
    }
}

void AudioSystem::StopAllGameplayVoices() {
    instance().stop_all_gameplay_voices();
}

void AudioSystem::stop_all_gameplay_voices() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // Immediately kill active one-shot death, suit puncture, breathing, and drill SFX channels
    const int gameplayVoices[] = {
        VOICE_DEATH,
        VOICE_SUIT_PUNCTURE,
        VOICE_BREATHING,
        VOICE_DRILL,
        VOICE_ENEMY,
        VOICE_SFX
    };

    for (int voiceIdx : gameplayVoices) {
        Voice& v = m_voices[voiceIdx];
        v.active = false;
        v.isFadingOut = false;
        v.currentGain = 0.0f;
        alSourceStop(v.source);
        alSourcei(v.source, AL_LOOPING, AL_FALSE);
    }

    // Stop all other non-ambient channels
    for (size_t i = 0; i < m_voices.size(); ++i) {
        if (i != VOICE_AMBIENT_BED && i != VOICE_AMBIENT_STEM) {
            m_voices[i].active = false;
            m_voices[i].isFadingOut = false;
            m_voices[i].currentGain = 0.0f;
            alSourceStop(m_voices[i].source);
            alSourcei(m_voices[i].source, AL_LOOPING, AL_FALSE);
        }
    }
}

bool AudioSystem::IsVoiceActive(int voiceType) {
    return instance().is_voice_active(voiceType);
}

bool AudioSystem::is_voice_active(int voiceType) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (voiceType >= 0 && voiceType < static_cast<int>(m_voices.size())) {
        return m_voices[voiceType].active && (m_alSources[voiceType].state == AL_PLAYING);
    }
    return false;
}

void AudioSystem::StopVoice(int voiceId, float fadeDuration) {
    instance().stop_voice(voiceId, fadeDuration);
}

void AudioSystem::stop_voice(int voiceId, float fadeDuration) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (voiceId < 0 || voiceId >= static_cast<int>(m_voices.size())) return;

    Voice& voice = m_voices[voiceId];
    if (!voice.active) return;

    if (fadeDuration <= 0.001f) {
        voice.active = false;
        voice.isFadingOut = false;
        voice.currentGain = 0.0f;
        alSourceStop(voice.source);
    } else {
        voice.isFadingOut = true;
        voice.fadeDuration = fadeDuration;
        voice.initialFadeGain = voice.currentGain;
        voice.fadeSpeed = voice.currentGain / fadeDuration;
        voice.fadeTimer = 0.0f;
    }
}

const Voice& AudioSystem::GetVoice(int voiceId) const {
    int idx = std::clamp(voiceId, 0, VOICE_MAX - 1);
    return m_voices[idx];
}

Voice& AudioSystem::GetVoiceMut(int voiceId) {
    int idx = std::clamp(voiceId, 0, VOICE_MAX - 1);
    return m_voices[idx];
}

uint32_t AudioSystem::TriggerRandomizedCue(SoundCue cue, float volume, float* outPitch, uint32_t* outBufferId) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    SamplePool* targetPool = nullptr;
    for (auto& pool : m_pools) {
        if (pool.cue == cue) {
            targetPool = &pool;
            break;
        }
    }

    uint32_t selectedBufferId = 1001;
    if (targetPool && !targetPool->bufferIds.empty()) {
        size_t count = targetPool->bufferIds.size();
        size_t idx = 0;
        if (count == 1) {
            idx = 0;
        } else {
            // Round-robin or random with repetition avoidance
            idx = (targetPool->lastIndex + 1 + (static_cast<size_t>(get_random_float_01() * (count - 1)))) % count;
            targetPool->lastIndex = idx;
        }
        selectedBufferId = targetPool->bufferIds[idx];
    } else {
        selectedBufferId = 1000 + static_cast<uint32_t>(cue);
    }

    // Procedural pitch jitter (±5% to ±10%)
    float pitchJitterPercent = 0.05f + get_random_float_01() * 0.05f; // 0.05 ... 0.10
    float sign = (get_random_float_01() >= 0.5f) ? 1.0f : -1.0f;
    float finalPitch = 1.0f + sign * pitchJitterPercent;

    // Minor gain variation (0.85 ... 1.0)
    float gainVariation = 0.85f + get_random_float_01() * 0.15f;
    float finalGain = std::clamp(volume * gainVariation, 0.0f, 1.0f);

    // Select voice slot based on cue type
    int voiceIdx = VOICE_SFX;
    if (cue == SoundCue::VoidStalkerRoar || cue == SoundCue::StalkerEchoScreech ||
        cue == SoundCue::StalkerChitter || cue == SoundCue::StalkerHiss ||
        cue == SoundCue::BurrowerRoar || cue == SoundCue::BurrowerGrind ||
        cue == SoundCue::MonsterDigging) {
        voiceIdx = VOICE_ENEMY;
    } else if (cue == SoundCue::CavernSettling || cue == SoundCue::CavernGroan ||
               cue == SoundCue::CavernDrip || cue == SoundCue::RockSlide) {
        voiceIdx = VOICE_AMBIENT_STEM;
    }

    Voice& v = m_voices[voiceIdx];
    v.active = true;
    v.cue = cue;
    v.bufferId = selectedBufferId;
    v.currentGain = finalGain;
    v.targetGain = finalGain;
    v.pitch = finalPitch;
    v.isFadingOut = false;
    v.looping = false;
    v.time = 0.0f;

    alSourcei(v.source, AL_BUFFER, selectedBufferId);
    alSourcei(v.source, AL_LOOPING, AL_FALSE);
    alSourcef(v.source, AL_PITCH, finalPitch);
    alSourcef(v.source, AL_GAIN, finalGain);
    alSourcePlay(v.source);

    if (outPitch) *outPitch = finalPitch;
    if (outBufferId) *outBufferId = selectedBufferId;

    return selectedBufferId;
}

void AudioSystem::SwitchAmbientTrack(int newTrackId, float crossfadeDuration) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_activeAmbientTrack == newTrackId && !m_isCrossfading) {
        return;
    }

    m_outgoingAmbientTrack = m_activeAmbientTrack;
    m_activeAmbientTrack = newTrackId;
    m_crossfadeDuration = (crossfadeDuration > 0.01f) ? crossfadeDuration : 1.8f;
    m_crossfadeTimer = 0.0f;
    m_isCrossfading = true;

    m_outgoingInitialGain = m_incomingAmbientGain;
    m_outgoingAmbientGain = m_outgoingInitialGain;
    m_incomingAmbientGain = 0.0f;
    m_incomingTargetGain = 1.0f;

    alSourcePlay(VOICE_AMBIENT_BED);
    alSourcef(VOICE_AMBIENT_BED, AL_GAIN, m_incomingAmbientGain);
}

void AudioSystem::Update(float dt) {
    instance().update(dt);
}

void AudioSystem::update(float dt) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // ── 1. Smooth Ambient Crossfading ──
    if (m_isCrossfading) {
        m_crossfadeTimer += dt;
        float progress = std::clamp(m_crossfadeTimer / m_crossfadeDuration, 0.0f, 1.0f);
        m_outgoingAmbientGain = m_outgoingInitialGain * (1.0f - progress);
        m_incomingAmbientGain = m_incomingTargetGain * progress;

        alSourcef(VOICE_AMBIENT_BED, AL_GAIN, m_incomingAmbientGain);

        if (m_crossfadeTimer >= m_crossfadeDuration) {
            m_isCrossfading = false;
            m_outgoingAmbientGain = 0.0f;
            m_incomingAmbientGain = m_incomingTargetGain;
            alSourcef(VOICE_AMBIENT_BED, AL_GAIN, m_incomingAmbientGain);
        }
    }

    // ── 2. Active Voice Natural Decay & Fade-Out Tail Processing ──
    for (size_t i = 0; i < m_voices.size(); ++i) {
        Voice& voice = m_voices[i];
        if (voice.active && voice.isFadingOut) {
            voice.currentGain -= voice.fadeSpeed * dt;
            if (voice.currentGain <= 0.0f) {
                voice.currentGain = 0.0f;
                alSourceStop(voice.source);
                voice.active = false;
                voice.isFadingOut = false;
            } else {
                alSourcef(voice.source, AL_GAIN, voice.currentGain);
            }
        }
    }

    // ── 3. Stochastic Environmental Stem Generator (12–25 second randomized interval) ──
    m_stochasticTimer += dt;
    if (m_stochasticTimer >= m_nextStochasticInterval) {
        m_stochasticTimer = 0.0f;
        m_nextStochasticInterval = 12.0f + get_random_float_01() * 13.0f; // 12 ... 25s

        static const SoundCue stems[] = {
            SoundCue::CavernSettling,
            SoundCue::CavernGroan,
            SoundCue::CavernDrip,
            SoundCue::RockSlide
        };
        int pick = static_cast<int>(get_random_float_01() * 4.0f) % 4;
        TriggerRandomizedCue(stems[pick], 0.65f);
    }
}

VirtualALSource& AudioSystem::get_al_source(uint32_t sourceId) {
    uint32_t idx = sourceId % VOICE_MAX;
    return m_alSources[idx];
}

const VirtualALSource& AudioSystem::get_al_source(uint32_t sourceId) const {
    uint32_t idx = sourceId % VOICE_MAX;
    return m_alSources[idx];
}

void AudioSystem::al_sourcei(uint32_t sourceId, int param, int value) {
    VirtualALSource& src = get_al_source(sourceId);
    switch (param) {
        case AL_SOURCE_STATE:
            src.state = value;
            break;
        case AL_LOOPING:
            src.looping = value;
            break;
        case AL_BUFFER:
            src.bufferId = static_cast<uint32_t>(value);
            break;
        default:
            break;
    }
}

void AudioSystem::al_sourcef(uint32_t sourceId, int param, float value) {
    VirtualALSource& src = get_al_source(sourceId);
    switch (param) {
        case AL_GAIN:
            src.gain = std::max(0.0f, value);
            break;
        case AL_PITCH:
            src.pitch = std::max(0.01f, value);
            break;
        case AL_REFERENCE_DISTANCE:
            src.referenceDistance = value;
            break;
        case AL_MAX_DISTANCE:
            src.maxDistance = value;
            break;
        case AL_ROLLOFF_FACTOR:
            src.rolloffFactor = value;
            break;
        default:
            break;
    }
}

void AudioSystem::al_source_stop(uint32_t sourceId) {
    VirtualALSource& src = get_al_source(sourceId);
    src.state = AL_STOPPED;
    uint32_t idx = sourceId % VOICE_MAX;
    m_voices[idx].active = false;
}

void AudioSystem::al_source_play(uint32_t sourceId) {
    VirtualALSource& src = get_al_source(sourceId);
    src.state = AL_PLAYING;
    uint32_t idx = sourceId % VOICE_MAX;
    m_voices[idx].active = true;
}

void AudioSystem::al_get_sourcei(uint32_t sourceId, int param, int* value) const {
    if (!value) return;
    const VirtualALSource& src = get_al_source(sourceId);
    switch (param) {
        case AL_SOURCE_STATE:
            *value = src.state;
            break;
        case AL_LOOPING:
            *value = src.looping;
            break;
        case AL_BUFFER:
            *value = static_cast<int>(src.bufferId);
            break;
        default:
            *value = 0;
            break;
    }
}

void AudioSystem::al_get_sourcef(uint32_t sourceId, int param, float* value) const {
    if (!value) return;
    const VirtualALSource& src = get_al_source(sourceId);
    switch (param) {
        case AL_GAIN:
            *value = src.gain;
            break;
        case AL_PITCH:
            *value = src.pitch;
            break;
        case AL_REFERENCE_DISTANCE:
            *value = src.referenceDistance;
            break;
        case AL_MAX_DISTANCE:
            *value = src.maxDistance;
            break;
        case AL_ROLLOFF_FACTOR:
            *value = src.rolloffFactor;
            break;
        default:
            *value = 0.0f;
            break;
    }
}

float AudioSystem::calculate_distance_attenuation(float distance, float refDist, float maxDist, float rolloff) const {
    float d = std::clamp(distance, refDist, maxDist);
    float gain = refDist / (refDist + rolloff * (d - refDist));
    return std::clamp(gain, 0.0f, 1.0f);
}

// ── Global OpenAL Functions ──
void alSourcei(uint32_t source, int param, int value) {
    AudioSystem::instance().al_sourcei(source, param, value);
}

void alSourcef(uint32_t source, int param, float value) {
    AudioSystem::instance().al_sourcef(source, param, value);
}

void alSourceStop(uint32_t source) {
    AudioSystem::instance().al_source_stop(source);
}

void alSourcePlay(uint32_t source) {
    AudioSystem::instance().al_source_play(source);
}

void alGetSourcei(uint32_t source, int param, int* value) {
    AudioSystem::instance().al_get_sourcei(source, param, value);
}

void alGetSourcef(uint32_t source, int param, float* value) {
    AudioSystem::instance().al_get_sourcef(source, param, value);
}

void alDistanceModel(int model) {
    for (int i = 0; i < VOICE_MAX; ++i) {
        AudioSystem::instance().get_al_source(i).distanceModel = model;
    }
}

} // namespace Voidfall
