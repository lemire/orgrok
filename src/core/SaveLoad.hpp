#pragma once

#include "core/GameState.hpp"
#include <string>

namespace orion {

// Save the entire game state to a JSON file using simdjson builder API.
bool saveGame(const GameState& state, const std::string& filepath);

// Load a game state from a JSON file using simdjson ondemand parser.
// Fully restores Phase 2 state including designs, tech, colonies with allocations, AI empires, ships.
bool loadGame(GameState& state, const std::string& filepath);

} // namespace orion