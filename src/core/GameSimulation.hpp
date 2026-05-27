#pragma once

#include "core/GameState.hpp"
#include "core/EconomyConfig.hpp"
#include "core/TechTree.hpp"

#include <string>
#include <vector>
#include <functional>

extern orion::GameState gGameState;
extern std::vector<std::string> gTurnReportMessages;
extern bool gShowTurnReport;

// Simulation-relevant types moved for extraction (Phase 3 leaders/events groundwork)
struct Leader {
    std::string name;
    std::string title;          // "Governor" or "Admiral"
    std::string bonusDesc;
    float bonusValue = 0.0f;    // e.g. 0.15 = +15%
    int assignedTo = -1;        // colony index or ship id, -1 = unassigned
};

struct GameEvent {
    std::string title;
    std::string description;
    std::function<void()> effect;
    bool isGood;
};

extern std::vector<Leader> gLeaders;
extern std::vector<GameEvent> gPossibleEvents;
extern GameEvent gCurrentEvent;
extern bool gShowEventPopup;
extern std::vector<std::string> gCombatLog;

extern bool gShowTechChoice;
extern std::vector<int> gAvailableTechChoices;

void initializeGame(const std::string& playerRace);
void processEndOfTurn();  // moved (staged extraction complete)

// Simple helper for tests
void resetGameToNewGame(const std::string& race = "Human");

// Data-driven configuration (loaded at new game start)
extern orion::EconomyConfig gEconomyConfig;
extern orion::TechTree gTechTree;

// Convenience accessors that combine loaded data with current game state
int getCurrentFactoriesPerPop();

// Travel & combat helpers (moved in stages so processEndOfTurn can live in core)
float GetSystemDistance(const orion::StarSystem* a, const orion::StarSystem* b);
int GetTravelETA(float distance, float shipSpeed);
void resolveShipCombat();

// GUI / interface helpers (extracted so both the real UI and GUI simulation tests can use the same logic)
std::vector<int> getPlayerColoniesWithoutProduction();
