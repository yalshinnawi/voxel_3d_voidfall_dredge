#pragma once
#include "../player/character_class.hpp"
#include "../player/upgrades.hpp"
#include "../player/loadout.hpp"
#include "settings.hpp"
#include <string>
#include <iostream>
#include <algorithm>
#include <glm/glm.hpp>

namespace Voidfall {

inline std::ostream& operator<<(std::ostream& os, const CharacterClass& cls) {
    os << static_cast<int>(cls);
    return os;
}

inline std::istream& operator>>(std::istream& is, CharacterClass& cls) {
    int val = 0;
    if (is >> val) {
        cls = static_cast<CharacterClass>(val);
    }
    return is;
}

inline std::ostream& operator<<(std::ostream& os, const glm::vec3& v) {
    os << v.x << " " << v.y << " " << v.z;
    return os;
}

inline std::istream& operator>>(std::istream& is, glm::vec3& v) {
    is >> v.x >> v.y >> v.z;
    return is;
}

struct UserProfile {
    int save_version{2};
    std::string last_saved_time{""};
    std::string player_name{"DELVER-01"};
    int total_exp{300};        // Cumulative EXP for Player Level
    int total_coins{200};      // Spendable currency for Upgrades & Equipment
    int total_voidite{12};
    int total_titanium{8};
    int selected_class_id{0};  // 0 = Demolitionist, 1 = Vanguard, 2 = Scout
    CharacterClass selectedClass{CharacterClass::Demolitionist};
    UpgradeTree upgrades;
    GameSettings settings;
    static constexpr int MAX_SECTOR_RECORDS = PlayerInventory::MAX_SECTOR_RECORDS;
    SectorRecord sector_records[MAX_SECTOR_RECORDS] = {};
    int highest_cleared_sector{1};

    // EXP & Level Progression Thresholds
    // Level 1: 0 - 299 EXP
    // Level 2: 300 - 699 EXP (requires 300 EXP)
    // Level 3: 700 - 1299 EXP (requires 700 EXP)
    // Level 4: 1300 - 2199 EXP (requires 1300 EXP)
    // Level 5: 2200 - 3499 EXP (requires 2200 EXP)
    // Level 6+: 3500 + (level - 6) * 1500
    static int get_exp_for_level(int level) {
        if (level <= 1) return 0;
        if (level == 2) return 300;
        if (level == 3) return 700;
        if (level == 4) return 1300;
        if (level == 5) return 2200;
        return 2200 + (level - 5) * 1500;
    }

    int get_player_level() const {
        int lvl = 1;
        while (lvl < 50 && total_exp >= get_exp_for_level(lvl + 1)) {
            lvl++;
        }
        return lvl;
    }

    int get_current_level_exp_floor() const {
        return get_exp_for_level(get_player_level());
    }

    int get_next_level_exp_req() const {
        return get_exp_for_level(get_player_level() + 1);
    }

    float get_level_progress() const {
        int cur_lvl = get_player_level();
        int cur_floor = get_exp_for_level(cur_lvl);
        int next_req = get_exp_for_level(cur_lvl + 1);
        int diff = next_req - cur_floor;
        if (diff <= 0) return 1.0f;
        return std::clamp(static_cast<float>(total_exp - cur_floor) / static_cast<float>(diff), 0.0f, 1.0f);
    }

    // Award EXP, return how many levels were gained and bonus coins (+150 coins/lvl)
    bool add_exp(int amount, int& out_levels_gained, int& out_bonus_coins) {
        int old_lvl = get_player_level();
        total_exp = std::max(0, total_exp + amount);
        int new_lvl = get_player_level();
        out_levels_gained = std::max(0, new_lvl - old_lvl);
        out_bonus_coins = out_levels_gained * 150;
        if (out_bonus_coins > 0) {
            total_coins += out_bonus_coins;
        }
        return out_levels_gained > 0;
    }

    void grant_exp(int amount) {
        int lvls = 0, bonus = 0;
        add_exp(amount, lvls, bonus);
    }

    void grant_coins(int amount) {
        total_coins = std::max(0, total_coins + amount);
    }

    void grant_resources(int exp, int voidite, int titanium) {
        grant_exp(exp);
        total_voidite = std::max(0, total_voidite + voidite);
        total_titanium = std::max(0, total_titanium + titanium);
    }

    void grant_resources(int exp, int coins, int voidite, int titanium) {
        grant_exp(exp);
        grant_coins(coins);
        total_voidite = std::max(0, total_voidite + voidite);
        total_titanium = std::max(0, total_titanium + titanium);
    }

    // Level Gating Requirements
    static int get_required_level_for_class(CharacterClass cls) {
        switch (cls) {
            case CharacterClass::Demolitionist: return 1;
            case CharacterClass::Vanguard:      return 2;
            case CharacterClass::Scout:         return 3;
            default: return 1;
        }
    }

    bool is_class_unlocked(CharacterClass cls) const {
        return get_player_level() >= get_required_level_for_class(cls);
    }

    // Sectors 1-3 have fixed level requirements; sectors 4+ scale by +2 levels every 2 sectors,
    // capped at level 30 for deep-end endless play.
    static int get_required_level_for_sector(int sector) {
        if (sector <= 1) return 1; // Sector 1: Perimeter Drift (Lv 1)
        if (sector == 2) return 2; // Sector 2: Volatile Fault (Lv 2)
        if (sector == 3) return 4; // Sector 3: Void Cradle (Lv 4)
        // Sectors 4+: 6, 8, 10, ... capped at 30
        int required = 4 + (sector - 3) * 2;
        return std::min(required, 30);
    }

    // A sector is unlocked when the player has the required level.
    // Sectors 1-3: level-only gating (classic sectors, no prior-clear requirement).
    // Sectors 4+: also requires the previous sector to have a completion record OR
    //             highest_cleared_sector to cover the prerequisite (endless progression).
    bool is_sector_unlocked(int sector) const {
        if (sector < 1 || sector >= MAX_SECTOR_RECORDS) return false;
        if (get_player_level() < get_required_level_for_sector(sector)) return false;
        if (sector <= 3) return true; // Classic sectors: level gate only
        // Endless sectors 4+: require prior sector cleared
        int prev = sector - 1;
        if (prev < MAX_SECTOR_RECORDS && sector_records[prev].highest_completion_rate > 0) return true;
        return highest_cleared_sector >= prev;
    }
};

class SaveSystem {
public:
    inline static const std::string DEFAULT_SAVE_DIR = "saves";
    inline static const std::string DEFAULT_SAVE_FILE = "saves/save_data.json";
    inline static const std::string TEST_SAVE_FILE = "saves/test_save_data.json";

    static bool save_profile(const UserProfile& profile, const std::string& filepath = DEFAULT_SAVE_FILE);
    static bool load_profile(UserProfile& profile, const std::string& filepath = DEFAULT_SAVE_FILE);
};

} // namespace Voidfall
