#pragma once

#include <string>
#include <cstdint>
#include <vector>

#include "raylib.h"  // for Color

namespace orion {

// Very simplified Empire for Phase 1.
// Later: technology levels per category, race bonuses, relations, spies, etc.
struct Empire {
    int         id = -1;
    std::string name;
    std::string raceName;                // "Human", "Mrrshan", "Psilon"...

    // Economy (Phase 1)
    int         treasury = 1000;         // BC (billion credits)
    int         researchPool = 0;        // RP accumulated this turn (simplified)

    // Known systems / colonies (we will derive from Galaxy in real code)
    std::vector<int> ownedStarIds;

    // Racial bonuses (stub - will be data driven)
    float       populationGrowthMod = 1.0f;
    float       researchMod = 1.0f;
    float       shipCombatMod = 1.0f;
    float       productionMod = 1.0f;

    bool        isPlayer = false;

    Color       color = {255, 255, 255, 255};  // Flag / UI color for this empire
};

} // namespace orion