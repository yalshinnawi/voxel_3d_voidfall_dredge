#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <set>
#include <functional>
#include <cstdlib>
#include "../src/systems/audio_system.hpp"

using namespace Voidfall;

#define CHECK(expr, msg) \
    if (!(expr)) { \
        std::cerr << "  [FAIL] " << msg << " (" #expr ") at line " << __LINE__ << std::endl; \
        std::exit(1); \
    }

struct TestCase {
    std::string name;
    std::function<void()> func;
};

static std::vector<TestCase>& get_test_registry() {
    static std::vector<TestCase> registry;
    return registry;
}

#define TEST(suite, name) \
    void suite##_##name(); \
    struct Register_##suite##_##name { \
        Register_##suite##_##name() { \
            get_test_registry().push_back({ #suite "." #name, suite##_##name }); \
        } \
    } reg_##suite##_##name; \
    void suite##_##name()

// ── Test 1: Death Sound Stops On Menu Transition ──
TEST(AudioSystemTest, DeathSoundStopsOnMenuTransition) {
    AudioSystem& audio = AudioSystem::instance();
    audio.reset_death_sound();

    // 1. Simulate player death (PlayDeathSound())
    AudioSystem::PlayDeathSound();

    int stateBefore = 0;
    alGetSourcei(VOICE_DEATH, AL_SOURCE_STATE, &stateBefore);
    CHECK(AudioSystem::IsVoiceActive(VOICE_DEATH), "Death voice must be active after PlayDeathSound()");
    CHECK(stateBefore == AL_PLAYING, "Virtual OpenAL source state must be AL_PLAYING");

    int isLooping = AL_TRUE;
    alGetSourcei(VOICE_DEATH, AL_LOOPING, &isLooping);
    CHECK(isLooping == AL_FALSE, "One-shot death voice must have non-looping playback (AL_LOOPING == AL_FALSE)");

    // 2. Trigger state transition: in Application::TransitionState(STATE_MAIN_MENU),
    // AudioSystem::StopAllGameplayVoices() is invoked.
    AudioSystem::StopAllGameplayVoices();

    // 3. Assert that AudioSystem::IsVoiceActive(VOICE_DEATH) is false and OpenAL source state is AL_STOPPED
    int stateAfter = 0;
    alGetSourcei(VOICE_DEATH, AL_SOURCE_STATE, &stateAfter);
    CHECK(!AudioSystem::IsVoiceActive(VOICE_DEATH), "AudioSystem::IsVoiceActive(VOICE_DEATH) must be false after menu transition");
    CHECK(stateAfter == AL_STOPPED, "OpenAL source state must be AL_STOPPED after menu transition");
}

// ── Test 2: Death Sound Plays Exactly Once ──
TEST(AudioSystemTest, DeathSoundPlaysExactlyOnce) {
    AudioSystem& audio = AudioSystem::instance();
    audio.reset_death_sound();

    // Fire multiple consecutive death triggers in the same frame or tick
    for (int i = 0; i < 10; ++i) {
        AudioSystem::PlayDeathSound();
    }

    // Verify the sound source is triggered only once (playCount == 1)
    CHECK(audio.death_play_count() == 1, "Death sound source must be triggered only once per expedition run");
    CHECK(audio.is_death_sound_played(), "Death sound single-trigger guard flag must be set");

    // Reinitializing an active expedition in STATE_GAMEPLAY resets the guard
    audio.reset_death_sound();
    CHECK(!audio.is_death_sound_played(), "Resetting death sound must clear guard flag");

    AudioSystem::PlayDeathSound();
    CHECK(audio.death_play_count() == 1, "Death sound should trigger again on new run");
}

// ── Test 3: Ambient Crossfade Non-Zero Duration ──
TEST(AudioSystemTest, AmbientCrossfadeNonZeroDuration) {
    AudioSystem& audio = AudioSystem::instance();

    // Setup initial ambient track 1
    audio.SwitchAmbientTrack(1, 1.6f);
    audio.Update(2.0f); // Let track 1 settle
    CHECK(!audio.is_crossfading(), "Initial ambient track should be fully settled");

    // Switch ambient tracks over a 1.6 second duration
    audio.SwitchAmbientTrack(2, 1.6f);
    CHECK(audio.is_crossfading(), "AudioSystem should be actively crossfading");

    float prevOut = audio.get_outgoing_ambient_gain();
    float prevIn = audio.get_incoming_ambient_gain();

    // Step AudioSystem::Update(dt) over multiple frames (16 steps of 0.1s = 1.6s)
    for (int step = 1; step <= 16; ++step) {
        audio.Update(0.1f);
        float currentOut = audio.get_outgoing_ambient_gain();
        float currentIn = audio.get_incoming_ambient_gain();

        // Outgoing track gain decreases smoothly toward 0 without sudden step discontinuities
        CHECK(currentOut <= prevOut + 0.001f, "Outgoing track gain must decrease smoothly toward 0");
        // Incoming track gain increases toward target level without sudden step discontinuities
        CHECK(currentIn >= prevIn - 0.001f, "Incoming track gain must increase smoothly toward target");

        prevOut = currentOut;
        prevIn = currentIn;
    }

    // Crossfade should finish cleanly
    CHECK(!audio.is_crossfading(), "Crossfade should finish when duration has elapsed");
    CHECK(audio.get_outgoing_ambient_gain() <= 0.001f, "Outgoing track gain should be 0");
    CHECK(std::abs(audio.get_incoming_ambient_gain() - 1.0f) <= 0.001f, "Incoming track gain should be at 1.0");
}

// ── Test 4: Stop Voice Triggers Fade-Out ──
TEST(AudioSystemTest, StopVoiceTriggersFadeOut) {
    AudioSystem& audio = AudioSystem::instance();
    int voiceId = VOICE_ENEMY;

    // Start voice
    audio.TriggerRandomizedCue(SoundCue::VoidStalkerRoar, 1.0f);
    CHECK(audio.IsVoiceActive(voiceId), "Enemy voice should be active");

    // Call StopVoice(voiceId, fadeDuration = 0.5f)
    AudioSystem::StopVoice(voiceId, 0.5f);
    const Voice& v = audio.GetVoice(voiceId);

    // Assert that voice.isFadingOut == true and the source remains active during the fade window
    CHECK(v.isFadingOut == true, "voice.isFadingOut must be true during fade-out");
    CHECK(audio.IsVoiceActive(voiceId), "Sound source must remain active during the fade window");

    // Step 0.25s (halfway through the fade duration)
    audio.Update(0.25f);
    CHECK(v.isFadingOut == true, "Voice must remain in fading out state halfway through");
    CHECK(audio.IsVoiceActive(voiceId), "Voice must still be active halfway through fade");
    CHECK(v.currentGain > 0.1f && v.currentGain < 0.9f, "currentGain must be partially attenuated");

    // Step the remaining 0.30s (past the 0.5f fade duration)
    audio.Update(0.30f);

    // Assert that voice stops completely once currentGain <= 0.0f
    CHECK(!audio.IsVoiceActive(voiceId), "Voice must stop completely once currentGain <= 0.0f");
    int sourceState = 0;
    alGetSourcei(v.source, AL_SOURCE_STATE, &sourceState);
    CHECK(sourceState == AL_STOPPED, "Underlying OpenAL source state must be AL_STOPPED");
}

// ── Test 5: Sound Cue Randomization ──
TEST(AudioSystemTest, SoundCueRandomization) {
    AudioSystem& audio = AudioSystem::instance();

    std::set<uint32_t> selectedBufferIds;
    std::vector<float> observedPitches;

    // Trigger randomized SoundCue containing multiple sound variants 50 times
    for (int i = 0; i < 50; ++i) {
        float pitch = 1.0f;
        uint32_t bufferId = 0;
        audio.TriggerRandomizedCue(SoundCue::VoidStalkerRoar, 1.0f, &pitch, &bufferId);

        selectedBufferIds.insert(bufferId);
        observedPitches.push_back(pitch);

        // Assert pitch values vary within defined bounds (±5% to ±10%, so [0.90, 1.10])
        CHECK(pitch >= 0.89f && pitch <= 1.11f, "Pitch jitter must stay within defined ±5% to ±10% bounds");
    }

    // Assert that multiple distinct buffer IDs are selected across executions
    CHECK(selectedBufferIds.size() >= 3, "Randomized SoundCue pool must select multiple distinct sample buffer IDs");

    // Assert that pitch values vary across executions
    bool hasVariation = false;
    for (size_t i = 1; i < observedPitches.size(); ++i) {
        if (std::abs(observedPitches[i] - observedPitches[0]) > 0.001f) {
            hasVariation = true;
            break;
        }
    }
    CHECK(hasVariation, "Pitch must exhibit non-zero procedural variance across executions");
}

// ── Test 6: Distance Attenuation Model Invariants ──
TEST(AudioSystemTest, DistanceAttenuationModel) {
    AudioSystem& audio = AudioSystem::instance();

    // Test AL_INVERSE_DISTANCE_CLAMPED at reference distance (4.0m)
    float gainAtRef = audio.calculate_distance_attenuation(4.0f, 4.0f, 65.0f, 1.2f);
    CHECK(std::abs(gainAtRef - 1.0f) < 0.001f, "Gain at reference distance (4m) must be 1.0");

    // Closer than reference distance should be clamped to 1.0
    float gainClose = audio.calculate_distance_attenuation(1.0f, 4.0f, 65.0f, 1.2f);
    CHECK(std::abs(gainClose - 1.0f) < 0.001f, "Gain closer than ref distance must clamp to 1.0");

    // Intermediate distance (e.g. 20m) should have smooth rolloff
    float gainMid = audio.calculate_distance_attenuation(20.0f, 4.0f, 65.0f, 1.2f);
    CHECK(gainMid < 1.0f && gainMid > 0.05f, "Gain at 20m must be between 0.05 and 1.0");

    // At or beyond max distance (65m), gain should be clamped
    float gainMax = audio.calculate_distance_attenuation(65.0f, 4.0f, 65.0f, 1.2f);
    float gainFar = audio.calculate_distance_attenuation(120.0f, 4.0f, 65.0f, 1.2f);
    CHECK(std::abs(gainMax - gainFar) < 0.0001f, "Distance beyond maxDistance (65m) must be clamped");
    CHECK(gainMax > 0.0f, "Clamped distance model should trail off smoothly rather than hard-cut");
}

int main(int argc, char** argv) {
    std::cout << "========================================================\n";
    std::cout << "  Voidfall Dredge: Audio Subsystem Regression Test Suite\n";
    std::cout << "========================================================\n";

    auto& tests = get_test_registry();
    int passed = 0;
    int failed = 0;

    for (const auto& test : tests) {
        std::cout << "[ RUN      ] " << test.name << "\n";
        try {
            test.func();
            std::cout << "[       OK ] " << test.name << "\n";
            passed++;
        } catch (const std::exception& e) {
            std::cerr << "[  FAILED  ] " << test.name << ": " << e.what() << "\n";
            failed++;
        } catch (...) {
            std::cerr << "[  FAILED  ] " << test.name << ": Unknown exception\n";
            failed++;
        }
    }

    std::cout << "--------------------------------------------------------\n";
    std::cout << "Total: " << tests.size() << " | Passed: " << passed << " | Failed: " << failed << "\n";

    return (failed == 0) ? 0 : 1;
}
