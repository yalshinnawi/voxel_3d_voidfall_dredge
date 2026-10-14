#include "save_system.hpp"
#include "logger.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <random>

namespace Voidfall {

uint32_t SaveSystem::generate_random_seed(int sector) {
    std::random_device rd;
    auto now_epoch = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    uint32_t t_low = static_cast<uint32_t>(now_epoch);
    uint32_t t_high = static_cast<uint32_t>(now_epoch >> 32);
    uint32_t dev = static_cast<uint32_t>(rd());
    uint32_t sector_mix = static_cast<uint32_t>(sector) * 2654435761u;
    uint32_t seed = dev ^ t_low ^ (t_high * 1013904223u) ^ sector_mix;
    if (seed == 0) {
        seed = 1337u;
    }
    return seed;
}

bool SaveSystem::save_profile(const UserProfile& profile, const std::string& filepath) {
    std::filesystem::path p(filepath);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    std::ofstream out(filepath, std::ios::trunc);
    if (!out.is_open()) {
        VF_LOG_ERROR("SaveSystem", "Failed to open save file for writing: " << filepath);
        return false;
    }

    // Generate formatted ISO/standard timestamp for last save
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &in_time_t);
#else
    localtime_r(&in_time_t, &tm_buf);
#endif
    std::stringstream time_ss;
    time_ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    const_cast<UserProfile&>(profile).last_saved_time = time_ss.str();

    out << "{\n";
    out << "  \"save_version\": " << profile.save_version << ",\n";
    out << "  \"last_saved_time\": \"" << profile.last_saved_time << "\",\n";
    out << "  \"player_name\": \"" << profile.player_name << "\",\n";
    out << "  \"total_exp\": " << profile.total_exp << ",\n";
    out << "  \"lifetime_exp\": " << std::max(profile.lifetime_exp, profile.total_exp) << ",\n";
    out << "  \"total_coins\": " << profile.total_coins << ",\n";
    out << "  \"total_voidite\": " << profile.total_voidite << ",\n";
    out << "  \"total_titanium\": " << profile.total_titanium << ",\n";
    out << "  \"total_astral_ore\": " << profile.total_astral_ore << ",\n";
    int class_val = (profile.selected_class_id != 0) ? profile.selected_class_id : static_cast<int>(profile.selectedClass);
    out << "  \"selected_class_id\": " << class_val << ",\n";
    out << "  \"selected_class\": " << class_val << ",\n";
    out << "  \"drill_speed_tier\": " << profile.upgrades.drillSpeedTier << ",\n";
    out << "  \"drill_durability_tier\": " << profile.upgrades.drillDurabilityTier << ",\n";
    out << "  \"micro_charges_tier\": " << profile.upgrades.microChargesTier << ",\n";
    out << "  \"thruster_tank_tier\": " << profile.upgrades.thrusterTankTier << ",\n";
    out << "  \"kinetic_dynamo_tier\": " << profile.upgrades.kineticDynamoTier << ",\n";
    out << "  \"impact_dampeners_tier\": " << profile.upgrades.impactDampenersTier << ",\n";
    out << "  \"sonar_frequency_tier\": " << profile.upgrades.sonarFrequencyTier << ",\n";
    out << "  \"reinforced_plating_tier\": " << profile.upgrades.reinforcedPlatingTier << ",\n";
    out << "  \"radiation_shielding_tier\": " << profile.upgrades.radiationShieldingTier << ",\n";
    out << "  \"thermal_insulation_tier\": " << profile.upgrades.thermalInsulationTier << ",\n";
    out << "  \"kinetic_shielding_tier\": " << profile.upgrades.kineticShieldingTier << ",\n";
    out << "  \"defibrillator_tier\": " << profile.upgrades.defibrillatorTier << ",\n";
    out << "  \"acoustic_dampening_tier\": " << profile.upgrades.acousticDampeningTier << ",\n";
    out << "  \"shadow_veil_tier\": " << profile.upgrades.shadowVeilTier << ",\n";
    out << "  \"ambush_protocol_tier\": " << profile.upgrades.ambushProtocolTier << ",\n";
    out << "  \"pneumatic_impact_tier\": " << profile.upgrades.pneumaticImpactTier << ",\n";
    out << "  \"cleave_shockwave_tier\": " << profile.upgrades.cleaveShockwaveTier << ",\n";
    out << "  \"berserker_overdrive_tier\": " << profile.upgrades.berserkerOverdriveTier << ",\n";
    out << "  \"hyper_velocity_slugs_tier\": " << profile.upgrades.hyperVelocitySlugsTier << ",\n";
    out << "  \"rapid_cycling_tier\": " << profile.upgrades.rapidCyclingTier << ",\n";
    out << "  \"apex_boss_damage_tier\": " << profile.upgrades.apexBossDamageTier << ",\n";
    out << "  \"critical_weakpoint_tier\": " << profile.upgrades.criticalWeakpointTier << ",\n";
    out << "  \"highest_cleared_sector\": " << profile.highest_cleared_sector << ",\n";
    for (int s = 1; s < UserProfile::MAX_SECTOR_RECORDS; ++s) {
        if (s <= 3 || profile.sector_records[s].highest_completion_rate > 0 || profile.sector_records[s].completed || profile.sector_records[s].saved_seed != 0) {
            out << "  \"sector" << s << "_rate\": " << profile.sector_records[s].highest_completion_rate << ",\n";
            out << "  \"sector" << s << "_badge\": \"" << profile.sector_records[s].best_badge << "\",\n";
            if (profile.sector_records[s].saved_seed != 0) {
                out << "  \"sector" << s << "_seed\": " << profile.sector_records[s].saved_seed << ",\n";
            }
            if (profile.sector_records[s].completed) {
                out << "  \"sector" << s << "_completed\": true,\n";
            }
        }
    }
    out << "  \"master_volume\": " << profile.settings.master_volume << ",\n";
    out << "  \"music_volume\": " << profile.settings.music_volume << ",\n";
    out << "  \"sfx_volume\": " << profile.settings.sfx_volume << ",\n";
    out << "  \"enemy_volume\": " << profile.settings.enemy_volume << ",\n";
    out << "  \"ambient_volume\": " << profile.settings.ambient_volume << ",\n";
    out << "  \"ui_volume\": " << profile.settings.ui_volume << ",\n";
    out << "  \"mute_all\": " << (profile.settings.mute_all ? "true" : "false") << ",\n";
    out << "  \"mouse_sensitivity\": " << profile.settings.mouse_sensitivity << ",\n";
    out << "  \"fov\": " << profile.settings.fov << ",\n";
    out << "  \"brightness\": " << profile.settings.brightness << ",\n";
    out << "  \"screen_shake\": " << profile.settings.screen_shake << ",\n";
    out << "  \"auto_step\": " << (profile.settings.auto_step ? "true" : "false") << "\n";
    out << "}\n";

    out.close();
    VF_LOG_INFO("SaveSystem", "Successfully saved profile to " << filepath << " (Timestamp: " << profile.last_saved_time << ")");
    return true;
}

bool SaveSystem::load_profile(UserProfile& profile, const std::string& filepath) {
    std::string path_to_open = filepath;
    bool migrated = false;

    // Backward compatibility: If target in saves/ doesn't exist, check root legacy file
    if (!std::filesystem::exists(path_to_open) && filepath == DEFAULT_SAVE_FILE && std::filesystem::exists("save_data.json")) {
        VF_LOG_INFO("SaveSystem", "Found legacy save_data.json in root. Migrating to " << DEFAULT_SAVE_FILE);
        path_to_open = "save_data.json";
        migrated = true;
    }

    std::ifstream in(path_to_open);
    if (!in.is_open()) {
        VF_LOG_INFO("SaveSystem", "No existing save file found at " << filepath << ". Initializing new profile in saves folder.");
        save_profile(profile, filepath);
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        auto parse_int = [&](const std::string& key, int& val) {
            auto pos = line.find("\"" + key + "\":");
            if (pos != std::string::npos) {
                auto comma = line.find(',', pos);
                std::string num_str = line.substr(pos + key.length() + 3, (comma != std::string::npos ? comma : line.length()) - (pos + key.length() + 3));
                try {
                    val = std::stoi(num_str);
                } catch (...) {}
            }
        };

        auto parse_str = [&](const std::string& key, std::string& val) {
            auto pos = line.find("\"" + key + "\": \"");
            if (pos != std::string::npos) {
                auto start = pos + key.length() + 5;
                auto end = line.find('"', start);
                if (end != std::string::npos) {
                    val = line.substr(start, end - start);
                }
            }
        };
        auto parse_float = [&](const std::string& key, float& val) {
            auto pos = line.find("\"" + key + "\":");
            if (pos != std::string::npos) {
                auto comma = line.find(',', pos);
                std::string num_str = line.substr(pos + key.length() + 3, (comma != std::string::npos ? comma : line.length()) - (pos + key.length() + 3));
                try {
                    val = std::stof(num_str);
                } catch (...) {}
            }
        };

        auto parse_uint = [&](const std::string& key, uint32_t& val) {
            auto pos = line.find("\"" + key + "\":");
            if (pos != std::string::npos) {
                auto comma = line.find(',', pos);
                std::string num_str = line.substr(pos + key.length() + 3, (comma != std::string::npos ? comma : line.length()) - (pos + key.length() + 3));
                try {
                    val = static_cast<uint32_t>(std::stoul(num_str));
                } catch (...) {}
            }
        };

        auto parse_bool = [&](const std::string& key, bool& val) {
            auto pos = line.find("\"" + key + "\":");
            if (pos != std::string::npos) {
                if (line.find("true", pos) != std::string::npos) val = true;
                else if (line.find("false", pos) != std::string::npos) val = false;
            }
        };

        parse_int("save_version", profile.save_version);
        parse_str("last_saved_time", profile.last_saved_time);
        parse_str("player_name", profile.player_name);
        parse_int("total_exp", profile.total_exp);
        profile.lifetime_exp = profile.total_exp;
        parse_int("lifetime_exp", profile.lifetime_exp);
        profile.lifetime_exp = std::max(profile.lifetime_exp, profile.total_exp);
        parse_int("total_coins", profile.total_coins);
        parse_int("total_voidite", profile.total_voidite);
        parse_int("total_titanium", profile.total_titanium);
        parse_int("total_astral_ore", profile.total_astral_ore);
        parse_int("selected_class_id", profile.selected_class_id);
        profile.selectedClass = static_cast<CharacterClass>(profile.selected_class_id);

        parse_int("drill_speed_tier", profile.upgrades.drillSpeedTier);
        parse_int("drill_durability_tier", profile.upgrades.drillDurabilityTier);
        parse_int("micro_charges_tier", profile.upgrades.microChargesTier);
        parse_int("thruster_tank_tier", profile.upgrades.thrusterTankTier);
        parse_int("kinetic_dynamo_tier", profile.upgrades.kineticDynamoTier);
        parse_int("impact_dampeners_tier", profile.upgrades.impactDampenersTier);
        parse_int("sonar_frequency_tier", profile.upgrades.sonarFrequencyTier);
        parse_int("reinforced_plating_tier", profile.upgrades.reinforcedPlatingTier);
        parse_int("radiation_shielding_tier", profile.upgrades.radiationShieldingTier);
        parse_int("thermal_insulation_tier", profile.upgrades.thermalInsulationTier);
        parse_int("kinetic_shielding_tier", profile.upgrades.kineticShieldingTier);
        parse_int("defibrillator_tier", profile.upgrades.defibrillatorTier);
        parse_int("acoustic_dampening_tier", profile.upgrades.acousticDampeningTier);
        parse_int("shadow_veil_tier", profile.upgrades.shadowVeilTier);
        parse_int("ambush_protocol_tier", profile.upgrades.ambushProtocolTier);
        parse_int("pneumatic_impact_tier", profile.upgrades.pneumaticImpactTier);
        parse_int("cleave_shockwave_tier", profile.upgrades.cleaveShockwaveTier);
        parse_int("berserker_overdrive_tier", profile.upgrades.berserkerOverdriveTier);
        parse_int("hyper_velocity_slugs_tier", profile.upgrades.hyperVelocitySlugsTier);
        parse_int("rapid_cycling_tier", profile.upgrades.rapidCyclingTier);
        parse_int("apex_boss_damage_tier", profile.upgrades.apexBossDamageTier);
        parse_int("critical_weakpoint_tier", profile.upgrades.criticalWeakpointTier);

        parse_int("highest_cleared_sector", profile.highest_cleared_sector);
        for (int s = 1; s < UserProfile::MAX_SECTOR_RECORDS; ++s) {
            parse_int("sector" + std::to_string(s) + "_rate", profile.sector_records[s].highest_completion_rate);
            parse_str("sector" + std::to_string(s) + "_badge", profile.sector_records[s].best_badge);
            parse_uint("sector" + std::to_string(s) + "_seed", profile.sector_records[s].saved_seed);
            parse_bool("sector" + std::to_string(s) + "_completed", profile.sector_records[s].completed);
        }

        parse_float("master_volume", profile.settings.master_volume);
        parse_float("music_volume", profile.settings.music_volume);
        parse_float("sfx_volume", profile.settings.sfx_volume);
        parse_float("enemy_volume", profile.settings.enemy_volume);
        parse_float("ambient_volume", profile.settings.ambient_volume);
        parse_float("ui_volume", profile.settings.ui_volume);
        parse_bool("mute_all", profile.settings.mute_all);
        parse_float("mouse_sensitivity", profile.settings.mouse_sensitivity);
        parse_float("fov", profile.settings.fov);
        parse_float("brightness", profile.settings.brightness);
        parse_float("screen_shake", profile.settings.screen_shake);
        parse_bool("auto_step", profile.settings.auto_step);
    }

    profile.settings.sanitize();

    in.close();

    // Ensure highest_cleared_sector reflects loaded sector records and infer completion for legacy saves
    for (int s = 1; s < UserProfile::MAX_SECTOR_RECORDS; ++s) {
        if (!profile.sector_records[s].completed && profile.sector_records[s].is_completed()) {
            profile.sector_records[s].completed = true;
        }
        if (profile.sector_records[s].completed && profile.sector_records[s].saved_seed == 0) {
            profile.sector_records[s].saved_seed = (static_cast<uint32_t>(s) * 2654435761u) ^ 0x5EED1337u;
        }
        if (profile.sector_records[s].highest_completion_rate > 0 || profile.sector_records[s].completed) {
            profile.highest_cleared_sector = std::max(profile.highest_cleared_sector, s);
        }
    }
    profile.highest_cleared_sector = std::max(profile.highest_cleared_sector, 1);

    if (migrated) {
        save_profile(profile, filepath);
        VF_LOG_INFO("SaveSystem", "Saved migrated profile to " << filepath);
    }

    VF_LOG_INFO("SaveSystem", "Successfully loaded user profile from " << filepath
                << " (Level: " << profile.get_player_level()
                << ", EXP: " << profile.total_exp
                << ", Coins: " << profile.total_coins
                << ", Class: " << profile.selected_class_id
                << ", Saved: " << (profile.last_saved_time.empty() ? "None" : profile.last_saved_time) << ")");
    return true;
}

bool SaveSystem::delete_save_file(const std::string& filepath) {
    std::error_code ec;
    bool removed = false;
    if (std::filesystem::exists(filepath, ec)) {
        removed = std::filesystem::remove(filepath, ec);
    }
    if (filepath == DEFAULT_SAVE_FILE && std::filesystem::exists("save_data.json", ec)) {
        std::filesystem::remove("save_data.json", ec);
        removed = true;
    }
    VF_LOG_INFO("SaveSystem", "Save file deleted at " << filepath << " (status: " << (removed ? "REMOVED" : "NOT FOUND") << ")");
    return removed;
}

bool SaveSystem::clear_save(const std::string& filepath) {
    std::error_code ec;
    if (std::filesystem::exists(filepath, ec)) {
        std::filesystem::remove(filepath, ec);
    }
    if (filepath == DEFAULT_SAVE_FILE && std::filesystem::exists("save_data.json", ec)) {
        std::filesystem::remove("save_data.json", ec);
    }
    UserProfile fresh_profile{};
    bool saved = save_profile(fresh_profile, filepath);
    VF_LOG_INFO("SaveSystem", "Save profile cleared and fresh defaults written to " << filepath
                << " (success: " << (saved ? "YES" : "NO") << ")");
    return saved;
}

bool SaveSystem::reset_profile(UserProfile& profile, const std::string& filepath, bool preserve_settings) {
    GameSettings old_settings = profile.settings;
    std::string old_name = profile.player_name;
    profile = UserProfile{};
    if (preserve_settings) {
        profile.settings = old_settings;
    }
    if (!old_name.empty()) {
        profile.player_name = old_name;
    }
    return save_profile(profile, filepath);
}

} // namespace Voidfall
