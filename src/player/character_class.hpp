#pragma once
#include <string>
#include <glm/glm.hpp>

namespace Voidfall {

enum class CharacterClass : int {
    Demolitionist = 0,
    Vanguard = 1,
    Scout = 2,
    DEMOLITIONIST = 0,
    VANGUARD = 1,
    SCOUT = 2
};

struct CharacterAttributes {
    CharacterClass classType{CharacterClass::Demolitionist};
    std::string name;
    std::string role;
    std::string abilityDescription;
    std::string traitName;
    std::string traitDescription;

    float baseMineSpeed{1.0f};          // Multiplier for excavation rate
    float moveSpeed{1.0f};              // Multiplier for walking/sprinting
    float suitIntegrity{100.0f};        // Base maximum health points
    float grapplePullSpeed{1.0f};       // Reel velocity factor
    float sonarRadius{14.0f};           // Scan query sphere radius (voxels)
    int maxBulkheads{10};               // Base inventory limit for placed supports
    float fallingDamageReduction{0.0f}; // Percentage damage mitigation from falling rocks (0.0 to 1.0)
    float scanLingerBonus{0.0f};        // Extra linger duration for surveyed outlines (seconds)

    // Visual theme colors for viewmodel hands & suit
    glm::vec4 primaryAccentColor{0.95f, 0.55f, 0.05f, 1.0f};
    glm::vec4 suitSleeveColor{0.18f, 0.20f, 0.24f, 1.0f};
    glm::vec4 gloveColor{0.24f, 0.26f, 0.28f, 1.0f};
};

CharacterAttributes get_character_attributes(CharacterClass cls);
const char* get_character_class_name(CharacterClass cls);

} // namespace Voidfall
