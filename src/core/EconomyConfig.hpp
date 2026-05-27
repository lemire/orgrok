#pragma once

#include <string>
#include <map>
#include <vector>
#include <simdjson.h>

namespace orion {

/**
 * Runtime representation of the economic model parameters loaded from data/economy.json.
 *
 * Usage example:
 *   auto econ = EconomyConfig::load("data/economy.json");
 *   float prod = (pop * econ.workerOutput) + (effectiveFactories * econ.factoryOutput);
 *
 * This keeps the economic simulation completely data-driven.
 */
struct EconomyConfig {
    // Core production parameters
    float workerOutput = 0.5f;
    float factoryOutput = 1.0f;
    int   baseFactoriesPerPop = 2;

    // Growth model (growth points system)
    float baseGrowthRate = 0.045f;
    float baseGrowthPointsCost = 100.0f;
    float growthCostMultiplier = 0.15f;

    // Richness multipliers
    std::map<std::string, float> planetRichnessProductionMultipliers;

    // Building effects (simple for now)
    std::map<std::string, float> buildingProductionBonuses;
    std::map<std::string, float> buildingGrowthBonuses;

    // Pollution
    float basePollutionPerFactory = 0.28f;

    // --- Utility functions ---

    [[nodiscard]] float getProductionMultiplierForRichness(const std::string& richness) const {
        auto it = planetRichnessProductionMultipliers.find(richness);
        return (it != planetRichnessProductionMultipliers.end()) ? it->second : 1.0f;
    }

    [[nodiscard]] int computeGrowthPointsNeeded(int currentPopulation) const {
        return static_cast<int>(baseGrowthPointsCost * (1.0f + growthCostMultiplier * currentPopulation));
    }

    /**
     * Loads the economy configuration from a JSON file using simdjson ondemand parser.
     */
    static EconomyConfig load(const std::string& filepath);
};

} // namespace orion