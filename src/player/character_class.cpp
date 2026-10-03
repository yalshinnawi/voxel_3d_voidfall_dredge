#include "character_class.hpp"

namespace Voidfall {

CharacterAttributes get_character_attributes(CharacterClass cls) {
    CharacterAttributes attr;
    attr.classType = cls;

    switch (cls) {
        case CharacterClass::Demolitionist:
            attr.name = "Kaelen";
            attr.role = "DEMOLITIONIST // HEAVY BREACHER";
            attr.abilityDescription = "Heavy demolition operative with reinforced pneumatic drilling and concentrated micro-explosives.";
            attr.traitName = "Surgical Blast & Volatile Refining";
            attr.traitDescription = "Shaped charges breach +1 perimeter radius. Volatile ore mining yields +25% bonus EXP.";
            attr.baseMineSpeed = 1.40f;
            attr.moveSpeed = 1.00f;
            attr.suitIntegrity = 100.0f;
            attr.grapplePullSpeed = 1.00f;
            attr.sonarRadius = 14.0f;
            attr.sonarCooldown = 10.0f;
            attr.maxBulkheads = 8;
            attr.fallingDamageReduction = 0.00f;
            attr.scanLingerBonus = 0.0f;
            attr.primaryAccentColor = glm::vec4(1.0f, 0.584f, 0.0f, 1.0f); // #FF9500 Hazard Orange
            attr.suitSleeveColor = glm::vec4(0.20f, 0.17f, 0.15f, 1.0f);
            attr.gloveColor = glm::vec4(0.32f, 0.26f, 0.22f, 1.0f);
            break;

        case CharacterClass::Vanguard:
            attr.name = "Rhodes";
            attr.role = "VANGUARD // ARMORED STABILIZER";
            attr.abilityDescription = "Heavy fortified delver built to withstand subterranean shockwaves and cave-in impacts.";
            attr.traitName = "Tectonic Bulkhead Plating";
            attr.traitDescription = "50% damage reduction against falling rocks. Placed overhead bulkheads deflect debris safely.";
            attr.baseMineSpeed = 1.00f;
            attr.moveSpeed = 0.88f;
            attr.suitIntegrity = 135.0f;
            attr.grapplePullSpeed = 1.00f;
            attr.sonarRadius = 14.0f;
            attr.sonarCooldown = 10.0f;
            attr.maxBulkheads = 12;
            attr.fallingDamageReduction = 0.50f; // 50% damage mitigation from falling ceiling rocks
            attr.scanLingerBonus = 0.0f;
            attr.primaryAccentColor = glm::vec4(0.29f, 0.333f, 0.408f, 1.0f); // #4A5568 Matte Slate
            attr.suitSleeveColor = glm::vec4(0.18f, 0.20f, 0.24f, 1.0f);
            attr.gloveColor = glm::vec4(0.25f, 0.28f, 0.32f, 1.0f);
            break;

        case CharacterClass::Scout:
            attr.name = "Vesper";
            attr.role = "SCOUT // ACOUSTIC PATHFINDER";
            attr.abilityDescription = "Agile recon specialist equipped with wide-spectrum seismic sonar and high-speed winch.";
            attr.traitName = "Acoustic Resonance & High-Tension Winch";
            attr.traitDescription = "Sonar radius expanded to 22 voxels (+1.0s outline linger, -2s cooldown). Grapple reels 50% faster.";
            attr.baseMineSpeed = 1.00f;
            attr.moveSpeed = 1.20f;
            attr.suitIntegrity = 80.0f;
            attr.grapplePullSpeed = 1.50f;
            attr.sonarRadius = 22.0f;
            attr.sonarCooldown = 8.0f;
            attr.maxBulkheads = 6;
            attr.fallingDamageReduction = 0.00f;
            attr.scanLingerBonus = 1.0f;
            attr.primaryAccentColor = glm::vec4(0.0f, 0.824f, 0.827f, 1.0f); // #00D2D3 Carbon Cyan
            attr.suitSleeveColor = glm::vec4(0.12f, 0.14f, 0.18f, 1.0f); // Sleek Carbon
            attr.gloveColor = glm::vec4(0.16f, 0.22f, 0.26f, 1.0f);
            break;
    }

    return attr;
}

const char* get_character_class_name(CharacterClass cls) {
    switch (cls) {
        case CharacterClass::Demolitionist: return "Demolitionist";
        case CharacterClass::Vanguard:      return "Vanguard";
        case CharacterClass::Scout:         return "Scout";
        default:                            return "Delver";
    }
}

} // namespace Voidfall
