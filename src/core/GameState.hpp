#pragma once

#include "core/Galaxy.hpp"
#include "core/Empire.hpp"
#include "entities/Colony.hpp"
#include "entities/Ship.hpp"
#include "core/Technology.hpp"
#include <vector>
#include <memory>
#include <cstdint>

namespace orion {

// Central container for the entire game state.
// This will grow to include tech tree state, ship designs, diplomatic relations, etc.
struct GameState {
    Galaxy galaxy;
    std::vector<Empire> empires;         // Empire 0 is always the human player in Phase 1

    int currentTurn = 1;
    int selectedStarId = -1;             // UI selection

    // Very temporary colony storage (Phase 1). In real architecture colonies live on planets.
    std::vector<Colony> colonies;

    std::vector<Ship> ships;   // Early ship support for colonization gameplay

    orion::TechnologyState technology;  // Phase 2 tech tree

    [[nodiscard]] Empire& playerEmpire() { return empires[0]; }
    [[nodiscard]] const Empire& playerEmpire() const { return empires[0]; }

    [[nodiscard]] bool isPlayerEmpire(int id) const { return id == 0; }
};

} // namespace orion