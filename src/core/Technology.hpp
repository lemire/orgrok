#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>

namespace orion {

// Classic MoO 1 categories
enum class TechCategory : uint8_t {
    Computers = 0,
    Construction,
    ForceFields,
    Planetology,
    Propulsion,
    Weapons,
    Count
};

inline std::string_view to_string(TechCategory cat) {
    switch (cat) {
        case TechCategory::Computers:    return "Computers";
        case TechCategory::Construction: return "Construction";
        case TechCategory::ForceFields:  return "Force Fields";
        case TechCategory::Planetology:  return "Planetology";
        case TechCategory::Propulsion:   return "Propulsion";
        case TechCategory::Weapons:      return "Weapons";
        default: return "Unknown";
    }
}

struct TechDef {
    std::string name;
    TechCategory category;
    int researchCost;
    std::string description;
};

// Very small starting tech tree for Phase 2
inline const std::vector<TechDef> TECH_TREE = {
    // Computers
    {"Improved Computers",     TechCategory::Computers,    120, "Better research and ship targeting"},
    {"Battle Computers",       TechCategory::Computers,    280, "Significant combat bonus"},

    // Construction
    {"Improved Construction",  TechCategory::Construction, 100, "Cheaper and faster ship building"},
    {"Advanced Construction",  TechCategory::Construction, 250, "Unlocks larger hulls and better factories"},

    // Force Fields
    {"Deflector Shields",      TechCategory::ForceFields,  110, "Basic shield technology"},
    {"Energy Shields",         TechCategory::ForceFields,  260, "Stronger ship and planetary defenses"},

    // Planetology
    {"Terraforming",           TechCategory::Planetology,  140, "Improved planet habitability"},
    {"Advanced Terraforming",  TechCategory::Planetology,  320, "Can turn hostile worlds into paradises"},

    // Propulsion
    {"Nuclear Engines",        TechCategory::Propulsion,   80,  "Faster ship movement"},
    {"Fusion Engines",         TechCategory::Propulsion,   210, "Much faster interstellar travel"},

    // Weapons
    {"Laser Weapons",          TechCategory::Weapons,      90,  "Basic ship weapons"},
    {"Particle Beams",         TechCategory::Weapons,      230, "Powerful direct fire weapons"},
};

struct TechnologyState {
    std::array<int, 6> level = {0};           // how many techs completed per category
    std::vector<int> researchedTechIndices;   // indices into TECH_TREE

    // Simple research bank (added from colonies each turn)
    int researchThisTurn = 0;

    bool hasTech(int techIndex) const {
        for (int idx : researchedTechIndices) if (idx == techIndex) return true;
        return false;
    }

    // Convenience checks for game systems
    bool hasImprovedConstruction() const { return hasTech(2) || hasTech(3); }
    bool hasBetterEngines() const { return hasTech(8); }  // Nuclear Engines (index 8)
    bool hasParticleBeams() const { return hasTech(11); } // Particle Beams tech

    float getIndustryBonus() const {
        float bonus = 1.0f;
        if (hasTech(2)) bonus += 0.15f;
        if (hasTech(3)) bonus += 0.25f;
        return bonus;
    }

    [[nodiscard]] float getResearchBonus() const {
        float bonus = 1.0f;
        if (hasTech(0)) bonus += 0.18f; // Improved Computers
        if (hasTech(1)) bonus += 0.22f; // Battle Computers (better labs)
        return bonus;
    }

    // === Propulsion travel tech (Phase 2) ===
    // These affect ship speed and maximum jump range when giving orders.
    [[nodiscard]] float getShipSpeedMultiplier() const {
        float mul = 1.0f;
        if (hasTech(8)) mul += 0.38f;   // Nuclear Engines
        if (hasTech(9)) mul += 0.65f;   // Fusion Engines (much faster)
        return mul;
    }

    [[nodiscard]] float getShipRangeBonus() const {
        float bonus = 0.0f;
        if (hasTech(8)) bonus += 50.0f;   // Nuclear
        if (hasTech(9)) bonus += 110.0f;  // Fusion dramatically extends reach
        return bonus;
    }
};

} // namespace orion