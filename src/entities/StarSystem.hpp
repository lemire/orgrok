#pragma once

#include "entities/Planet.hpp"
#include "raylib.h"   // Only for Vector2 in rendering layer - acceptable for now (position is pure data)
#include <string>
#include <vector>
#include <cstdint>

namespace orion {

// A star system containing 1-5 planets (MoO1 style).
// Position is in "galactic map" coordinates (we will define scale later).
struct StarSystem {
    std::string name;                    // "Sol", "Alpha Centauri", "Vega"...
    Vector2     position{0.0f, 0.0f};    // screen / logical map coords
    std::vector<Planet> planets;

    int         ownerEmpireId = -1;      // Empire that controls the system (usually the one with colonies here)
    int         starId = -1;             // Unique index in galaxy

    [[nodiscard]] bool hasColony() const {
        for (const auto& p : planets) {
            if (p.isColonized()) return true;
        }
        return false;
    }

    [[nodiscard]] int colonyCount() const {
        int count = 0;
        for (const auto& p : planets) if (p.isColonized()) ++count;
        return count;
    }
};

} // namespace orion