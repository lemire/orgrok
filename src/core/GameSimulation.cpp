#include "core/GameSimulation.hpp"

#include "core/GameState.hpp"
#include "core/GalaxyGeneration.hpp"
#include "core/Technology.hpp"
#include "core/Empire.hpp"
#include "entities/Ship.hpp"
#include "entities/Colony.hpp"
#include "entities/ShipDesign.hpp"

#include <vector>
#include <string>
#include <iostream>
#include <map>
#include <algorithm>
#include <cmath>

// Globals that initializeGame/processEndOfTurn depend on (provided by main.cpp at link time for the exe;
// tests can define their own instances when exercising simulation functions).

// Globals that initializeGame depends on (declared in main.cpp for now)
// Leader is now defined in GameSimulation.hpp so the type is complete for this TU.
// gLeaders symbol is provided by main.cpp at link time.
// (playerDesigns is visible because we #include "entities/ShipDesign.hpp" which provides the inline global inside namespace orion)

// Moved InitRandomEvents here (from main.cpp) as part of staged extraction.
// This lets initializeGame() and future processEndOfTurn() work when linking only orion_core (for tests).
void InitRandomEvents() {
    gPossibleEvents.clear();

    GameEvent e1;
    e1.title = "Scientific Breakthrough";
    e1.description = "Your researchers have made an unexpected discovery!";
    e1.effect = []() {
        auto& emp = gGameState.playerEmpire();
        emp.researchPool += 80;
        gCombatLog.push_back("Event: Scientific Breakthrough (+80 RP)");
    };
    e1.isGood = true;
    gPossibleEvents.push_back(e1);

    GameEvent e2;
    e2.title = "Economic Boom";
    e2.description = "A period of prosperity increases your treasury.";
    e2.effect = []() {
        auto& emp = gGameState.playerEmpire();
        emp.treasury += 120;
        gCombatLog.push_back("Event: Economic Boom (+120 BC)");
    };
    e2.isGood = true;
    gPossibleEvents.push_back(e2);

    GameEvent e3;
    e3.title = "Fertile World Discovered";
    e3.description = "One of your colonies reports unusually high growth this year.";
    e3.effect = []() {
        if (!gGameState.colonies.empty()) {
            int idx = rand() % gGameState.colonies.size();
            gGameState.colonies[idx].population += 0.8f;
            gCombatLog.push_back("Event: Fertile conditions on a colony");
        }
    };
    e3.isGood = true;
    gPossibleEvents.push_back(e3);

    GameEvent e4;
    e4.title = "Pirate Raid";
    e4.description = "Space pirates have attacked one of your systems!";
    e4.effect = []() {
        auto& emp = gGameState.playerEmpire();
        emp.treasury = std::max(0, emp.treasury - 60);
        for (auto& sh : gGameState.ships) {
            if (sh.ownerId == 0) {
                sh.weaponPower = std::max(0, sh.weaponPower - 2);
                sh.shieldStrength = std::max(0, sh.shieldStrength - 3);
                gCombatLog.push_back("Event: Pirates damaged one of your ships");
                break;
            }
        }
    };
    e4.isGood = false;
    gPossibleEvents.push_back(e4);

    GameEvent e5;
    e5.title = "Industrial Accident";
    e5.description = "A major accident has occurred on one of your colonies.";
    e5.effect = []() {
        if (!gGameState.colonies.empty()) {
            int idx = rand() % gGameState.colonies.size();
            gGameState.colonies[idx].population = std::max(0.5f, gGameState.colonies[idx].population - 0.6f);
            gCombatLog.push_back("Event: Industrial accident reduced population");
        }
    };
    e5.isGood = false;
    gPossibleEvents.push_back(e5);

    GameEvent e6;
    e6.title = "Diplomatic Incident";
    e6.description = "Tensions with a rival empire have flared.";
    e6.effect = []() {
        auto& emp = gGameState.playerEmpire();
        emp.researchPool = std::max(0, emp.researchPool - 40);
        gCombatLog.push_back("Event: Diplomatic incident (-40 RP)");
    };
    e6.isGood = false;
    gPossibleEvents.push_back(e6);
}

// === Travel helpers (Phase 2 tech) - moved for extraction ===
float GetSystemDistance(const orion::StarSystem* a, const orion::StarSystem* b) {
    if (!a || !b) return 99999.0f;
    float dx = a->position.x - b->position.x;
    float dy = a->position.y - b->position.y;
    return sqrtf(dx*dx + dy*dy);
}

int GetTravelETA(float distance, float shipSpeed) {
    if (shipSpeed <= 0.01f) return 999;
    return (int)ceilf(distance / shipSpeed);
}

// === Combat Resolution (Phase 2 - improved depth) - moved for extraction stage ===
static int GetShipCombatPower(const orion::Ship* sh) {
    if (!sh) return 0;
    int base = sh->weaponPower;

    // Type effectiveness
    if (sh->type == orion::ShipType::Destroyer) base = static_cast<int>(base * 1.35f);
    if (sh->type == orion::ShipType::Scout)    base = static_cast<int>(base * 0.65f);
    if (sh->type == orion::ShipType::ColonyShip) base = static_cast<int>(base * 0.4f);

    // Weapon type special effects (Phase 2 depth)
    switch (sh->primaryWeapon) {
        case orion::WeaponType::ParticleCannon:
            base = static_cast<int>(base * 1.22f);  // High raw damage
            break;
        case orion::WeaponType::MassDriver:
            base = static_cast<int>(base * 1.08f);  // Slightly better overall
            break;
        case orion::WeaponType::Laser:
            base = static_cast<int>(base * 1.0f);   // Reliable baseline
            break;
        default:
            break;
    }

    return std::max(0, base);
}

void resolveShipCombat() {
    std::map<int, std::vector<orion::Ship*>> shipsBySystem;

    for (auto& sh : gGameState.ships) {
        if (sh.locationSystemId != -1) {
            shipsBySystem[sh.locationSystemId].push_back(&sh);
        }
    }

    for (auto& [sysId, shipsInSys] : shipsBySystem) {
        std::vector<orion::Ship*> playerShips;
        std::vector<orion::Ship*> enemyShips;

        for (auto* sh : shipsInSys) {
            if (sh->ownerId == 0) playerShips.push_back(sh);
            else enemyShips.push_back(sh);
        }

        if (playerShips.empty() || enemyShips.empty()) continue;

        // Fleet size modifier (bonus for outnumbering the enemy)
        float playerFleetMod = 1.0f;
        float enemyFleetMod  = 1.0f;

        int pCount = static_cast<int>(playerShips.size());
        int eCount = static_cast<int>(enemyShips.size());

        if (pCount > eCount * 1.5f) playerFleetMod = 1.15f;
        if (eCount > pCount * 1.5f) enemyFleetMod  = 1.15f;
        if (pCount >= eCount * 2)   playerFleetMod = 1.25f;
        if (eCount >= pCount * 2)   enemyFleetMod  = 1.25f;

        // Calculate total effective power
        int playerPower = 0;
        for (auto* sh : playerShips) {
            playerPower += GetShipCombatPower(sh);
        }
        playerPower = static_cast<int>(playerPower * playerFleetMod);

        int enemyPower = 0;
        for (auto* sh : enemyShips) {
            enemyPower += GetShipCombatPower(sh);
        }
        enemyPower = static_cast<int>(enemyPower * enemyFleetMod);

        // Improved per-ship damage application
        auto applyDamageToGroup = [](std::vector<orion::Ship*>& group, int incomingDamage) -> int {
            if (group.empty() || incomingDamage <= 0) return 0;

            int totalDestroyed = 0;
            int remainingDamage = incomingDamage;

            // Sort by remaining shield strength (weakest first)
            std::sort(group.begin(), group.end(), [](orion::Ship* a, orion::Ship* b) {
                return a->shieldStrength < b->shieldStrength;
            });

            for (auto* sh : group) {
                if (remainingDamage <= 0) break;

                // === Weapon Special Effects ===
                int shieldAbsorb = sh->shieldStrength;
                int effectiveDamage = remainingDamage;

                switch (sh->primaryWeapon) {
                    case orion::WeaponType::MassDriver:
                        // Mass Drivers are excellent at punching through shields
                        shieldAbsorb = static_cast<int>(shieldAbsorb * 0.65f);
                        break;
                    case orion::WeaponType::ParticleCannon:
                        // Particle Cannons do bonus damage when shields are low
                        if (sh->shieldStrength < 6) {
                            effectiveDamage = static_cast<int>(effectiveDamage * 1.25f);
                        }
                        break;
                    case orion::WeaponType::Laser:
                        // Lasers are reliable - slight bonus vs already damaged targets
                        if (sh->shieldStrength < 4 || sh->weaponPower < 3) {
                            effectiveDamage = static_cast<int>(effectiveDamage * 1.12f);
                        }
                        break;
                    default:
                        break;
                }

                int damageAfterShields = std::max(0, effectiveDamage - shieldAbsorb);

                // Shields take some permanent reduction
                sh->shieldStrength = std::max(0, sh->shieldStrength - (remainingDamage / 3));

                // Apply remaining damage to "hull" (abstracted)
                if (damageAfterShields > (sh->weaponPower + 4)) {
                    // Ship is destroyed
                    sh->weaponPower = 0;
                    sh->shieldStrength = 0;
                    sh->locationSystemId = -999; // mark for removal
                    totalDestroyed++;
                    remainingDamage -= (sh->weaponPower + 8); // some overkill
                } else if (damageAfterShields > 0) {
                    // Ship is damaged
                    sh->weaponPower = std::max(0, sh->weaponPower - (damageAfterShields / 2));
                    remainingDamage -= damageAfterShields;
                } else {
                    remainingDamage -= shieldAbsorb;
                }
            }

            return totalDestroyed;
        };

        int destroyedEnemy  = applyDamageToGroup(enemyShips, playerPower);
        int destroyedPlayer = applyDamageToGroup(playerShips, enemyPower);

        // Clean up destroyed ships
        auto it = gGameState.ships.begin();
        while (it != gGameState.ships.end()) {
            if (it->locationSystemId == -999) {
                it = gGameState.ships.erase(it);
            } else {
                ++it;
            }
        }

        // Logging
        if (destroyedPlayer > 0 || destroyedEnemy > 0) {
            std::string msg = "Combat in system " + std::to_string(sysId) + ": ";
            if (destroyedPlayer > 0) msg += std::to_string(destroyedPlayer) + " friendly ship(s) lost. ";
            if (destroyedEnemy > 0)  msg += std::to_string(destroyedEnemy)  + " enemy ship(s) destroyed.";
            if (pCount != eCount) {
                msg += " (Fleet advantage applied)";
            }
            gCombatLog.push_back(msg);
            if (gCombatLog.size() > 8) gCombatLog.erase(gCombatLog.begin());
        }

        // === Retreat Logic (AI ships) ===
        // Heavily damaged AI military ships try to flee to the nearest friendly system
        for (auto* sh : shipsInSys) {
            if (sh->ownerId == 0) continue; // only AI
            if (sh->locationSystemId == -999) continue;

            bool isMilitary = (sh->type == orion::ShipType::Destroyer);
            int effectiveStrength = sh->shieldStrength + sh->weaponPower;

            if (isMilitary && effectiveStrength < 4) {
                // Try to find a nearby friendly system to retreat to
                auto* current = gGameState.galaxy.findSystemById(sh->locationSystemId);
                int bestTarget = -1;
                float bestDist = 99999.0f;

                for (const auto& sys : gGameState.galaxy.systems) {
                    if (sys.ownerEmpireId != sh->ownerId) continue;
                    if (sys.starId == sh->locationSystemId) continue;

                    float d = GetSystemDistance(current, &sys);
                    if (d < bestDist) {
                        bestDist = d;
                        bestTarget = sys.starId;
                    }
                }

                if (bestTarget != -1) {
                    sh->destinationSystemId = bestTarget;
                    sh->isMoving = true;
                    sh->travelProgress = 0.0f;
                    // Small log entry
                    gCombatLog.push_back("Damaged AI ship retreating from system " + std::to_string(sysId));
                    if (gCombatLog.size() > 8) gCombatLog.erase(gCombatLog.begin());
                }
            }
        }
    }
}

void resetGameToNewGame(const std::string& race) {
    gGameState = orion::GameState{};
    gTurnReportMessages.clear();
    gShowTurnReport = false;

    initializeGame(race);
}

void initializeGame(const std::string& playerRace) {
    InitRandomEvents();  // Phase 3

    // Basic Phase 3 Leaders
    gLeaders.clear();
    gLeaders.push_back({"Dr. Elena Voss", "Governor", "+20% Research output", 0.20f, -1});
    gLeaders.push_back({"Admiral Kael Thorne", "Admiral", "+15% Fleet combat power", 0.15f, -1});
    gLeaders.push_back({"Industrialist Rax", "Governor", "+15% Production", 0.15f, -1});

    // Generate a nice sized galaxy (55 stars is classic MoO "medium" feel)
    gGameState.galaxy = orion::generateGalaxy(55, 20260526);
    gGameState.empires.clear();
    orion::playerDesigns.clear();  // fresh designer state on new game (Phase 2)
    auto player = orion::createPlayerEmpire(playerRace);
    player.color = {100, 180, 255, 255};  // Light blue
    gGameState.empires.push_back(player);  // Phase 2: different starting race

    // Give the player a nice starting colony on the first "good" planet we find
    bool started = false;
    for (auto& sys : gGameState.galaxy.systems) {
        for (auto& pl : sys.planets) {
            if (!started && (pl.type == orion::PlanetType::Terran || pl.type == orion::PlanetType::Ocean || pl.type == orion::PlanetType::Gaia)) {
                pl.ownerEmpireId = 0;
                pl.population = (pl.size == orion::PlanetSize::Large || pl.size == orion::PlanetSize::Huge) ? 3.2f : 2.1f;

                orion::Colony col{};
                col.planetId = sys.starId; // simplistic mapping for Phase 1
                col.ownerId = 0;
                col.population = pl.population;
                col.maxPopulation = static_cast<float>(pl.maxPopulation);
                gGameState.colonies.push_back(col);

                sys.ownerEmpireId = 0;
                started = true;
                gGameState.selectedStarId = sys.starId;
                break;
            }
        }
        if (started) break;
    }

    if (!started) {
        // Fallback: colonize whatever is first
        auto& first = gGameState.galaxy.systems[0];
        if (!first.planets.empty()) {
            first.planets[0].ownerEmpireId = 0;
            first.planets[0].population = 2.0f;
            first.ownerEmpireId = 0;

            orion::Colony col{};
            col.ownerId = 0;
            col.population = 2.0f;
            gGameState.colonies.push_back(col);
            gGameState.selectedStarId = first.starId;
        }
    }

    // === Phase 2: Add basic AI empires ===
    // AI Empire 1 - Mrrshan (aggressive)
    orion::Empire ai1 = orion::createPlayerEmpire("Mrrshan");
    ai1.id = 1;
    ai1.isPlayer = false;
    ai1.color = {255, 90, 70, 255};  // Aggressive red
    gGameState.empires.push_back(ai1);

    // Give the Mrrshan one good starting colony
    for (auto& sys : gGameState.galaxy.systems) {
        if (sys.ownerEmpireId == -1) {
            for (auto& pl : sys.planets) {
                if (pl.type >= orion::PlanetType::Ocean && !pl.isColonized()) {
                    pl.ownerEmpireId = 1;
                    pl.population = 2.5f;
                    sys.ownerEmpireId = 1;

                    orion::Colony col{};
                    col.ownerId = 1;
                    col.population = 2.5f;
                    col.planetId = sys.starId;
                    col.maxPopulation = static_cast<float>(pl.maxPopulation);
                    gGameState.colonies.push_back(col);
                    goto ai1_done;
                }
            }
        }
    }
    ai1_done:;

    // AI Empire 2 - Silicoid (slow but tough)
    orion::Empire ai2 = orion::createPlayerEmpire("Silicoid");
    ai2.id = 2;
    ai2.isPlayer = false;
    ai2.color = {170, 150, 130, 255};  // Rocky gray-brown
    gGameState.empires.push_back(ai2);

    // Give the Silicoid one starting colony
    for (auto& sys : gGameState.galaxy.systems) {
        if (sys.ownerEmpireId == -1) {
            for (auto& pl : sys.planets) {
                if (pl.type >= orion::PlanetType::Arid && !pl.isColonized()) {
                    pl.ownerEmpireId = 2;
                    pl.population = 1.8f;
                    sys.ownerEmpireId = 2;

                    orion::Colony col{};
                    col.ownerId = 2;
                    col.population = 1.8f;
                    col.planetId = sys.starId;
                    col.maxPopulation = static_cast<float>(pl.maxPopulation);
                    gGameState.colonies.push_back(col);
                    goto ai2_done;
                }
            }
        }
    }
    ai2_done:;

    // === Phase 2: Give every empire starting ships (2 Scouts + 1 Colony Ship) ===
    auto spawnStartingShips = [](int empireId, int systemId) {
        // 2 Scouts
        for (int i = 0; i < 2; ++i) {
            orion::Ship scout;
            scout.id = static_cast<int>(gGameState.ships.size());
            scout.type = orion::ShipType::Scout;
            scout.ownerId = empireId;
            scout.locationSystemId = systemId;
            scout.name = (empireId == 0 ? "Scout" : "AI Scout") + (" #" + std::to_string(i + 1));
            scout.effectiveSpeed = 1.35f;
            scout.maxRange = 105.0f;
            scout.weaponPower = 2;
            scout.shieldStrength = 1;
            gGameState.ships.push_back(scout);
        }

        // 1 Colony Ship
        orion::Ship colonyShip;
        colonyShip.id = static_cast<int>(gGameState.ships.size());
        colonyShip.type = orion::ShipType::ColonyShip;
        colonyShip.ownerId = empireId;
        colonyShip.locationSystemId = systemId;
        colonyShip.name = (empireId == 0 ? "Colony Ship" : "AI Colony Ship");
        colonyShip.effectiveSpeed = 1.0f;
        colonyShip.maxRange = 90.0f;
        gGameState.ships.push_back(colonyShip);
    };

    // Spawn starting ships for all empires
    spawnStartingShips(0, gGameState.selectedStarId); // Player

    // Find AI1's capital
    for (const auto& sys : gGameState.galaxy.systems) {
        if (sys.ownerEmpireId == 1) {
            spawnStartingShips(1, sys.starId);
            break;
        }
    }

    // Find AI2's capital
    for (const auto& sys : gGameState.galaxy.systems) {
        if (sys.ownerEmpireId == 2) {
            spawnStartingShips(2, sys.starId);
            break;
        }
    }
}

// processEndOfTurn moved here (final stage of the requested extraction).
// All its helper dependencies (combat, travel, InitRandomEvents, types) were moved in prior stages.
// This enables future Catch2 simulation tests to drive full turns using only orion_core.
void processEndOfTurn() {
    gTurnReportMessages.clear();  // Start fresh report for this turn

    auto& player = gGameState.playerEmpire();

    // Record starting values for better reporting
    int startingTreasury = player.treasury;
    int startingResearch = player.researchPool;

    int totalColonies = 0;

    for (auto& sys : gGameState.galaxy.systems) {
        for (auto& pl : sys.planets) {
            if (pl.ownerEmpireId == 0) {
                ++totalColonies;

                // Very simple growth (classic MoO style: food surplus + race bonus)
                float growth = 0.08f + (pl.population < 3.0f ? 0.04f : 0.0f);
                if (pl.traits & static_cast<uint32_t>(orion::PlanetTrait::Fertile)) growth *= 1.25f;

                pl.population = std::min(static_cast<float>(pl.maxPopulation), pl.population + growth);

                // Update the matching colony record and use real allocation-based production
                for (auto& col : gGameState.colonies) {
                    if (col.planetId == sys.starId) {
                        col.population = pl.population;

                        // Use the new model + tech bonus
                        float techBonus = gGameState.technology.getIndustryBonus();

                        // Find owning empire for racial bonuses
                        float prodMod = 1.0f;
                        float resMod = 1.0f;
                        float growthMod = 1.0f;
                        for (const auto& emp : gGameState.empires) {
                            if (emp.id == col.ownerId) {
                                prodMod = emp.productionMod;
                                resMod = emp.researchMod;
                                growthMod = emp.populationGrowthMod;
                                break;
                            }
                        }

                        col.recalculateOutputs(pl.size, pl.type, pl.richness, pl.traits,
                                               static_cast<float>(pl.maxPopulation), techBonus, prodMod);

                        // Apply results to empire (now using the already-modded netProduction from the formula)
                        player.treasury += static_cast<int>(col.netProduction * 0.85f + 7);
                        player.researchPool += static_cast<int>(col.researchOutput * 0.75f + 4);

                        // Apply production to active building project
                        if (col.projectCost > 0 && col.projectProgress < col.projectCost) {
                            float spent = col.netProduction * 0.85f;  // Higher % goes into the active project
                            float before = col.projectProgress;
                            col.projectProgress += spent;

                            // Report progress occasionally (every turn for visibility in report)
                            if (col.projectCost > 0) {
                                float pctBefore = (before / col.projectCost) * 100.0f;
                                float pctAfter = (col.projectProgress / col.projectCost) * 100.0f;
                                if (pctAfter > pctBefore + 5.0f || col.projectProgress >= col.projectCost) {  // meaningful progress
                                    auto* colSys = gGameState.galaxy.findSystemById(col.planetId);
                                    std::string projName = col.currentProject;
                                    gTurnReportMessages.push_back("Production at " + (colSys ? colSys->name : "colony") + ": " + projName + " now at " + std::to_string((int)pctAfter) + "%.");
                                }
                            }

                            if (col.projectProgress >= col.projectCost) {
                                col.projectProgress = col.projectCost;

                                // Spawn a real ship when a design (or generic colony ship) finishes
                                if (col.currentProject.find("Build:") != std::string::npos) {
                                    // This is a saved design
                                    std::string designName = col.currentProject.substr(7); // after "Build: "
                                    orion::Ship newShip;
                                    newShip.id = static_cast<int>(gGameState.ships.size());
                                    newShip.type = orion::ShipType::ColonyShip;
                                    newShip.locationSystemId = sys.starId;
                                    newShip.name = designName;
                                    newShip.designName = designName;

                                    // Compute travel + combat stats from the saved design + current tech
                                    for (const auto& d : orion::playerDesigns) {
                                        if (d.name == designName) {
                                            newShip.effectiveSpeed = d.getBaseSpeed();
                                            newShip.maxRange = d.getMaxRange();
                                            newShip.weaponPower = d.totalWeaponPower;
                                            newShip.shieldStrength = d.totalShieldStrength;

                                            // Determine primary weapon type for special effects
                                            newShip.primaryWeapon = orion::WeaponType::None;
                                            int bestWeaponTier = 0;
                                            for (int compIdx : d.componentIndices) {
                                                if (compIdx >= 0 && compIdx < static_cast<int>(orion::AVAILABLE_COMPONENTS.size())) {
                                                    const auto& comp = orion::AVAILABLE_COMPONENTS[compIdx];
                                                    if (comp.category == orion::ComponentCategory::Weapon) {
                                                        if (comp.name == "Particle Cannon" && bestWeaponTier < 3) {
                                                            newShip.primaryWeapon = orion::WeaponType::ParticleCannon;
                                                            bestWeaponTier = 3;
                                                        } else if (comp.name == "Mass Driver" && bestWeaponTier < 2) {
                                                            newShip.primaryWeapon = orion::WeaponType::MassDriver;
                                                            bestWeaponTier = 2;
                                                        } else if (comp.name == "Laser Battery" && bestWeaponTier < 1) {
                                                            newShip.primaryWeapon = orion::WeaponType::Laser;
                                                            bestWeaponTier = 1;
                                                        }
                                                    }
                                                }
                                            }
                                            break;
                                        }
                                    }
                                    // Apply global propulsion tech bonuses (affects all ships)
                                    newShip.effectiveSpeed *= gGameState.technology.getShipSpeedMultiplier();
                                    newShip.maxRange   += gGameState.technology.getShipRangeBonus();

                                    gGameState.ships.push_back(newShip);
                                } else if (col.currentProject.find("Colony Ship") != std::string::npos) {
                                    // Generic colony ship (not from designer)
                                    orion::Ship newShip;
                                    newShip.id = static_cast<int>(gGameState.ships.size());
                                    newShip.type = orion::ShipType::ColonyShip;
                                    newShip.locationSystemId = sys.starId;
                                    newShip.name = "Colony Ship #" + std::to_string(newShip.id + 1);

                                    // Defaults for generic ships + tech
                                    newShip.effectiveSpeed = 1.05f;
                                    newShip.maxRange = 95.0f;
                                    newShip.effectiveSpeed *= gGameState.technology.getShipSpeedMultiplier();
                                    newShip.maxRange   += gGameState.technology.getShipRangeBonus();

                                    gGameState.ships.push_back(newShip);
                                }

                                // Record completed building and reset
                                if (!col.currentProject.empty() && col.currentProject != "None") {
                                    col.completedBuildings.push_back(col.currentProject);
                                    auto* colSys = gGameState.galaxy.findSystemById(col.planetId);
                                    gTurnReportMessages.push_back("Completed: " + col.currentProject + " at " + (colSys ? colSys->name : "colony") + "!");
                                }
                                col.currentProject = "None";
                                col.projectCost = 0;
                                col.projectProgress = 0.0f;
                            }
                        }

                        // Growth already applied via colony model in recalculateOutputs + small bonus (racial growth mod)
                        float growth = (col.foodNet > 0.8f) ? 0.11f : 0.04f;
                        if (pl.traits & static_cast<uint32_t>(orion::PlanetTrait::Fertile)) growth *= 1.25f;
                        growth *= growthMod;
                        pl.population = std::min(static_cast<float>(pl.maxPopulation), pl.population + growth);
                        col.population = pl.population;
                        break;
                    }
                }
            }
        }
    }

    gGameState.currentTurn++;
    // Small bonus for having more colonies (encourages expansion)
    player.treasury += std::max(0, totalColonies - 1) * 18;

    // === Ship movement (simple turn-based) ===
    for (auto& sh : gGameState.ships) {
        if (!sh.isMoving || sh.destinationSystemId == -1) continue;

        // Use the ship's own effective speed (normalized to progress per turn)
        // We treat ~1.0 speed as roughly 0.28 progress units per turn on a typical jump.
        float progressPerTurn = sh.effectiveSpeed * 0.26f;
        sh.travelProgress += std::max(0.08f, progressPerTurn);

        if (sh.travelProgress >= 1.0f) {
            int previousLocation = sh.locationSystemId;
            int arrivalSystem = sh.destinationSystemId;

            // Arrived
            sh.locationSystemId = arrivalSystem;
            sh.destinationSystemId = -1;
            sh.travelProgress = 0.0f;
            sh.isMoving = false;

            // Report ship movement completion
            auto* arrivedSys = gGameState.galaxy.findSystemById(arrivalSystem);
            std::string arrivalName = arrivedSys ? arrivedSys->name : "Unknown System";
            gTurnReportMessages.push_back(sh.name + " arrived at " + arrivalName + ".");

            // Phase 3: Exploration announcement + marking
            if (previousLocation != arrivalSystem) {
                if (arrivedSys && arrivedSys->ownerEmpireId == -1) {
                    // Mark as explored (we'll use ownerEmpireId == -1 as "unexplored" for now,
                    // or add explicit flag later if needed)
                    gTurnReportMessages.push_back("New system explored: " + arrivalName + "!");
                }
            }

            // Colony ship special handling
            if (sh.type == orion::ShipType::ColonyShip) {
                if (arrivedSys) {
                    bool hasColonizable = false;
                    for (const auto& p : arrivedSys->planets) {
                        if (!p.isColonized() && p.canBeColonized()) {
                            hasColonizable = true;
                            break;
                        }
                    }
                    if (hasColonizable && arrivedSys->ownerEmpireId == -1) {
                        gTurnReportMessages.push_back(sh.name + " is ready to colonize " + arrivalName + ".");
                        // Future: trigger colonization choice menu here
                        // For now, we log it prominently in the report
                    }
                }
            }
        }
    }

    // === Combat Groundwork (Phase 2) ===
    // Simple turn-based combat when hostile ships share a system.
    // This is lightweight groundwork — no full tactical combat yet.
    resolveShipCombat();

    // === Light Technology Tree (Phase 2) ===
    // Accumulate research from colonies + tech bonus
    float researchBonus = gGameState.technology.getResearchBonus();
    int totalResearch = 0;
    for (const auto& col : gGameState.colonies) {
        totalResearch += static_cast<int>(col.researchOutput * researchBonus);
    }
    gGameState.technology.researchThisTurn = totalResearch;
    player.researchPool += totalResearch;

    // === Phase 2: AI Empires now actively use ships ===
    for (auto& emp : gGameState.empires) {
        if (emp.isPlayer) continue;

        // Passive income (scaled by racial mods)
        int aiColonies = 0;
        for (const auto& col : gGameState.colonies) {
            if (col.ownerId == emp.id) {
                aiColonies++;
                emp.treasury += static_cast<int>(45 * emp.productionMod);
                emp.researchPool += static_cast<int>(9 * emp.researchMod);
            }
        }

        // 1. AI ship orders: Scouts explore, Colony Ships colonize
        for (auto& sh : gGameState.ships) {
            if (sh.ownerId != emp.id) continue;
            if (sh.isMoving) continue;

            if (sh.type == orion::ShipType::Scout) {
                // Scouts go explore unowned systems (simple random-ish behavior)
                int exploreTarget = -1;
                float bestExplore = 999999.0f;
                auto* from = gGameState.galaxy.findSystemById(sh.locationSystemId);

                for (const auto& sys : gGameState.galaxy.systems) {
                    if (sys.ownerEmpireId != -1) continue;
                    if (from) {
                        float d = GetSystemDistance(from, &sys);
                        if (d < bestExplore && d > 10.0f) {
                            bestExplore = d;
                            exploreTarget = sys.starId;
                        }
                    }
                }
                if (exploreTarget != -1 && exploreTarget != sh.locationSystemId) {
                    sh.destinationSystemId = exploreTarget;
                    sh.isMoving = true;
                    sh.travelProgress = 0.0f;
                }
                continue;
            }

            if (sh.type != orion::ShipType::ColonyShip) continue;

            // Find best unowned target (improved AI - considers distance + player competition)
            int bestTarget = -1;
            float bestScore = 0.0f;

            auto* homeSys = gGameState.galaxy.findSystemById(sh.locationSystemId);

            for (const auto& sys : gGameState.galaxy.systems) {
                if (sys.ownerEmpireId != -1) continue;
                for (const auto& pl : sys.planets) {
                    if (pl.isColonized()) continue;
                    if (pl.type < orion::PlanetType::Arid) continue;

                    float score = (pl.type >= orion::PlanetType::Terran) ? 110.0f : 70.0f;
                    score += pl.maxPopulation * 2.5f;

                    // Distance penalty / bonus using real travel math
                    if (homeSys) {
                        float dist = GetSystemDistance(homeSys, &sys);
                        float effectiveSpeed = std::max(0.8f, sh.effectiveSpeed);
                        int eta = GetTravelETA(dist, effectiveSpeed);
                        score -= eta * 1.8f;  // closer is better
                    }

                    // Slight competition awareness: if a planet is near player systems, rush it
                    for (const auto& other : gGameState.galaxy.systems) {
                        if (other.ownerEmpireId == 0) {
                            float d2 = GetSystemDistance(&sys, &other);
                            if (d2 < 180.0f) score += 25.0f;  // contested good planet
                        }
                    }

                    if (score > bestScore) {
                        bestScore = score;
                        bestTarget = sys.starId;
                    }
                }
            }

            if (bestTarget != -1 && bestTarget != sh.locationSystemId) {
                sh.destinationSystemId = bestTarget;
                sh.isMoving = true;
                sh.travelProgress = 0.0f;
            }
        }

        // 2. AI builds a variety of ships (Phase 2 improvement)
        int colonyShips = 0;
        int scoutShips = 0;
        int militaryShips = 0;
        for (const auto& sh : gGameState.ships) {
            if (sh.ownerId != emp.id) continue;
            if (sh.type == orion::ShipType::ColonyShip) colonyShips++;
            else if (sh.type == orion::ShipType::Scout) scoutShips++;
            else militaryShips++;
        }

        bool hasWeaponsTech = gGameState.technology.hasTech(10) || gGameState.technology.hasTech(11);

        // Build Scouts for exploration (cheaper, good for finding planets)
        if (scoutShips < 2 && (gGameState.currentTurn % 6 == 0) && emp.treasury > 80) {
            for (const auto& sys : gGameState.galaxy.systems) {
                if (sys.ownerEmpireId == emp.id) {
                    orion::Ship scout;
                    scout.id = static_cast<int>(gGameState.ships.size());
                    scout.type = orion::ShipType::Scout;
                    scout.ownerId = emp.id;
                    scout.locationSystemId = sys.starId;
                    scout.name = "AI Scout #" + std::to_string(scoutShips + 1);
                    scout.effectiveSpeed = 1.4f;
                    scout.maxRange = 110.0f;
                    scout.effectiveSpeed *= gGameState.technology.getShipSpeedMultiplier();
                    scout.maxRange   += gGameState.technology.getShipRangeBonus();
                    scout.weaponPower = 3;
                    scout.shieldStrength = 2;
                    gGameState.ships.push_back(scout);
                    emp.treasury -= 70;
                    break;
                }
            }
        }

        // Build Colony Ships when expansion is needed
        bool needsColonyShips = (colonyShips < 2 + (aiColonies / 2));
        if (needsColonyShips && (gGameState.currentTurn % 4 == 0) && emp.treasury > 160) {
            for (const auto& sys : gGameState.galaxy.systems) {
                if (sys.ownerEmpireId == emp.id) {
                    orion::Ship newShip;
                    newShip.id = static_cast<int>(gGameState.ships.size());
                    newShip.type = orion::ShipType::ColonyShip;
                    newShip.ownerId = emp.id;
                    newShip.locationSystemId = sys.starId;
                    newShip.name = "AI Colony Ship";
                    newShip.effectiveSpeed = 1.0f;
                    newShip.maxRange = 95.0f;
                    newShip.effectiveSpeed *= gGameState.technology.getShipSpeedMultiplier();
                    newShip.maxRange   += gGameState.technology.getShipRangeBonus();
                    gGameState.ships.push_back(newShip);
                    emp.treasury -= 140;
                    break;
                }
            }
        }

        // Build "military" ships once weapons tech is available ( groundwork for combat)
        if (hasWeaponsTech && militaryShips < 3 && (gGameState.currentTurn % 5 == 0) && emp.treasury > 200) {
            for (const auto& sys : gGameState.galaxy.systems) {
                if (sys.ownerEmpireId == emp.id) {
                    orion::Ship mil;
                    mil.id = static_cast<int>(gGameState.ships.size());
                    mil.type = orion::ShipType::Destroyer;  // Using Destroyer as military type
                    mil.ownerId = emp.id;
                    mil.locationSystemId = sys.starId;
                    mil.name = "AI Destroyer";
                    mil.effectiveSpeed = 1.15f;
                    mil.maxRange = 100.0f;
                    mil.effectiveSpeed *= gGameState.technology.getShipSpeedMultiplier();
                    mil.maxRange   += gGameState.technology.getShipRangeBonus();
                    // Basic combat stats for AI military ships (improves with tech over time)
                    mil.weaponPower = hasWeaponsTech ? 8 : 3;
                    mil.shieldStrength = hasWeaponsTech ? 6 : 2;
                    gGameState.ships.push_back(mil);
                    emp.treasury -= 180;
                    break;
                }
            }
        }

        // === Improved AI Research (Phase 2) ===
        // AI occasionally researches real techs from the tree, applying actual bonuses.
        if ((gGameState.currentTurn % 5 == 0) && emp.researchPool > 120) {
            // Find an unresearched tech the AI "would like"
            int chosen = -1;
            float bestScore = 0.0f;

            for (int i = 0; i < static_cast<int>(orion::TECH_TREE.size()); ++i) {
                if (gGameState.technology.hasTech(i)) continue;

                const auto& t = orion::TECH_TREE[i];
                float score = 50.0f;

                // Race-biased preferences
                if (emp.raceName == "Psilon" && t.category == orion::TechCategory::Computers) score += 80.0f;
                if (emp.raceName == "Psilon" && t.category == orion::TechCategory::Planetology) score += 40.0f;
                if (emp.raceName == "Mrrshan" && t.category == orion::TechCategory::Weapons) score += 90.0f;
                if (emp.raceName == "Silicoid" && t.category == orion::TechCategory::Construction) score += 70.0f;

                // Prefer cheaper techs early
                if (t.researchCost < 150) score += 30.0f;

                if (score > bestScore) {
                    bestScore = score;
                    chosen = i;
                }
            }

            if (chosen != -1) {
                gGameState.technology.researchedTechIndices.push_back(chosen);
                int cat = static_cast<int>(orion::TECH_TREE[chosen].category);
                gGameState.technology.level[cat]++;

                emp.researchPool -= 90;

                // Apply immediate known bonuses for the AI empire
                if (chosen == 2 || chosen == 3) { // Construction line
                    emp.productionMod = std::max(emp.productionMod, 1.18f);
                }
                if (chosen == 8 || chosen == 9) { // Propulsion
                    // AI ships will benefit on next build via global tech functions
                }
            }
        }
    }

    // Very simple trigger: if we have a decent research bank and no pending choice, offer some techs
    if (!gShowTechChoice && player.researchPool > 180 && gGameState.technology.researchedTechIndices.size() < 3) {
        gAvailableTechChoices.clear();
        // Pick 2-3 random unresearched techs
        for (int i = 0; i < static_cast<int>(orion::TECH_TREE.size()) && gAvailableTechChoices.size() < 3; ++i) {
            if (!gGameState.technology.hasTech(i)) {
                gAvailableTechChoices.push_back(i);
            }
        }
        if (!gAvailableTechChoices.empty()) {
            gShowTechChoice = true;
        }
    }

    // Always report current construction status so the user sees progress
    for (const auto& col : gGameState.colonies) {
        if (col.ownerId == 0 && col.projectCost > 0) {
            auto* colSys = gGameState.galaxy.findSystemById(col.planetId);
            std::string sysName = colSys ? colSys->name : "a colony";
            float pct = (col.projectProgress / col.projectCost) * 100.0f;
            gTurnReportMessages.push_back("Construction at " + sysName + ": " + col.currentProject + " (" + std::to_string((int)pct) + "%)");
        }
    }

    // === Always generate a useful End Turn Report ===
    int treasuryGained = player.treasury - startingTreasury;
    int researchGained = player.researchPool - startingResearch;

    // Always add a baseline summary
    gTurnReportMessages.push_back("Turn " + std::to_string(gGameState.currentTurn) + " completed.");

    if (treasuryGained != 0 || researchGained != 0) {
        gTurnReportMessages.push_back("Income this turn: +" + std::to_string(treasuryGained) + " BC, +" + std::to_string(researchGained) + " RP");
    } else {
        gTurnReportMessages.push_back("No income changes recorded this turn.");
    }

    // List active construction projects
    bool hadConstruction = false;
    for (const auto& col : gGameState.colonies) {
        if (col.ownerId == 0 && col.projectCost > 0) {
            if (!hadConstruction) {
                gTurnReportMessages.push_back("Active Projects:");
                hadConstruction = true;
            }
            auto* colSys = gGameState.galaxy.findSystemById(col.planetId);
            std::string sysName = colSys ? colSys->name : "Unknown";
            float pct = (col.projectProgress / col.projectCost) * 100.0f;
            gTurnReportMessages.push_back("  - " + sysName + ": " + col.currentProject + " (" + std::to_string((int)pct) + "%)");
        }
    }
    if (!hadConstruction) {
        gTurnReportMessages.push_back("No active construction projects this turn.");
    }

    // === Phase 3: Random Events ===
    // Chance to trigger an event each turn
    if (!gPossibleEvents.empty() && (rand() % 100) < 22) {  // ~22% chance per turn
        // Pick a random event
        int idx = rand() % gPossibleEvents.size();
        gCurrentEvent = gPossibleEvents[idx];
        gCurrentEvent.effect();
        gShowEventPopup = true;
    }

    // === Phase 3: Basic Victory Condition ===
    int playerColonies = 0;
    for (const auto& sys : gGameState.galaxy.systems) {
        if (sys.ownerEmpireId == 0) playerColonies++;
    }
    if (playerColonies >= 12 || gGameState.technology.researchedTechIndices.size() >= 10) {
        // Simple win condition popup (for now just log + disable further events)
        if (!gShowEventPopup) {
            gCurrentEvent.title = "Victory!";
            gCurrentEvent.description = "You have achieved dominance in the galaxy!";
            gCurrentEvent.isGood = true;
            gShowEventPopup = true;
            gPossibleEvents.clear(); // stop random events
        }
    }
}

// GUI simulation helper - extracted from the End Turn button logic in main.cpp
// Returns indices (into gGameState.colonies) of player-owned colonies that have no active project.
std::vector<int> getPlayerColoniesWithoutProduction() {
    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(gGameState.colonies.size()); ++i) {
        const auto& col = gGameState.colonies[i];
        if (col.ownerId == 0 && (col.currentProject == "None" || col.projectCost <= 0)) {
            result.push_back(i);
        }
    }
    return result;
}
