#pragma once

#include "entities/StarSystem.hpp"
#include <vector>
#include <cstdint>
#include <random>
#include <string>

namespace orion {

// The entire galaxy. Owns all star systems.
// Generation is handled by free functions in galaxy_generation.cpp (later file).
struct Galaxy {
    std::vector<StarSystem> systems;
    uint32_t                width  = 1000;   // logical units
    uint32_t                height = 800;

    int                     seed = 0;        // for reproducibility / save games

    [[nodiscard]] StarSystem* findSystemById(int id) {
        for (auto& sys : systems) {
            if (sys.starId == id) return &sys;
        }
        return nullptr;
    }

    [[nodiscard]] const StarSystem* findSystemById(int id) const {
        for (const auto& sys : systems) {
            if (sys.starId == id) return &sys;
        }
        return nullptr;
    }

    [[nodiscard]] size_t colonizedSystemCount() const {
        size_t count = 0;
        for (const auto& s : systems) if (s.hasColony()) ++count;
        return count;
    }
};

} // namespace orion