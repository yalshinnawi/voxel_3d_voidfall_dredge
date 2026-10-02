#pragma once
#include "../player/character_class.hpp"
#include "../player/upgrades.hpp"
#include "../player/loadout.hpp"
#include <string>
#include <iostream>
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
    int save_version{1};
    std::string last_saved_time{""};
    std::string player_name{"DELVER-01"};
    int total_exp{300};
    int total_voidite{12};
    int total_titanium{8};
    int selected_class_id{0}; // 0 = Demolitionist, 1 = Vanguard, 2 = Scout
    CharacterClass selectedClass{CharacterClass::Demolitionist};
    UpgradeTree upgrades;
    SectorRecord sector_records[4] = {
        {0, "UNEXPLORED"},
        {0, "UNEXPLORED"},
        {0, "UNEXPLORED"},
        {0, "UNEXPLORED"}
    };
};

class SaveSystem {
public:
    inline static const std::string DEFAULT_SAVE_DIR = "saves";
    inline static const std::string DEFAULT_SAVE_FILE = "saves/save_data.json";

    static bool save_profile(const UserProfile& profile, const std::string& filepath = DEFAULT_SAVE_FILE);
    static bool load_profile(UserProfile& profile, const std::string& filepath = DEFAULT_SAVE_FILE);
};

} // namespace Voidfall
