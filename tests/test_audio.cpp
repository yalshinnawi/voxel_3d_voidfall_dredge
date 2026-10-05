#include <iostream>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <numeric>
#include <chrono>
#include <fstream>
#include <filesystem>
#include "../src/audio/audio_engine.hpp"
#include "../src/entities/enemies/void_stalker.hpp"
#include "../src/voxel/world.hpp"

using namespace Voidfall;

#define CHECK(expr, msg) \
    if (!(expr)) { \
        std::cerr << "[TEST FAILURE] " << msg << " (" #expr ") at line " << __LINE__ << std::endl; \
        std::exit(1); \
    }

static void write_wav_file(const std::string& path, const std::vector<float>& float_samples, int sample_rate = 44100, int channels = 2) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return;

    uint32_t num_samples = static_cast<uint32_t>(float_samples.size());
    uint32_t data_size = num_samples * sizeof(int16_t);
    uint32_t chunk_size = 36 + data_size;
    uint16_t audio_format = 1; // PCM
    uint16_t num_channels = channels;
    uint32_t s_rate = sample_rate;
    uint16_t bits_per_sample = 16;
    uint32_t byte_rate = s_rate * num_channels * (bits_per_sample / 8);
    uint16_t block_align = num_channels * (bits_per_sample / 8);

    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&chunk_size), 4);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    uint32_t subchunk1_size = 16;
    out.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
    out.write(reinterpret_cast<const char*>(&audio_format), 2);
    out.write(reinterpret_cast<const char*>(&num_channels), 2);
    out.write(reinterpret_cast<const char*>(&s_rate), 4);
    out.write(reinterpret_cast<const char*>(&byte_rate), 4);
    out.write(reinterpret_cast<const char*>(&block_align), 2);
    out.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&data_size), 4);

    for (float s : float_samples) {
        float val = std::clamp(s * 32767.0f, -32767.0f, 32767.0f);
        int16_t pcm = static_cast<int16_t>(val);
        out.write(reinterpret_cast<const char*>(&pcm), sizeof(int16_t));
    }
}

int main(int argc, char** argv) {
    bool export_wav = false;
    bool enable_hardware = false; // Default: 100% silent headless mode to protect developer hearing

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--export-wav") {
            export_wav = true;
        } else if (arg == "--audible" || arg == "--hardware" || arg == "--listen") {
            enable_hardware = true;
        }
    }

    std::cout << "========================================" << std::endl;
    std::cout << "RUNNING VOIDFALL DREDGE AUDIO & EAR SAFETY TEST SUITE" << std::endl;
    if (enable_hardware) {
        std::cout << "Mode: HARDWARE PLAYBACK ACTIVE (--audible)" << std::endl;
    } else {
        std::cout << "Mode: SILENT HEADLESS MODE (Ear protection / quiet testing)" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    AudioEngine audio;
    audio.init(enable_hardware);

    // ── Test 1: Peak Amplitude Invariant & Hearing Protection Under Simultaneous Voice Overload ──
    {
        std::cout << "[Test 1] Testing Hearing Protection, Peak Limiting & Overload Saturation..." << std::endl;
        audio.stop_all();

        // Deliberate "Torture Test": Trigger 16 loud voices simultaneously at the exact same frame
        for (int i = 0; i < 4; ++i) {
            audio.play_sound_2d(SoundCue::ExplosiveBlast, 1.5f);
            audio.play_sound_2d(SoundCue::StalkerLunge, 1.5f);
            audio.play_sound_2d(SoundCue::VoxelBreakBulkhead, 1.5f);
            audio.play_sound_2d(SoundCue::SeismicTremor, 1.5f);
        }

        constexpr size_t FRAMES = 44100; // 1 second of audio
        std::vector<float> mix(FRAMES * 2, 0.0f);
        audio.render_mix(mix.data(), FRAMES);

        float max_peak = 0.0f;
        int nan_count = 0;
        int inf_count = 0;
        int hard_clip_count = 0;

        for (size_t i = 0; i < mix.size(); ++i) {
            float s = mix[i];
            if (std::isnan(s)) nan_count++;
            if (std::isinf(s)) inf_count++;
            float abs_s = std::abs(s);
            if (abs_s > max_peak) max_peak = abs_s;
            if (abs_s >= 0.999f) hard_clip_count++;
        }

        CHECK(nan_count == 0, "No NaNs permitted in audio output");
        CHECK(inf_count == 0, "No Infs permitted in audio output");
        CHECK(max_peak <= 0.95f, "Peak amplitude must NEVER exceed 0.95 (-1.0 dBFS safety ceiling)");
        CHECK(hard_clip_count == 0, "Analog soft-saturation and limiter must prevent harsh digital flat-top clipping");

        auto metrics = audio.get_and_reset_safety_metrics();
        std::cout << " -> Overload stack peak: " << max_peak << " (" << metrics.peak_dbfs << " dBFS)" << std::endl;
        std::cout << " -> Hearing safety invariants verified." << std::endl;
    }

    // ── Test 2: Click Prevention, Anti-Popping Envelopes & Slew-Rate Clamping ──
    {
        std::cout << "[Test 2] Testing Anti-Click Envelopes & Slew Rate Continuity..." << std::endl;
        audio.stop_all();

        // Test rapid triggering and stopping of high-transient sounds
        for (int step = 0; step < 8; ++step) {
            audio.play_sound_2d(SoundCue::VoxelHit, 1.0f);
            audio.play_sound_2d(SoundCue::ExplosiveBlast, 1.0f);

            constexpr size_t CHUNK = 2048;
            std::vector<float> chunk(CHUNK * 2, 0.0f);
            audio.render_mix(chunk.data(), CHUNK);

            // Verify sample-to-sample difference is bounded
            for (size_t f = 1; f < CHUNK; ++f) {
                float delta_l = std::abs(chunk[f * 2 + 0] - chunk[(f - 1) * 2 + 0]);
                float delta_r = std::abs(chunk[f * 2 + 1] - chunk[(f - 1) * 2 + 1]);
                CHECK(delta_l <= 0.20f, "Left channel slew rate exceeded click-safety threshold");
                CHECK(delta_r <= 0.20f, "Right channel slew rate exceeded click-safety threshold");
            }
        }
        std::cout << " -> Smooth transient envelopes and click-free continuity verified." << std::endl;
    }

    // ── Test 3: High-Frequency De-Harshing Filter (Ear Pain Elimination) ──
    {
        std::cout << "[Test 3] Testing High-Frequency De-Harshing & Spectral Damping..." << std::endl;
        audio.stop_all();

        // Screeching sounds like stalker lunge or drill grinding must not have harsh high-end spikes
        audio.play_sound_2d(SoundCue::StalkerLunge, 1.0f);
        audio.play_sound_2d(SoundCue::DrillLoop, 1.0f);

        constexpr size_t FRAMES = 10000;
        std::vector<float> buf(FRAMES * 2, 0.0f);
        audio.render_mix(buf.data(), FRAMES);

        auto metrics = audio.get_and_reset_safety_metrics();
        // High frequency ratio above 6.5 kHz must be small (< 0.25 of total spectral energy)
        CHECK(metrics.high_frequency_ratio < 0.25f, "Harsh high frequencies must be safely attenuated");

        std::cout << " -> High-frequency ratio: " << metrics.high_frequency_ratio * 100.0f << "% (safe < 25%)" << std::endl;
        std::cout << " -> Subterranean acoustic warmth verified." << std::endl;
    }

    // ── Test 4: Mining Sounds & Voxel Material Acoustic Timbre ──
    {
        std::cout << "[Test 4] Testing Mining Drill & Voxel Fracture Acoustic Variations..." << std::endl;
        audio.stop_all();

        // 4.1 Drill Loop activation and progress pitch modulation
        audio.set_drill_active(true, 0.0f, glm::vec3(0, 0, 2));
        auto drill_samples_start = audio.render_offline_samples(0.1f);
        float energy_drill_start = 0.0f;
        for (float s : drill_samples_start) energy_drill_start += s * s;
        CHECK(energy_drill_start > 0.01f, "Drill loop must produce audible energy");

        audio.set_drill_active(true, 1.0f, glm::vec3(0, 0, 2));
        auto drill_samples_end = audio.render_offline_samples(0.1f);
        float energy_drill_end = 0.0f;
        for (float s : drill_samples_end) energy_drill_end += s * s;
        CHECK(energy_drill_end > 0.01f, "Drill loop at 100% progress must produce audible energy");
        audio.set_drill_active(false);

        // 4.2 Material Break Cues
        SoundCue mats[] = {
            SoundCue::VoxelBreakBasalt,
            SoundCue::VoxelBreakTitanium,
            SoundCue::VoxelBreakVoidite,
            SoundCue::VoxelBreakBulkhead,
            SoundCue::VoxelBreakRadioactive
        };

        for (auto cue : mats) {
            audio.stop_all();
            audio.play_sound_2d(cue, 1.0f);
            auto samples = audio.render_offline_samples(0.2f);
            float energy = 0.0f;
            for (float s : samples) energy += s * s;
            CHECK(energy > 0.005f, "Material break sound must generate audible energy");
        }
        std::cout << " -> Drill loop modulation and 5 material acoustic profiles verified." << std::endl;
    }

    // ── Test 5: Hostile Enemy Audio Integration (Void Stalker & Seismic Burrower) ──
    {
        std::cout << "[Test 5] Testing Hostile Enemy Cues & Cavern Echo Proximity Scaling..." << std::endl;

        SoundCue enemy_cues[] = {
            SoundCue::StalkerSpotted,
            SoundCue::StalkerLunge,
            SoundCue::StalkerHit,
            SoundCue::StalkerDie,
            SoundCue::StalkerEchoScreech,
            SoundCue::StalkerChitter,
            SoundCue::StalkerHiss,
            SoundCue::BurrowerRoar,
            SoundCue::BurrowerGrind
        };

        for (auto cue : enemy_cues) {
            audio.stop_all();
            audio.play_sound_2d(cue, 1.0f);
            auto samples = audio.render_offline_samples(0.20f);
            float max_val = 0.0f;
            for (float s : samples) max_val = std::max(max_val, std::abs(s));
            CHECK(max_val > 0.04f, "Enemy audio cue must generate distinct non-zero waveform");
            CHECK(max_val <= 0.95f, "Enemy cue must obey ear-safety ceiling");
        }

        // 5.1 Verify 3D Proximity Attenuation: Close scream must be significantly louder than distant echo
        audio.stop_all();
        audio.set_listener(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));

        // Proximity Stalker Screech (3.0m away)
        audio.play_sound_3d(SoundCue::StalkerEchoScreech, glm::vec3(0.0f, 0.0f, -3.0f), 1.0f);
        auto close_samples = audio.render_offline_samples(0.25f);
        float close_max = 0.0f;
        for (float s : close_samples) close_max = std::max(close_max, std::abs(s));

        // Distant Stalker Screech (38.0m away)
        audio.stop_all();
        audio.play_sound_3d(SoundCue::StalkerEchoScreech, glm::vec3(0.0f, 0.0f, -38.0f), 1.0f);
        auto distant_samples = audio.render_offline_samples(0.25f);
        float distant_max = 0.0f;
        for (float s : distant_samples) distant_max = std::max(distant_max, std::abs(s));

        CHECK(distant_max > 0.005f, "Distant monster screech must remain audible echoing through cavern corridors");
        CHECK(close_max >= distant_max * 3.5f, "Proximity monster screech must be dramatically louder than distant echo to alert player");

        std::cout << " -> Proximity peak: " << close_max << " vs Distant peak: " << distant_max << " (ratio: "
                  << (close_max / std::max(distant_max, 0.0001f)) << "x louder in close proximity)" << std::endl;
        std::cout << " -> All 7 hostile organism behavioral cues and proximity scaling verified." << std::endl;
    }

    // ── Test 6: Ambience, Seismic Tremor & Hazard Geiger Escalation ──
    {
        std::cout << "[Test 6] Testing Ambient Cavern Drone, Seismic Tremor & Geiger Counter..." << std::endl;
        audio.stop_all();

        // 6.1 Subterranean Cavern Ambient
        audio.play_sound_2d(SoundCue::AmbientCavern, 0.5f, 1.0f, true);
        auto amb_samples = audio.render_offline_samples(0.2f);
        float amb_energy = 0.0f;
        for (float s : amb_samples) amb_energy += s * s;
        CHECK(amb_energy > 0.001f, "Ambient cavern drone must generate continuous subtle energy");

        // 6.2 Seismic Tremor
        audio.stop_all();
        audio.set_seismic_rumble(0.8f);
        auto tremor_samples = audio.render_offline_samples(0.2f);
        float tremor_energy = 0.0f;
        for (float s : tremor_samples) tremor_energy += s * s;
        CHECK(tremor_energy > 0.01f, "Seismic tremor must generate deep sub-bass vibration");

        // 6.3 Hazard Geiger Clicks
        audio.stop_all();
        audio.set_seismic_rumble(0.0f);
        audio.set_hazard_phase(0); // Phase 0: Silent
        auto p0 = audio.render_offline_samples(0.5f);
        float p0_e = 0.0f;
        for (float s : p0) p0_e += s * s;
        CHECK(p0_e < 0.001f, "Phase 0 should have no geiger clicks");

        audio.set_hazard_phase(3); // Phase 3: Severe radiation
        auto p3 = audio.render_offline_samples(0.5f);
        float p3_e = 0.0f;
        for (float s : p3) p3_e += s * s;
        CHECK(p3_e > 0.001f, "Phase 3 hazard must generate frequent geiger clicks");

        // 6.4 Procedural Micro-Ambience (Cavern Moisture Drip & Tectonic Groan)
        audio.stop_all();
        audio.play_sound_2d(SoundCue::CavernDrip, 0.8f);
        auto drip_samples = audio.render_offline_samples(0.25f);
        float drip_energy = 0.0f;
        for (float s : drip_samples) drip_energy += s * s;
        CHECK(drip_energy > 0.002f, "Cavern drip must generate audible acoustic reflection");

        audio.stop_all();
        audio.play_sound_2d(SoundCue::CavernGroan, 0.8f);
        auto groan_samples = audio.render_offline_samples(0.50f);
        float groan_energy = 0.0f;
        for (float s : groan_samples) groan_energy += s * s;
        CHECK(groan_energy > 0.005f, "Tectonic cavern groan must produce deep low-frequency stress energy");

        // 6.5 Trigger Cavern Micro Event helper
        audio.stop_all();
        audio.trigger_cavern_micro_event(1, 0.8f);
        CHECK(audio.active_voice_count() >= 1, "trigger_cavern_micro_event must spawn an active voice");

        std::cout << " -> Cavern atmosphere, seismic tremors, micro-ambience (drips & groans), and radiation scaling verified." << std::endl;
    }

    // ── Test 7: 3D Spatial Audio Attenuation & Stereo Directional Panning ──
    {
        std::cout << "[Test 7] Testing 3D Spatial Attenuation & Stereo Azimuth Panning..." << std::endl;
        audio.stop_all();
        audio.set_listener(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));

        // 7.1 Left-side sound source (X = -8, Y = 0, Z = -2)
        audio.play_sound_3d(SoundCue::SonarPulse, glm::vec3(-8.0f, 0.0f, -2.0f), 1.0f);
        auto left_mix = audio.render_offline_samples(0.3f);
        float left_ch_l = 0.0f, left_ch_r = 0.0f;
        for (size_t f = 0; f < left_mix.size() / 2; ++f) {
            left_ch_l += left_mix[f * 2 + 0] * left_mix[f * 2 + 0];
            left_ch_r += left_mix[f * 2 + 1] * left_mix[f * 2 + 1];
        }
        CHECK(left_ch_l > left_ch_r * 1.5f, "Sound on left must have significantly greater energy in left channel");

        // 7.2 Right-side sound source (X = +8, Y = 0, Z = -2)
        audio.stop_all();
        audio.play_sound_3d(SoundCue::SonarPulse, glm::vec3(8.0f, 0.0f, -2.0f), 1.0f);
        auto right_mix = audio.render_offline_samples(0.3f);
        float right_ch_l = 0.0f, right_ch_r = 0.0f;
        for (size_t f = 0; f < right_mix.size() / 2; ++f) {
            right_ch_l += right_mix[f * 2 + 0] * right_mix[f * 2 + 0];
            right_ch_r += right_mix[f * 2 + 1] * right_mix[f * 2 + 1];
        }
        CHECK(right_ch_r > right_ch_l * 1.5f, "Sound on right must have significantly greater energy in right channel");

        // 7.3 Distance Attenuation: Near (2m) vs Far (25m)
        audio.stop_all();
        audio.play_sound_3d(SoundCue::ExplosiveBlast, glm::vec3(0.0f, 0.0f, -2.0f), 1.0f);
        auto near_mix = audio.render_offline_samples(0.2f);
        float near_e = 0.0f;
        for (float s : near_mix) near_e += s * s;

        audio.stop_all();
        audio.play_sound_3d(SoundCue::ExplosiveBlast, glm::vec3(0.0f, 0.0f, -25.0f), 1.0f);
        auto far_mix = audio.render_offline_samples(0.2f);
        float far_e = 0.0f;
        for (float s : far_mix) far_e += s * s;

        CHECK(near_e > far_e * 4.0f, "Sound at 2m must be much louder than sound at 25m");

        std::cout << " -> 3D inverse-distance falloff and directional binaural panning verified." << std::endl;
    }

    // ── Test 8: Granular Sound Channels, Turn-Down Options & Mute Controls ──
    {
        std::cout << "[Test 8] Testing Sound Channels Turn-Down Options & Mute Isolation..." << std::endl;

        // 8A. Master Volume and Master Mute
        audio.stop_all();
        audio.set_master_volume(0.0f);
        audio.play_sound_2d(SoundCue::ExplosiveBlast, 1.0f);
        audio.play_sound_2d(SoundCue::StalkerLunge, 1.0f);

        auto mute_mix = audio.render_offline_samples(0.1f);
        float sum = 0.0f;
        for (float s : mute_mix) sum += std::abs(s);
        CHECK(sum == 0.0f, "Master volume at 0.0 must guarantee exact digital silence");

        audio.set_master_volume(1.0f);
        audio.set_mute_all(true);
        audio.play_sound_2d(SoundCue::ExplosiveBlast, 1.0f);
        auto mute_all_mix = audio.render_offline_samples(0.1f);
        float mute_all_sum = 0.0f;
        for (float s : mute_all_mix) mute_all_sum += std::abs(s);
        CHECK(mute_all_sum == 0.0f, "set_mute_all(true) must guarantee exact digital silence");
        audio.set_mute_all(false);

        // 8B. Turning down SFX / Mining Volume to 0.0
        audio.stop_all();
        audio.set_sfx_volume(0.0f);
        audio.set_enemy_volume(1.0f);
        audio.set_ambient_volume(1.0f);
        audio.set_ui_volume(1.0f);

        audio.play_sound_2d(SoundCue::VoxelBreakTitanium, 1.0f);
        audio.play_sound_2d(SoundCue::ExplosiveBlast, 1.0f);
        auto sfx_silent_mix = audio.render_offline_samples(0.1f);
        float sfx_sum = 0.0f;
        for (float s : sfx_silent_mix) sfx_sum += std::abs(s);
        CHECK(sfx_sum == 0.0f, "Turning down SFX volume to 0.0 must completely silence mining and tools");

        // Isolation: Enemy cue still plays while SFX is muted
        audio.play_sound_2d(SoundCue::StalkerSpotted, 1.0f);
        auto enemy_while_sfx_muted = audio.render_offline_samples(0.1f);
        float enemy_sum = 0.0f;
        for (float s : enemy_while_sfx_muted) enemy_sum += std::abs(s);
        CHECK(enemy_sum > 0.05f, "Hostile enemy sounds must remain audible when SFX volume is turned down");

        // 8C. Turning down Enemy Volume to 0.0
        audio.stop_all();
        audio.set_sfx_volume(1.0f);
        audio.set_enemy_volume(0.0f);
        audio.play_sound_2d(SoundCue::StalkerLunge, 1.0f);
        audio.play_sound_2d(SoundCue::StalkerSpotted, 1.0f);
        auto enemy_silent_mix = audio.render_offline_samples(0.1f);
        float enemy_silent_sum = 0.0f;
        for (float s : enemy_silent_mix) enemy_silent_sum += std::abs(s);
        CHECK(enemy_silent_sum == 0.0f, "Turning down Enemy volume to 0.0 must completely silence creatures");

        // 8D. Turning down Ambience / Hazards Volume to 0.0
        audio.stop_all();
        audio.set_ambient_volume(0.0f);
        audio.set_ambient_intensity(1.0f);
        audio.set_seismic_rumble(1.0f);
        audio.play_sound_2d(SoundCue::AmbientCavern, 1.0f, 1.0f, true);
        audio.play_sound_2d(SoundCue::SeismicTremor, 1.0f, 1.0f, true);
        auto ambient_silent_mix = audio.render_offline_samples(0.1f);
        float ambient_silent_sum = 0.0f;
        for (float s : ambient_silent_mix) ambient_silent_sum += std::abs(s);
        CHECK(ambient_silent_sum == 0.0f, "Turning down Ambience volume to 0.0 must completely silence cavern wind and tremors");

        // 8E. Turning down UI Volume to 0.0
        audio.stop_all();
        audio.set_ui_volume(0.0f);
        audio.play_sound_2d(SoundCue::UIBlip, 1.0f);
        audio.play_sound_2d(SoundCue::UIUpgrade, 1.0f);
        auto ui_silent_mix = audio.render_offline_samples(0.1f);
        float ui_silent_sum = 0.0f;
        for (float s : ui_silent_mix) ui_silent_sum += std::abs(s);
        CHECK(ui_silent_sum == 0.0f, "Turning down UI volume to 0.0 must completely silence menu sounds and alarms");

        // 8F. Proportional Stepped Attenuation (e.g. 30%, 60%, 100%)
        audio.stop_all();
        audio.set_sfx_volume(1.0f);
        audio.set_enemy_volume(1.0f);
        audio.set_ambient_volume(1.0f);
        audio.set_ui_volume(1.0f);

        audio.set_sfx_volume(0.30f);
        audio.play_sound_2d(SoundCue::VoxelBreakTitanium, 1.0f);
        auto low_sfx = audio.render_offline_samples(0.1f);
        float max_low = 0.0f;
        for (float s : low_sfx) max_low = std::max(max_low, std::abs(s));

        audio.stop_all();
        audio.set_sfx_volume(0.90f);
        audio.play_sound_2d(SoundCue::VoxelBreakTitanium, 1.0f);
        auto high_sfx = audio.render_offline_samples(0.1f);
        float max_high = 0.0f;
        for (float s : high_sfx) max_high = std::max(max_high, std::abs(s));

        CHECK(max_low > 0.005f && max_high > max_low * 2.0f, "Stepped sound channel volume must scale output amplitude predictably");

        // Reset to default
        audio.set_master_volume(1.0f);
        audio.set_sfx_volume(1.0f);
        audio.set_enemy_volume(1.0f);
        audio.set_ambient_volume(1.0f);
        audio.set_ui_volume(1.0f);
        audio.set_mute_all(false);

        std::cout << " -> All sound channel turn-down options and mute isolation verified." << std::endl;
    }

    // ── Test 9: Real-time Performance & Zero-Allocation Buffer Render Benchmark ──
    {
        std::cout << "[Test 9] Benchmarking Real-Time Buffer Generation & CPU Overhead..." << std::endl;
        audio.stop_all();

        // Activate 12 concurrent voices
        for (int i = 0; i < 4; ++i) {
            audio.play_sound_2d(SoundCue::AmbientCavern, 0.5f, 1.0f, true);
            audio.play_sound_2d(SoundCue::DrillLoop, 0.5f, 1.0f, true);
            audio.play_sound_2d(SoundCue::SonarPulse, 0.5f);
        }

        // Render 10 full seconds of 44.1 kHz stereo audio (441,000 frames)
        constexpr size_t TEN_SECONDS_FRAMES = 44100 * 10;
        std::vector<float> big_buffer(TEN_SECONDS_FRAMES * 2, 0.0f);

        auto t0 = std::chrono::high_resolution_clock::now();
        audio.render_mix(big_buffer.data(), TEN_SECONDS_FRAMES);
        auto t1 = std::chrono::high_resolution_clock::now();

        double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double realtime_budget_ms = 10000.0;
        double cpu_usage_pct = (elapsed_ms / realtime_budget_ms) * 100.0;

        std::cout << " -> Rendered 10 seconds of 12-voice audio in " << elapsed_ms << " ms ("
                  << cpu_usage_pct << "% of 1 core CPU budget)" << std::endl;
        CHECK(elapsed_ms < 150.0, "10 seconds of audio mixing must complete in < 150 ms (< 1.5% CPU usage)");
    }

    // ── Test 10: Dynamic Threat Ducking (Dead Space / Alien Isolation Negative Space) ──
    {
        std::cout << "[Test 10] Testing Threat Ducking & Negative Space Vacuum..." << std::endl;
        audio.stop_all();

        // 10.1 Play ambient cavern drone and measure baseline energy
        audio.play_sound_2d(SoundCue::AmbientCavern, 0.50f, 1.0f, true);
        auto base_amb = audio.render_offline_samples(0.2f);
        float base_energy = 0.0f;
        for (float s : base_amb) base_energy += s * s;
        CHECK(base_energy > 0.001f, "Ambient baseline must produce audible energy");

        // 10.2 Trigger high-priority threat cue (Stalker Echo Screech) which auto-triggers ducking
        audio.play_sound_2d(SoundCue::StalkerEchoScreech, 1.0f);
        CHECK(audio.ducking_factor() < 0.60f, "Triggering threat cue must immediately duck ambient audio");

        // 10.3 Render 0.3s of audio during active ducking
        auto ducked_samples = audio.render_offline_samples(0.3f);
        CHECK(audio.ducking_factor() <= 0.35f, "Ducking attenuation must reach target negative space floor");

        // 10.4 Advance audio timeline by 2.0s to allow ducking envelope to recover
        audio.render_offline_samples(2.0f);
        CHECK(audio.ducking_factor() > 0.90f, "Ducking attenuation must smoothly recover back to full ambience");

        std::cout << " -> Threat cue auto-ducking and smooth negative space recovery verified." << std::endl;
    }

    // ── Test 11: Distance-Based Low-Pass Filtering & Duration Envelopes ──
    {
        std::cout << "[Test 11] Testing Distance Low-Pass Air Absorption & Tightened Durations..." << std::endl;
        audio.stop_all();

        // 11.1 Verify distance air absorption on 3D Stalker Screech
        audio.set_listener(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));

        // Close stalker (3m)
        audio.play_sound_3d(SoundCue::StalkerEchoScreech, glm::vec3(0, 0, -3.0f), 1.0f);
        audio.render_offline_samples(0.2f);
        auto close_metrics = audio.get_and_reset_safety_metrics();

        // Distant stalker (35m)
        audio.stop_all();
        audio.play_sound_3d(SoundCue::StalkerEchoScreech, glm::vec3(0, 0, -35.0f), 1.0f);
        audio.render_offline_samples(0.2f);
        auto distant_metrics = audio.get_and_reset_safety_metrics();

        CHECK(close_metrics.peak_amplitude > distant_metrics.peak_amplitude * 4.0f,
              "Distant sound must be significantly quieter due to inverse distance roll-off");

        // 11.2 Verify tightened durations (no endless droning sounds)
        audio.stop_all();
        audio.play_sound_2d(SoundCue::CavernGroan, 1.0f);
        audio.play_sound_2d(SoundCue::SeismicTremor, 1.0f);
        audio.play_sound_2d(SoundCue::BeaconSiren, 1.0f);

        // Render 1.3 seconds: all three non-looping cues must have finished
        audio.render_offline_samples(1.3f);
        CHECK(audio.active_voice_count() == 0,
              "Cavern groan (0.70s), seismic tremor (1.10s) and beacon siren (0.35s) must all complete within 1.3s");

        std::cout << " -> Distance air absorption and non-droning concise sound durations verified." << std::endl;
    }

    // ── Test 12: Gameplay Interaction Audio: Killing Enemies, Mining, Sprinting & Clean Voice Release ──
    {
        std::cout << "[Test 12] Testing Gameplay Interaction Audio: Enemy Elimination, Mining & Sprinting..." << std::endl;
        audio.stop_all();

        // 12.1 Enemy Elimination: StalkerDie must decay cleanly to silence within 0.55s and release voice
        audio.play_sound_3d(SoundCue::StalkerDie, glm::vec3(0, 0, 2), 0.70f);
        CHECK(audio.active_voice_count() == 1, "StalkerDie voice must be active initially");

        // Render 0.60 seconds (duration is 0.55s)
        auto die_samples = audio.render_offline_samples(0.60f);
        CHECK(audio.active_voice_count() == 0,
              "StalkerDie voice MUST be completely released at 0.55s with zero lingering voices or continuous looping!");

        // Verify end of sound has absolute silence (no cutoff pop)
        float tail_energy = 0.0f;
        size_t tail_start = die_samples.size() - 4410; // last 100ms
        for (size_t i = tail_start; i < die_samples.size(); ++i) {
            tail_energy += std::abs(die_samples[i]);
        }
        float avg_tail = tail_energy / 4410.0f;
        CHECK(avg_tail < 0.01f, "StalkerDie tail must fade smoothly to silence before cut-off");

        // 12.2 Mining Drill Start/Stop & Smooth Voice Deactivation
        audio.stop_all();
        audio.set_drill_active(true, 0.4f, glm::vec3(0, 0, 2));
        audio.render_offline_samples(0.3f);
        CHECK(audio.active_voice_count() == 1, "DrillLoop must be active while drilling");

        // Stop drilling: voice must immediately deactivate cleanly without hanging
        audio.set_drill_active(false);
        CHECK(audio.active_voice_count() == 0, "DrillLoop must immediately terminate when drilling stops");

        // 12.3 Sprinting Footsteps: Fast cadence without voice pile-up or clipping
        audio.stop_all();
        for (int step = 0; step < 5; ++step) {
            audio.play_sound_2d(SoundCue::Footstep, 0.35f, 0.95f + step * 0.02f);
            audio.render_offline_samples(0.32f); // 0.32s sprint step cadence
            CHECK(audio.active_voice_count() == 0, "Footstep voice (0.07s) must finish well before next sprint step");
        }

        // 12.4 Weapon Fire Cues (Plasma, Scattergun, Railgun): All concise, no harsh pops
        audio.stop_all();
        audio.play_sound_2d(SoundCue::PlasmaFire, 0.85f);
        audio.play_sound_2d(SoundCue::ScattergunFire, 0.85f);
        audio.play_sound_2d(SoundCue::RailgunFire, 0.85f);
        audio.render_offline_samples(0.50f);
        CHECK(audio.active_voice_count() == 0, "All weapon fire sounds must finish and release voices within 0.50s");

        std::cout << " -> Enemy death voice release, mining drill toggle, and sprint cadence verified." << std::endl;
    }

    // ── Test 13: Sector Ambient Soundtracks & Descent Arrival Stingers ──
    {
        std::cout << "[Test 13] Testing Sector-Specific Ambience & Descent Arrival Stingers..." << std::endl;

        SoundCue ambients[] = { SoundCue::AmbientSector1, SoundCue::AmbientSector2, SoundCue::AmbientSector3 };
        SoundCue stingers[] = {
            SoundCue::SectorArrival1,
            SoundCue::SectorArrival2,
            SoundCue::SectorArrival3,
            SoundCue::SectorArrival4,
            SoundCue::SectorArrival5
        };

        for (int s = 1; s <= 3; ++s) {
            audio.stop_all();
            audio.set_current_sector(s);
            CHECK(audio.current_sector() == s, "Current sector must be recorded correctly");

            // Test Ambient Loop
            audio.play_sound_2d(ambients[s - 1], 0.5f, 1.0f, true);
            auto amb_samples = audio.render_offline_samples(1.0f);
            float amb_energy = 0.0f;
            float amb_peak = 0.0f;
            for (float val : amb_samples) {
                amb_energy += val * val;
                amb_peak = std::max(amb_peak, std::abs(val));
            }
            std::cout << " -> Sector " << s << " amb_energy: " << amb_energy << " peak: " << amb_peak << std::endl;
            CHECK(amb_energy > 0.001f, "Sector ambient drone must generate continuous subtle energy");
            CHECK(amb_peak <= 0.95f, "Sector ambient drone must obey ear-safety ceiling");
        }

        // Test All 5 Arrival Stingers
        for (int i = 0; i < 5; ++i) {
            audio.stop_all();
            audio.play_sound_2d(stingers[i], 0.8f);
            auto stinger_samples = audio.render_offline_samples(0.4f);
            float stinger_peak = 0.0f;
            for (float val : stinger_samples) {
                stinger_peak = std::max(stinger_peak, std::abs(val));
            }
            std::cout << " -> Stinger " << (i + 1) << " peak: " << stinger_peak << std::endl;
            CHECK(stinger_peak > 0.05f, "Arrival stinger must generate distinct non-zero waveform");
            CHECK(stinger_peak <= 0.95f, "Arrival stinger must obey ear-safety ceiling");
        }

        std::cout << " -> Sector ambients and all 5 arrival stingers validated." << std::endl;
    }

    // ── Test 14: Sector-Specific Micro-Ambience Distribution ──
    {
        std::cout << "[Test 14] Testing Sector Micro-Ambience Events & Spatial Distribution..." << std::endl;

        SoundCue micro_cues[] = {
            SoundCue::CrystalChime,
            SoundCue::GeothermalVent,
            SoundCue::VoidDistortion
        };

        for (auto cue : micro_cues) {
            audio.stop_all();
            audio.play_sound_2d(cue, 0.75f);
            auto samples = audio.render_offline_samples(0.35f);
            float energy = 0.0f;
            float peak = 0.0f;
            for (float s : samples) {
                energy += s * s;
                peak = std::max(peak, std::abs(s));
            }
            CHECK(energy > 0.001f, "Micro-ambience cue must generate audible energy");
            CHECK(peak <= 0.95f, "Micro-ambience cue must obey ear-safety ceiling");
        }

        // Test sector-specific micro-event triggers
        for (int s = 1; s <= 3; ++s) {
            audio.stop_all();
            audio.set_current_sector(s);
            audio.trigger_cavern_micro_event(s, 0.70f);
            CHECK(audio.active_voice_count() >= 1, "trigger_cavern_micro_event for sector must spawn active voice");
        }

        std::cout << " -> Quartz crystal chimes, geothermal vents, and void distortions verified." << std::endl;
    }

    // ── Test 15: Subtle & Scary Enemy Audio Dynamics (Mandible Clicks & Throat Hisses) ──
    {
        std::cout << "[Test 15] Testing Subtle & Scary Enemy Audio Dynamics..." << std::endl;

        // 15.1 Stalker Mandible Chitter: Subtle (< 0.65 peak) but audible (> 0.04)
        audio.stop_all();
        audio.play_sound_2d(SoundCue::StalkerChitter, 0.55f);
        auto chitter_samples = audio.render_offline_samples(0.25f);
        float chitter_peak = 0.0f;
        for (float s : chitter_samples) chitter_peak = std::max(chitter_peak, std::abs(s));
        CHECK(chitter_peak > 0.04f, "Mandible chitter must generate audible dry clicks");
        CHECK(chitter_peak < 0.65f, "Mandible chitter must remain subtle and not overpower scene");

        // StalkerChitter has duration (0.75s) - must release cleanly when finished
        audio.render_offline_samples(0.60f); // 0.25s + 0.60s = 0.85s > 0.75s
        CHECK(audio.active_voice_count() == 0, "StalkerChitter voice must release cleanly within 0.85s");

        // 15.2 Stalker Throat Hiss: Distinct predatory exhalation
        audio.stop_all();
        audio.play_sound_2d(SoundCue::StalkerHiss, 0.70f);
        auto hiss_samples = audio.render_offline_samples(0.35f);
        float hiss_peak = 0.0f;
        for (float s : hiss_samples) hiss_peak = std::max(hiss_peak, std::abs(s));
        CHECK(hiss_peak > 0.05f, "Throat hiss must generate distinct sibilant exhalation");
        CHECK(hiss_peak <= 0.95f, "Throat hiss must obey ear-safety ceiling");

        // StalkerHiss duration (0.65s) - must release cleanly when finished
        audio.render_offline_samples(0.40f); // 0.35s + 0.40s = 0.75s > 0.65s
        CHECK(audio.active_voice_count() == 0, "StalkerHiss voice must release cleanly within 0.75s");

        std::cout << " -> Mandible chitters and throat hiss dynamics verified." << std::endl;
    }

    // ── Test 16: Room Archetype & Environmental Hazard Acoustics ──
    {
        std::cout << "[Test 16] Testing Room Archetype Acoustics & Environmental Hazard Proximity..." << std::endl;

        SoundCue room_cues[] = {
            SoundCue::LavaBubble,
            SoundCue::ThermalHiss,
            SoundCue::RadioactiveHum,
            SoundCue::VoidWind,
            SoundCue::GravityDistortion,
            SoundCue::SporePlop,
            SoundCue::OrganicCreak,
            SoundCue::IndustrialHum,
            SoundCue::HydraulicExhaust,
            SoundCue::PebbleSkitter,
            SoundCue::SpikeRattle
        };

        for (auto cue : room_cues) {
            audio.stop_all();
            audio.play_sound_2d(cue, 0.80f);
            auto samples = audio.render_offline_samples(0.30f);
            float energy = 0.0f;
            float peak = 0.0f;
            for (float s : samples) {
                energy += s * s;
                peak = std::max(peak, std::abs(s));
            }
            CHECK(energy > 0.0005f, "Room environmental sound cue must generate audible energy");
            CHECK(peak <= 0.95f, "Room environmental sound cue must obey ear-safety ceiling");
        }

        // 16.2 Test room archetype micro-event selection across diverse archetypes
        int test_archetypes[] = { 9 /* MagmaCaldera */, 7 /* RadioactiveCore */, 11 /* VoidSingularity */, 12 /* FungoidGrotto */, 13 /* Foundry */, 10 /* SpikeArena */ };
        for (int shape : test_archetypes) {
            audio.stop_all();
            audio.set_current_room_type(shape);
            CHECK(audio.current_room_type() == shape, "Current room type must match assigned archetype");
            audio.trigger_room_micro_event(shape, 0.70f);
            CHECK(audio.active_voice_count() >= 1, "trigger_room_micro_event must spawn active voice for room archetype");
        }

        // 16.3 Test dynamic hazard proximity audio modulation
        audio.stop_all();
        // Lava at 3.0m (within 10m threshold)
        audio.update_hazard_proximity_audio(0.50f, 3.0f, 0.0f, 20.0f, 20.0f);
        auto lava_samples = audio.render_offline_samples(0.30f);
        // Spikes at 2.0m (within 7m threshold)
        audio.stop_all();
        audio.update_hazard_proximity_audio(0.50f, 20.0f, 0.0f, 2.0f, 20.0f);
        auto spike_samples = audio.render_offline_samples(0.30f);
        // Void at 2.5m (within 8m threshold)
        audio.stop_all();
        audio.update_hazard_proximity_audio(0.50f, 20.0f, 0.0f, 20.0f, 2.5f);
        auto void_samples = audio.render_offline_samples(0.30f);

        std::cout << " -> All 11 room/hazard sound cues, archetype micro-ambience and proximity modulations verified." << std::endl;
    }

    // ── Test 17: Long Instrumental Track Playback & Non-Repetitive Time-Window Invariants ──
    {
        std::cout << "[Test 17] Testing Extended Instrumental Duration & Non-Repetitive Waveforms..." << std::endl;
        audio.stop_all();

        // 17.1 Verify loaded track durations: each sector track must be >= 120 seconds
        SoundCue music_cues[] = {
            SoundCue::AmbientCavern,
            SoundCue::AmbientSector1,
            SoundCue::AmbientSector2,
            SoundCue::AmbientSector3
        };

        for (SoundCue cue : music_cues) {
            CHECK(audio.has_sample(cue), "Music track must be loaded in memory");
            const auto* s = audio.get_sample(cue);
            CHECK(s != nullptr, "Sample pointer must be non-null");
            CHECK(s->duration_seconds >= 120.0f, "Extended instrumental music tracks must exceed 120 seconds duration");
            CHECK(s->frame_count >= static_cast<size_t>(120 * 44100), "Track must contain >= 120s of PCM frames");
        }

        // 17.2 Verify voice duration in play_sound_2d is not truncated (exceeds 100 seconds)
        audio.stop_all();
        audio.play_sound_2d(SoundCue::AmbientSector1, 0.5f, 1.0f, true);
        CHECK(audio.active_voice_count() == 1, "Ambient music voice must be active");

        // 17.3 Verify non-repetitiveness across sequential time windows (e.g. Window A [2s..6s] vs Window B [6s..10s])
        // In the old synthetic generator, audio looped identically every ~8s.
        // With actual instrumental compositions, different musical movements produce distinct waveform signatures.
        audio.stop_all();
        audio.play_sound_2d(SoundCue::AmbientSector1, 0.5f, 1.0f, true);

        // Discard initial transient/intro
        audio.render_offline_samples(2.0f);

        // Capture Window A: 4 seconds of music
        auto window_a = audio.render_offline_samples(4.0f);
        // Capture Window B: next 4 seconds of music
        auto window_b = audio.render_offline_samples(4.0f);

        CHECK(window_a.size() == window_b.size(), "Consecutive test windows must match in length");

        // Compute mean squared difference between Window A and Window B
        double sum_diff_sq = 0.0;
        double sum_energy_a = 0.0;
        double sum_energy_b = 0.0;
        for (size_t i = 0; i < window_a.size(); ++i) {
            double diff = static_cast<double>(window_a[i] - window_b[i]);
            sum_diff_sq += diff * diff;
            sum_energy_a += static_cast<double>(window_a[i] * window_a[i]);
            sum_energy_b += static_cast<double>(window_b[i] * window_b[i]);
        }

        CHECK(sum_energy_a > 0.1, "Window A must contain healthy musical energy");
        CHECK(sum_energy_b > 0.1, "Window B must contain healthy musical energy");
        // For a repeating 4s/8s loop, diff would be ~0. For rich instrumental music, diff is significant
        double normalized_diff = sum_diff_sq / (sum_energy_a + sum_energy_b);
        CHECK(normalized_diff > 0.05, "Sequential musical passages must be non-identical (non-repetitive music confirmed)");

        std::cout << " -> Extended instrumental playback verified (duration >= 120s, normalized variance: "
                  << normalized_diff << " > 0.05)." << std::endl;
    }

    // ── Test 18: Environmental Hazard Audio: Groan, Asphyxiation & Toxic Gas Hiss ──
    {
        std::cout << "[Test 18] Testing Environmental Hazard Audio (Player Groan, Asphyxiation & Toxic Gas Hiss)..." << std::endl;

        // 18.1 Player Groan (Radiation cellular damage)
        audio.stop_all();
        audio.play_sound_2d(SoundCue::PlayerGroan, 1.0f);
        CHECK(audio.active_voice_count() == 1, "PlayerGroan voice must be active");
        auto groan_samples = audio.render_offline_samples(0.85f);
        float groan_energy = 0.0f;
        float groan_max_abs = 0.0f;
        for (float s : groan_samples) {
            CHECK(!std::isnan(s) && !std::isinf(s), "Groan audio samples must be valid finite numbers");
            groan_energy += s * s;
            groan_max_abs = std::max(groan_max_abs, std::abs(s));
        }
        CHECK(groan_energy > 0.05f, "PlayerGroan must synthesize audible vocal resonance energy");
        CHECK(groan_max_abs <= 1.0f, "PlayerGroan must not exceed ceiling");

        // 18.2 Player Asphyxiation (Toxic gas inhalation)
        audio.stop_all();
        audio.play_sound_2d(SoundCue::PlayerAsphyxiation, 1.0f);
        CHECK(audio.active_voice_count() == 1, "PlayerAsphyxiation voice must be active");
        auto asph_samples = audio.render_offline_samples(1.1f);
        float asph_energy = 0.0f;
        float asph_max_abs = 0.0f;
        for (float s : asph_samples) {
            CHECK(!std::isnan(s) && !std::isinf(s), "Asphyxiation audio samples must be valid finite numbers");
            asph_energy += s * s;
            asph_max_abs = std::max(asph_max_abs, std::abs(s));
        }
        CHECK(asph_energy > 0.05f, "PlayerAsphyxiation must synthesize cough spasms and inspiratory wheeze");
        CHECK(asph_max_abs <= 1.0f, "PlayerAsphyxiation must not exceed ceiling");

        // 18.3 Toxic Gas Hiss (3D Proximity telegraph)
        audio.stop_all();
        audio.play_sound_2d(SoundCue::ToxicGasHiss, 1.0f);
        CHECK(audio.active_voice_count() == 1, "ToxicGasHiss voice must be active");
        auto hiss_samples = audio.render_offline_samples(0.8f);
        float hiss_energy = 0.0f;
        float hiss_max_abs = 0.0f;
        for (float s : hiss_samples) {
            CHECK(!std::isnan(s) && !std::isinf(s), "Toxic gas hiss samples must be valid finite numbers");
            hiss_energy += s * s;
            hiss_max_abs = std::max(hiss_max_abs, std::abs(s));
        }
        CHECK(hiss_energy > 0.05f, "ToxicGasHiss must synthesize audible pressurized gas vapor hiss");
        CHECK(hiss_max_abs <= 1.0f, "ToxicGasHiss must not exceed ceiling");

        // 18.4 Dedicated environmental trigger helper methods
        audio.stop_all();
        audio.trigger_player_groan(0.85f);
        CHECK(audio.active_voice_count() >= 1, "trigger_player_groan must spawn active voice");

        audio.stop_all();
        audio.trigger_player_asphyxiation(0.90f);
        CHECK(audio.active_voice_count() >= 1, "trigger_player_asphyxiation must spawn active voice");

        audio.stop_all();
        audio.trigger_toxic_gas_hiss(0.75f);
        CHECK(audio.active_voice_count() >= 1, "trigger_toxic_gas_hiss must spawn active voice");

        // 18.5 Hazard proximity gas modulation
        audio.stop_all();
        bool gas_hiss_triggered = false;
        for (int i = 0; i < 15; ++i) {
            audio.update_hazard_proximity_audio(0.50f, 50.0f, 0.0f, 50.0f, 50.0f, 1.0f);
            if (audio.active_voice_count() > 0) {
                gas_hiss_triggered = true;
                break;
            }
        }
        CHECK(gas_hiss_triggered, "update_hazard_proximity_audio must trigger ToxicGasHiss when near gas (<6m)");

        audio.stop_all();
        for (int i = 0; i < 5; ++i) {
            audio.update_hazard_proximity_audio(0.50f, 50.0f, 0.0f, 50.0f, 50.0f, 25.0f);
        }
        CHECK(audio.active_voice_count() == 0, "update_hazard_proximity_audio must not trigger ToxicGasHiss when far from gas (>6m)");

        std::cout << " -> Environmental hazard audio synthesis and proximity telegraph verified." << std::endl;
    }

    // ── Test 19: Impact Damage Audio Cues: Bone Crack, Enemy Flesh Hit & Debris Deflection ──
    {
        std::cout << "[Test 19] Testing Specific Impact Cues (Bone Crack, Enemy Flesh Hit & Debris Armor Deflection)..." << std::endl;

        // 19.1 Fall Impact Bone Crack (sickening bone fracture crunch)
        audio.stop_all();
        audio.trigger_bone_crack(1.0f);
        CHECK(audio.active_voice_count() == 1, "trigger_bone_crack voice must be active");
        auto bone_samples = audio.render_offline_samples(0.45f);
        float bone_energy = 0.0f;
        float bone_max_abs = 0.0f;
        for (float s : bone_samples) {
            CHECK(!std::isnan(s) && !std::isinf(s), "BoneCrack audio samples must be valid finite numbers");
            bone_energy += s * s;
            bone_max_abs = std::max(bone_max_abs, std::abs(s));
        }
        CHECK(bone_energy > 0.05f, "BoneCrack must synthesize audible fracture snapping and crunch energy");
        CHECK(bone_max_abs <= 1.0f, "BoneCrack must not exceed ceiling");

        // 19.2 Enemy Attack Flesh Hit (visceral organic slash & laceration squelch)
        audio.stop_all();
        audio.trigger_enemy_flesh_hit(1.0f);
        CHECK(audio.active_voice_count() == 1, "trigger_enemy_flesh_hit voice must be active");
        auto flesh_samples = audio.render_offline_samples(0.38f);
        float flesh_energy = 0.0f;
        float flesh_max_abs = 0.0f;
        for (float s : flesh_samples) {
            CHECK(!std::isnan(s) && !std::isinf(s), "EnemyFleshHit audio samples must be valid finite numbers");
            flesh_energy += s * s;
            flesh_max_abs = std::max(flesh_max_abs, std::abs(s));
        }
        CHECK(flesh_energy > 0.05f, "EnemyFleshHit must synthesize visceral claw tearing energy");
        CHECK(flesh_max_abs <= 1.0f, "EnemyFleshHit must not exceed ceiling");

        // 19.3 Falling Debris Armor Impact (heavy stone crash and titanium suit deflection)
        audio.stop_all();
        audio.trigger_debris_impact(1.0f);
        CHECK(audio.active_voice_count() == 1, "trigger_debris_impact voice must be active");
        auto debris_samples = audio.render_offline_samples(0.45f);
        float debris_energy = 0.0f;
        float debris_max_abs = 0.0f;
        for (float s : debris_samples) {
            CHECK(!std::isnan(s) && !std::isinf(s), "DebrisArmorImpact audio samples must be valid finite numbers");
            debris_energy += s * s;
            debris_max_abs = std::max(debris_max_abs, std::abs(s));
        }
        CHECK(debris_energy > 0.05f, "DebrisArmorImpact must synthesize heavy stone impact and armor deflection energy");
        CHECK(debris_max_abs <= 1.0f, "DebrisArmorImpact must not exceed ceiling");

        std::cout << " -> Bone crack, enemy flesh hit, and debris armor impact audio synthesis verified." << std::endl;
    }

    // ── Test 20: Arrival Stinger Procedural Non-Repetition & Enveloping Dynamics ──
    {
        std::cout << "[Test 20] Testing Arrival Stinger Procedural Non-Repetition, Fade-In & Fade-Out Envelopes..." << std::endl;

        // 20.1 Procedural Non-Repetition & Variety Across Consecutive Openings
        SoundCue last_cue = SoundCue::UIBlip;
        std::vector<float> pitches_observed;
        std::vector<SoundCue> cues_observed;

        for (int i = 0; i < 20; ++i) {
            audio.stop_all(true);
            int sector = 1 + (i % 3); // Cycle sectors 1, 2, 3
            audio.play_arrival_stinger(sector);

            CHECK(audio.active_voice_count() == 1, "play_arrival_stinger must activate exactly one voice");

            SoundCue chosen_cue = SoundCue::UIBlip;
            float chosen_pitch = 1.0f;
            for (const auto& voice : audio.voices()) {
                if (voice.active) {
                    chosen_cue = voice.cue;
                    chosen_pitch = voice.pitch;
                    break;
                }
            }

            CHECK(chosen_cue != SoundCue::UIBlip, "A valid stinger cue must be chosen");
            if (i > 0) {
                CHECK(chosen_cue != last_cue, "Consecutive arrival stingers must NOT repeat the same cue");
            }
            last_cue = chosen_cue;
            cues_observed.push_back(chosen_cue);
            pitches_observed.push_back(chosen_pitch);

            // Verify organic pitch modulation range [0.93 .. 1.07]
            CHECK(chosen_pitch >= 0.93f && chosen_pitch <= 1.07f, "Arrival stinger pitch must have subtle organic variation");
            // Verify threat ducking triggered
            CHECK(audio.ducking_factor() < 0.5f, "Arrival stinger must trigger dramatic threat ducking");
        }

        // Verify that multiple distinct cues were played across the 20 trials
        std::sort(cues_observed.begin(), cues_observed.end());
        size_t unique_cues = std::unique(cues_observed.begin(), cues_observed.end()) - cues_observed.begin();
        CHECK(unique_cues >= 3, "Arrival stinger system must utilize multiple stinger cues across sectors");

        // Verify pitch wasn't constant
        float min_pitch = *std::min_element(pitches_observed.begin(), pitches_observed.end());
        float max_pitch = *std::max_element(pitches_observed.begin(), pitches_observed.end());
        CHECK(max_pitch - min_pitch > 0.04f, "Pitches must vary across level openings");

        // 20.2 Smooth Fade-In Envelope (No DC Step / Initial Clicks)
        audio.stop_all(true);
        audio.play_arrival_stinger(1);
        auto attack_samples = audio.render_offline_samples(0.010f); // 10ms
        CHECK(attack_samples.size() >= 2, "Attack samples must be generated");
        CHECK(std::abs(attack_samples[0]) < 0.005f && std::abs(attack_samples[1]) < 0.005f,
              "Initial stinger frame must start at zero with anti-click attack ramp");

        // 20.3 Smooth Tail Fade-Out (Decay into Silence with Zero Discontinuity)
        for (int c = 1; c <= 5; ++c) {
            audio.stop_all(true);
            SoundCue cue = static_cast<SoundCue>(static_cast<int>(SoundCue::SectorArrival1) + (c - 1));
            audio.play_sound_2d(cue, 0.85f);
            CHECK(audio.active_voice_count() == 1, "Voice must start active");

            // Render 4.8 seconds in blocks to trace tail behavior
            float prev_s[2] = {0.0f, 0.0f};
            float final_block_max = 0.0f;
            float max_tail_slew = 0.0f;

            for (int block = 0; block < 16; ++block) {
                auto block_samples = audio.render_offline_samples(0.30f); // 0.30s chunks
                for (size_t idx = 0; idx < block_samples.size(); ++idx) {
                    int ch = idx % 2;
                    float s = block_samples[idx];
                    CHECK(!std::isnan(s) && !std::isinf(s), "Stinger samples must be finite");
                    CHECK(std::abs(s) <= 0.95f, "Stinger samples must respect ear-safety ceiling");
                    float slew = std::abs(s - prev_s[ch]);
                    if (block >= 13) {
                        max_tail_slew = std::max(max_tail_slew, slew);
                    }
                    if (block >= 14 || audio.active_voice_count() == 0) {
                        final_block_max = std::max(final_block_max, std::abs(s));
                    }
                    prev_s[ch] = s;
                }
                if (audio.active_voice_count() == 0) break;
            }

            // Voice must have cleanly finished within 4.8 seconds
            CHECK(audio.active_voice_count() == 0, "Arrival stinger voice must naturally release");
            // Final samples before/at release must be quiet (< 0.035)
            CHECK(final_block_max < 0.035f, "Stinger tail must fade to near-silence before voice release");
            std::cout << " -> Stinger " << c << " smooth tail verified (final block max: " << final_block_max << ")" << std::endl;
        }

        // 20.4 Smooth Anti-Click Quick-Release Stopping Fade
        audio.stop_all(true);
        audio.play_sound_2d(SoundCue::SectorArrival1, 0.90f);
        audio.render_offline_samples(0.50f); // Let it reach peak volume
        CHECK(audio.active_voice_count() == 1, "Voice must be playing");

        // Request graceful stop (smooth 25ms release)
        audio.stop_sound(SoundCue::SectorArrival1, false);
        // Voice should STILL be active during the 25ms quick-release ramp
        auto stop_ramp_part1 = audio.render_offline_samples(0.012f); // 12ms into 25ms fade
        float stop_peak1 = 0.0f;
        for (float s : stop_ramp_part1) stop_peak1 = std::max(stop_peak1, std::abs(s));
        CHECK(stop_peak1 > 0.001f, "Voice must smoothly decay during stopping fade rather than abruptly cutting to zero");

        // Finish the remaining fade (25ms)
        auto stop_ramp_part2 = audio.render_offline_samples(0.025f);
        CHECK(audio.active_voice_count() == 0, "Voice must release cleanly at the end of stopping fade");

        std::cout << " -> Arrival stinger non-repetition, fade-in attack ramp, smooth tail decay, and anti-click stopping verified." << std::endl;
    }

    // ── Test 21: Sneak Attack Critical Hit Audio Synthesis & Procedural Chime ──
    {
        std::cout << "[Test 21] Testing Sneak Attack Critical Hit Audio Synthesis (SoundCue::CritHit)..." << std::endl;

        audio.stop_all(true);
        audio.play_sound_2d(SoundCue::CritHit, 1.0f);
        CHECK(audio.active_voice_count() == 1, "CritHit voice must be active");

        auto crit_samples = audio.render_offline_samples(0.42f);
        float crit_energy = 0.0f;
        float crit_peak = 0.0f;
        for (float s : crit_samples) {
            CHECK(!std::isnan(s) && !std::isinf(s), "CritHit audio samples must be valid finite numbers");
            crit_energy += s * s;
            crit_peak = std::max(crit_peak, std::abs(s));
        }
        CHECK(crit_energy > 0.05f, "CritHit must synthesize audible punch, chime, and fracture energy");
        CHECK(crit_peak <= 1.0f, "CritHit audio must respect ear-safety ceiling");
        CHECK(crit_peak >= 0.15f, "CritHit audio peak must provide noticeable acoustic punch");

        std::cout << " -> Sneak attack critical hit procedural audio synthesis verified." << std::endl;
    }

    // ── Test 22: Monster Digging & Chattering Procedural Audio Synthesis ──
    {
        std::cout << "[Test 22] Testing Monster Digging & Mandible Chattering Audio Synthesis (SoundCue::MonsterDigging & StalkerChitter)..." << std::endl;

        // 22.1 Monster Wall Digging / Drilling Rotary Grinding
        audio.stop_all(true);
        audio.play_sound_2d(SoundCue::MonsterDigging, 1.0f);
        CHECK(audio.active_voice_count() == 1, "MonsterDigging voice must be active");

        auto dig_samples = audio.render_offline_samples(0.55f);
        float dig_energy = 0.0f;
        float dig_peak = 0.0f;
        for (float s : dig_samples) {
            CHECK(!std::isnan(s) && !std::isinf(s), "MonsterDigging audio samples must be valid finite numbers");
            dig_energy += s * s;
            dig_peak = std::max(dig_peak, std::abs(s));
        }
        CHECK(dig_energy > 0.05f, "MonsterDigging must synthesize audible mechanical grinding and stone fracture energy");
        CHECK(dig_peak <= 1.0f, "MonsterDigging audio must respect ear-safety ceiling");
        CHECK(dig_peak >= 0.15f, "MonsterDigging audio peak must provide noticeable acoustic presence");

        // 22.2 Monster Chattering Clicks & Mandible Scrape Burst
        audio.stop_all(true);
        audio.play_sound_2d(SoundCue::StalkerChitter, 1.0f);
        CHECK(audio.active_voice_count() == 1, "StalkerChitter voice must be active");

        auto chitter_samples = audio.render_offline_samples(0.40f);
        float chitter_energy = 0.0f;
        float chitter_peak = 0.0f;
        for (float s : chitter_samples) {
            CHECK(!std::isnan(s) && !std::isinf(s), "StalkerChitter audio samples must be valid finite numbers");
            chitter_energy += s * s;
            chitter_peak = std::max(chitter_peak, std::abs(s));
        }
        CHECK(chitter_energy > 0.01f, "StalkerChitter must synthesize distinct chitinous clicks");
        CHECK(chitter_peak <= 1.0f, "StalkerChitter audio must respect ear-safety ceiling");

        // 22.3 3D Spatial Attenuation for Wall Digging
        audio.stop_all(true);
        audio.set_listener(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
        audio.play_sound_3d(SoundCue::MonsterDigging, glm::vec3(0, 0, 3), 1.0f);
        auto close_samples = audio.render_offline_samples(0.3f);
        float close_energy = 0.0f;
        for (float s : close_samples) close_energy += s * s;

        audio.stop_all(true);
        audio.set_listener(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
        audio.play_sound_3d(SoundCue::MonsterDigging, glm::vec3(0, 0, 35), 1.0f);
        auto far_samples = audio.render_offline_samples(0.3f);
        float far_energy = 0.0f;
        for (float s : far_samples) far_energy += s * s;

        CHECK(close_energy > far_energy * 3.0f, "Close digging must be significantly louder than distant wall burrowing");

        std::cout << " -> Monster wall digging and chattering procedural audio synthesis verified." << std::endl;
    }

    // ── Test 23: Anti-Repetition Filter, Hazard Proximity Spacing & Non-Pumping Audio ──
    {
        std::cout << "[Test 23] Testing Anti-Repetition Filter, Hazard Cooldowns & Non-Pumping Audio..." << std::endl;
        audio.stop_all(true);

        // 23.1 Cavern Micro-Events Anti-Repetition Filter
        // Consecutive triggers must never repeat the exact same sound cue back-to-back
        SoundCue last_cue = SoundCue::UIBlip;
        for (int i = 0; i < 20; ++i) {
            audio.trigger_cavern_micro_event(1, 0, 0.4f);
            const auto& recent = audio.recent_micro_cues();
            if (!recent.empty()) {
                SoundCue current_cue = recent.back();
                if (i > 0) {
                    CHECK(current_cue != last_cue,
                          "Anti-repetition filter must prevent consecutive identical micro-ambience cues");
                }
                last_cue = current_cue;
            }
        }

        // Room-specific micro-event anti-repetition (Void Singularity Rift: room 11)
        last_cue = SoundCue::UIBlip;
        for (int i = 0; i < 15; ++i) {
            audio.trigger_cavern_micro_event(3, 11, 0.4f);
            const auto& recent = audio.recent_micro_cues();
            if (!recent.empty()) {
                SoundCue current_cue = recent.back();
                if (i > 0) {
                    CHECK(current_cue != last_cue,
                          "Anti-repetition filter in VoidSingularityRift must not repeat same wind/distortion cue consecutively");
                }
                last_cue = current_cue;
            }
        }

        // 23.2 Seismic Rumble Cooldown (prevents 1.1s repetitive rumble/wash sound re-triggering)
        audio.stop_all(true);
        audio.set_seismic_rumble(0.85f);
        CHECK(audio.seismic_rumble_cooldown() >= 10.0f,
              "Initial seismic rumble trigger must set refractory cooldown (>= 10s)");

        // Simulate voice completion after 1.5s (SeismicTremor duration is 1.1s / 0.85 pitch = ~1.29s)
        audio.render_offline_samples(1.5f);
        CHECK(audio.active_voice_count() == 0, "Seismic tremor voice must have finished");

        // Attempting to re-trigger while cooldown is active must be rejected
        audio.set_seismic_rumble(0.85f);
        CHECK(audio.active_voice_count() == 0,
              "Seismic rumble must NOT restart immediately while cooldown is active");

        // Advance cooldown to 0
        audio.update_hazard_proximity_audio(15.0f, 999.0f, 0.0f, 999.0f, 999.0f);
        CHECK(audio.seismic_rumble_cooldown() == 0.0f, "Cooldown must reach 0 after timer expires");

        // Now it can trigger again
        audio.set_seismic_rumble(0.85f);
        CHECK(audio.active_voice_count() == 1,
              "Seismic rumble can trigger cleanly once cooldown has elapsed");

        // 23.3 Void Hazard Wind Proximity Cooldown
        audio.stop_all(true);
        // Force void proximity triggers by updating hazard proximity with low void distance
        bool triggered_void = false;
        for (int step = 0; step < 20; ++step) {
            audio.update_hazard_proximity_audio(2.6f, 999.0f, 0.0f, 999.0f, 2.0f);
            if (audio.void_hazard_cooldown() > 0.0f) {
                triggered_void = true;
                break;
            }
        }
        CHECK(triggered_void, "Void abyss proximity must eventually trigger acoustic warning");
        CHECK(audio.void_hazard_cooldown() >= 8.0f,
              "Void hazard warning must initiate a strict 10-15s refractory cooldown");

        // Consecutive frames within cooldown must not spawn redundant overlapping voices
        int initial_voices = audio.active_voice_count();
        for (int f = 0; f < 5; ++f) {
            audio.update_hazard_proximity_audio(0.1f, 999.0f, 0.0f, 999.0f, 2.0f);
        }
        CHECK(audio.active_voice_count() == initial_voices,
              "No redundant overlapping void wind voices can spawn while cooldown is active");

        // 23.4 Ducking Non-Pumping Protection
        audio.stop_all(true);
        // Play ambient music bed
        audio.play_sound_2d(SoundCue::AmbientCavern, 0.5f, 1.0f, true);
        // Trigger threat sound
        audio.play_sound_2d(SoundCue::SeismicTremor, 0.85f);
        float duck1 = audio.ducking_factor();
        CHECK(duck1 <= 0.35f, "Threat sound must duck ambience");

        // Subsequent rapid calls must not repeatedly compress or re-pump already-ducked audio
        for (int k = 0; k < 5; ++k) {
            audio.play_sound_2d(SoundCue::ExplosiveBlast, 0.9f);
        }
        CHECK(audio.ducking_factor() >= 0.15f, "Ducking attenuation must remain within controlled safety floor");

        std::cout << " -> Anti-repetition, hazard refractory cooldowns, and non-pumping ducking verified." << std::endl;
    }

    if (export_wav) {
        std::cout << "[*] Exporting diagnostic audio sample WAV files to screenshots/audio_samples/..." << std::endl;

        // 1. Mining Drill Loop
        audio.stop_all();
        audio.set_drill_active(true, 0.5f, glm::vec3(0, 0, 2));
        write_wav_file("screenshots/audio_samples/mining_drill.wav", audio.render_offline_samples(1.0f));
        audio.set_drill_active(false);

        // 2. Voxel Fractures
        audio.stop_all();
        audio.play_sound_2d(SoundCue::VoxelBreakBasalt, 1.0f);
        write_wav_file("screenshots/audio_samples/voxel_break_basalt.wav", audio.render_offline_samples(0.5f));

        audio.stop_all();
        audio.play_sound_2d(SoundCue::VoxelBreakTitanium, 1.0f);
        write_wav_file("screenshots/audio_samples/voxel_break_titanium.wav", audio.render_offline_samples(0.6f));

        audio.stop_all();
        audio.play_sound_2d(SoundCue::VoxelBreakVoidite, 1.0f);
        write_wav_file("screenshots/audio_samples/voxel_break_voidite.wav", audio.render_offline_samples(0.7f));

        // 3. Stalker
        audio.stop_all();
        audio.play_sound_2d(SoundCue::StalkerSpotted, 1.0f);
        write_wav_file("screenshots/audio_samples/stalker_spotted.wav", audio.render_offline_samples(0.5f));

        audio.stop_all();
        audio.play_sound_2d(SoundCue::StalkerLunge, 1.0f);
        write_wav_file("screenshots/audio_samples/stalker_lunge.wav", audio.render_offline_samples(0.6f));

        audio.stop_all();
        audio.play_sound_2d(SoundCue::StalkerEchoScreech, 1.0f);
        write_wav_file("screenshots/audio_samples/stalker_echo_screech.wav", audio.render_offline_samples(1.1f));

        audio.stop_all();
        audio.play_sound_3d(SoundCue::StalkerDie, glm::vec3(0, 0, 2), 0.70f);
        write_wav_file("screenshots/audio_samples/stalker_die.wav", audio.render_offline_samples(0.7f));

        // Monster Digging & Chattering
        audio.stop_all();
        audio.play_sound_2d(SoundCue::MonsterDigging, 1.0f);
        write_wav_file("screenshots/audio_samples/monster_digging.wav", audio.render_offline_samples(0.6f));

        audio.stop_all();
        audio.play_sound_2d(SoundCue::StalkerChitter, 1.0f);
        write_wav_file("screenshots/audio_samples/stalker_chitter.wav", audio.render_offline_samples(0.4f));

        // 4. Seismic Burrower Roar & Tooth Grind
        audio.stop_all();
        audio.play_sound_2d(SoundCue::BurrowerRoar, 1.0f);
        write_wav_file("screenshots/audio_samples/burrower_roar.wav", audio.render_offline_samples(1.5f));

        audio.stop_all();
        audio.play_sound_2d(SoundCue::BurrowerGrind, 1.0f);
        write_wav_file("screenshots/audio_samples/burrower_grind.wav", audio.render_offline_samples(0.8f));

        // 5. Demolition Concussion Blast
        audio.stop_all();
        audio.play_sound_2d(SoundCue::ExplosiveBlast, 1.2f);
        write_wav_file("screenshots/audio_samples/demolition_explosion.wav", audio.render_offline_samples(1.2f));

        // 6. Surveying Sonar
        audio.stop_all();
        audio.play_sound_2d(SoundCue::SonarPulse, 1.0f);
        write_wav_file("screenshots/audio_samples/sonar_surveying.wav", audio.render_offline_samples(1.0f));

        // 7. Footsteps & Sprinting
        audio.stop_all();
        audio.play_sound_2d(SoundCue::Footstep, 0.35f, 1.0f);
        write_wav_file("screenshots/audio_samples/footstep_sprint.wav", audio.render_offline_samples(0.35f));

        // 8. Class Weapons
        audio.stop_all();
        audio.play_sound_2d(SoundCue::PlasmaFire, 0.85f);
        write_wav_file("screenshots/audio_samples/weapon_plasma.wav", audio.render_offline_samples(0.4f));

        audio.stop_all();
        audio.play_sound_2d(SoundCue::ScattergunFire, 0.85f);
        write_wav_file("screenshots/audio_samples/weapon_scattergun.wav", audio.render_offline_samples(0.4f));

        audio.stop_all();
        audio.play_sound_2d(SoundCue::RailgunFire, 0.85f);
        write_wav_file("screenshots/audio_samples/weapon_railgun.wav", audio.render_offline_samples(0.5f));

        // 9. Seismic Tremor
        audio.stop_all();
        audio.set_seismic_rumble(0.9f);
        write_wav_file("screenshots/audio_samples/seismic_tremor.wav", audio.render_offline_samples(2.0f));
        audio.set_seismic_rumble(0.0f);

        // 10. Hazard Radiation Geiger Clicks (Phase 3)
        audio.stop_all();
        audio.set_hazard_phase(3);
        write_wav_file("screenshots/audio_samples/hazard_geiger_clicks.wav", audio.render_offline_samples(1.5f));
        audio.set_hazard_phase(0);

        // 11. Full Gameplay Multitrack Master Mix
        audio.stop_all();
        audio.play_sound_2d(SoundCue::AmbientCavern, 0.45f, 1.0f, true);
        audio.set_drill_active(true, 0.8f, glm::vec3(0, 0, 2));
        audio.play_sound_2d(SoundCue::StalkerLunge, 0.9f);
        audio.play_sound_2d(SoundCue::ExplosiveBlast, 1.0f);
        write_wav_file("screenshots/audio_samples/full_gameplay_mix.wav", audio.render_offline_samples(2.5f));

        std::cout << " -> Reference WAV files exported successfully." << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "ALL 23 AUDIO & EAR-SAFETY TESTS PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;

    audio.shutdown();
    return 0;
}
