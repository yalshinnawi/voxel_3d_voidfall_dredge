#pragma once
#include <string>
#include <vector>
#include <algorithm>

namespace Voidfall {

struct SkillBranch {
    std::string name;
    std::string description;
    std::string unlock_perk_name;
    std::string unlock_description;
    int xp{0};
    int xp_for_unlock{100};
    bool unlocked{false};

    bool add_xp(int amount) {
        xp += amount;
        if (!unlocked && xp >= xp_for_unlock) {
            unlocked = true;
            return true; // Newly unlocked
        }
        return false;
    }

    float progress() const {
        return std::clamp(static_cast<float>(xp) / static_cast<float>(xp_for_unlock), 0.0f, 1.0f);
    }
};

class SkillMatrix {
public:
    SkillMatrix() {
        demolitions = {
            "Demolitions",
            "Drilling and blasting subterranean rock",
            "Micro-Charges",
            "Surgical 1x1x3 directional blast without ore destruction",
            0,
            100,
            false
        };

        surveying = {
            "Surveying",
            "Seismic sonar cavern mapping",
            "Extended Frequency",
            "Expands Sonar pulse radius from 18 to 30 voxels",
            0,
            80,
            false
        };

        suit = {
            "Exo-Suit Calibration",
            "Hazards, traversal, and subterranean survival",
            "Kinetic Dynamo",
            "Sprinting and falling regenerates thruster fuel 25% faster",
            0,
            120,
            false
        };

        acrobatics = {
            "Acrobatics",
            "Tension cable grapple traversal and swinging",
            "Anchor Surge",
            "Increases grapple cable reel speed by 50%",
            0,
            90,
            false
        };
    }

    SkillBranch demolitions;
    SkillBranch surveying;
    SkillBranch suit;
    SkillBranch acrobatics;

    std::vector<std::string> pending_unlock_notifications;

    void add_demolitions_xp(int amount) {
        if (demolitions.add_xp(amount)) {
            pending_unlock_notifications.push_back("DEMOLITIONS UNLOCKED: Micro-Charges!");
        }
    }

    void add_surveying_xp(int amount) {
        if (surveying.add_xp(amount)) {
            pending_unlock_notifications.push_back("SURVEYING UNLOCKED: Extended Frequency (30-Voxel Sonar)!");
        }
    }

    void add_suit_xp(int amount) {
        if (suit.add_xp(amount)) {
            pending_unlock_notifications.push_back("EXO-SUIT UNLOCKED: Kinetic Dynamo!");
        }
    }

    void add_acrobatics_xp(int amount) {
        if (acrobatics.add_xp(amount)) {
            pending_unlock_notifications.push_back("ACROBATICS UNLOCKED: Anchor Surge (+50% Reel Speed)!");
        }
    }

    float get_sonar_radius() const {
        return surveying.unlocked ? 30.0f : 18.0f;
    }

    float get_grapple_reel_multiplier() const {
        return acrobatics.unlocked ? 1.5f : 1.0f;
    }

    float get_thruster_regen_multiplier() const {
        return suit.unlocked ? 1.25f : 1.0f;
    }

    bool has_micro_charges() const {
        return demolitions.unlocked;
    }
};

} // namespace Voidfall
