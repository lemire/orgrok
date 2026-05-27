#pragma once

#include "core/Enums.hpp"
#include <cstdint>
#include <string>

namespace orion {

// Represents a single planet. Pure data for now (no logic).
struct Planet {
    std::string name;                    // e.g. "Sol III", "Alpha Centauri II"

    PlanetSize  size   = PlanetSize::Medium;
    PlanetType  type   = PlanetType::Terran;
    Richness    richness = Richness::Abundant;
    Gravity     gravity = Gravity::Normal;

    uint32_t    traits = 0;              // bitmask of PlanetTrait

    // Current colony (if any). For Phase 1 we keep it extremely simple:
    // A non-zero ownerEmpireId means the planet is colonized.
    // Later this will become a proper Colony object or index.
    int         ownerEmpireId = -1;      // -1 = uncolonized
    float       population    = 0.0f;    // in millions (classic MoO scale)

    // Cached / derived values (recomputed on turn processing)
    int         maxPopulation = 0;       // based on size + gravity + race + tech

    // Simple resource output last turn (for UI)
    float       foodOutput     = 0.0f;
    float       production     = 0.0f;
    float       research       = 0.0f;

    [[nodiscard]] bool isColonized() const { return ownerEmpireId >= 0; }
    [[nodiscard]] bool canBeColonized() const {
        return !isColonized() && (type != PlanetType::Radiated || type == PlanetType::Gaia); // simplistic
    }
};

} // namespace orion