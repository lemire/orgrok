#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>

namespace orion {

// Hull sizes with base capacity (classic MoO feel)
enum class HullSize : uint8_t {
    Small = 0,    // Frigate / Destroyer
    Medium,       // Cruiser
    Large,        // Battleship / Dreadnought
    Huge          // Doom Star / Titan (future)
};

struct HullData {
    std::string name;
    int baseSpace;
    int basePower;
    float costMultiplier;
};

inline const std::array<HullData, 3> HULLS = {{
    {"Small (Frigate)",     25,  30, 0.8f},
    {"Medium (Cruiser)",    60,  70, 1.0f},
    {"Large (Battleship)", 120, 140, 1.5f}
}};

// Component categories (very simplified for Phase 2 start)
enum class ComponentCategory : uint8_t {
    Engine = 0,
    Weapon,
    Shield,
    Special,
    Count
};

struct ComponentDef {
    std::string name;
    ComponentCategory category;
    int spaceCost;
    int powerCost;
    int buildCost;      // used when building the ship
    std::string description;

    // Propulsion stats contributed by this component (especially Engines)
    float speedBonus = 0.0f;   // additive to base speed (parsecs per turn)
    float rangeBonus = 0.0f;   // additive to max jump range in parsecs

    // Additional Phase 2 stats
    int colonyPopBonus = 0;     // extra starting population when colonizing (for Colony Modules)
    float scannerBonus = 0.0f;  // future detection / exploration bonus
    int weaponPower = 0;        // placeholder for future combat
    int shieldStrength = 0;     // placeholder for future combat
};

// Available components (hardcoded for now - will be tech-unlocked later)
inline const std::vector<ComponentDef> AVAILABLE_COMPONENTS = {
    // Engines
    {"Basic Engines",      ComponentCategory::Engine,  4,  8,  30, "Standard sublight drive",  1.0f,  85.0f},
    {"Improved Engines",   ComponentCategory::Engine,  6, 12,  55, "Faster movement",           1.6f, 115.0f},
    {"Fusion Engines",     ComponentCategory::Engine,  9, 18,  95, "High-performance interstellar drive", 2.4f, 165.0f},

    // Weapons - each has a special combat effect
    {"Laser Battery",      ComponentCategory::Weapon,  5,  6,  40, "Reliable - bonus vs damaged targets", 0,0,0,0, 5},
    {"Mass Driver",        ComponentCategory::Weapon,  7,  8,  65, "Piercing - strong vs shields",       0,0,0,0, 9},
    {"Particle Cannon",    ComponentCategory::Weapon, 10, 14, 110, "High damage - bonus vs low shields", 0,0,0,0,14},

    // Shields
    {"Basic Shields",      ComponentCategory::Shield,  3, 10,  35, "Light deflection field",     0,0,0, 8},
    {"Deflector Shields",  ComponentCategory::Shield,  6, 15,  70, "Stronger protection",        0,0,0,14},
    {"Energy Shields",     ComponentCategory::Shield,  9, 20, 120, "Advanced energy absorption", 0,0,0,22},

    // Special
    {"Colony Module",      ComponentCategory::Special, 8,  4,  50, "Required for colonization",  0,0, 1},
    {"Improved Colony Pod",ComponentCategory::Special,11,  6,  85, "Larger starting population on new worlds", 0,0, 3},
    {"Scanner",            ComponentCategory::Special, 2,  3,  25, "Improved detection",          0,0.5f},
    {"Advanced Scanner",   ComponentCategory::Special, 5,  7,  70, "Long range sensors",          0,1.2f},
};

struct ShipDesign {
    std::string name = "New Design";
    HullSize hull = HullSize::Small;

    std::vector<int> componentIndices;   // indices into AVAILABLE_COMPONENTS

    // Cached stats
    int totalSpaceUsed = 0;
    int totalPowerUsed = 0;
    int buildCost = 0;

    // Additional cached gameplay stats
    int totalColonyPopBonus = 0;
    float totalScannerBonus = 0.0f;
    int totalWeaponPower = 0;
    int totalShieldStrength = 0;

    void recalculateStats() {
        totalSpaceUsed = 0;
        totalPowerUsed = 0;
        buildCost = 0;
        totalColonyPopBonus = 0;
        totalScannerBonus = 0.0f;
        totalWeaponPower = 0;
        totalShieldStrength = 0;

        const auto& hullData = HULLS[static_cast<int>(hull)];

        for (int idx : componentIndices) {
            if (idx >= 0 && idx < static_cast<int>(AVAILABLE_COMPONENTS.size())) {
                const auto& comp = AVAILABLE_COMPONENTS[idx];
                totalSpaceUsed += comp.spaceCost;
                totalPowerUsed += comp.powerCost;
                buildCost += comp.buildCost;

                totalColonyPopBonus += comp.colonyPopBonus;
                totalScannerBonus += comp.scannerBonus;
                totalWeaponPower += comp.weaponPower;
                totalShieldStrength += comp.shieldStrength;
            }
        }

        // Base hull cost
        buildCost += static_cast<int>(50 * hullData.costMultiplier);
    }

    bool isValid() const {
        const auto& hullData = HULLS[static_cast<int>(hull)];
        return totalSpaceUsed <= hullData.baseSpace &&
               totalPowerUsed <= hullData.basePower;
    }

    // === Propulsion stats (Phase 2 travel tech) ===
    [[nodiscard]] float getBaseSpeed() const {
        float speed = 0.8f; // minimum hull speed
        for (int idx : componentIndices) {
            if (idx >= 0 && idx < static_cast<int>(AVAILABLE_COMPONENTS.size())) {
                const auto& comp = AVAILABLE_COMPONENTS[idx];
                if (comp.category == ComponentCategory::Engine) {
                    speed += comp.speedBonus;
                }
            }
        }
        // Hull size penalty for larger ships (slightly slower)
        if (hull == HullSize::Medium) speed *= 0.92f;
        if (hull == HullSize::Large)  speed *= 0.82f;
        return std::max(0.6f, speed);
    }

    [[nodiscard]] float getMaxRange() const {
        float range = 70.0f; // baseline hull jump range
        for (int idx : componentIndices) {
            if (idx >= 0 && idx < static_cast<int>(AVAILABLE_COMPONENTS.size())) {
                const auto& comp = AVAILABLE_COMPONENTS[idx];
                if (comp.category == ComponentCategory::Engine) {
                    range += comp.rangeBonus;
                }
            }
        }
        // Larger hulls get a bit more range (better fuel tanks)
        if (hull == HullSize::Medium) range += 25.0f;
        if (hull == HullSize::Large)  range += 55.0f;
        return std::max(50.0f, range);
    }
};

// Player's saved designs
inline std::vector<ShipDesign> playerDesigns;

} // namespace orion