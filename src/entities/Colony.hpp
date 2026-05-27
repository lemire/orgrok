#pragma once

#include "core/Enums.hpp"
#include <cstdint>
#include <array>
#include <string>
#include <algorithm>

namespace orion {

// Classic Master of Orion colony allocation (the famous 5 sliders).
enum class ColonyFocus : uint8_t {
    ShipBuilding = 0,
    Defense,
    Industry,
    Ecology,
    Research,
    Count
};

struct Colony {
    int   planetId = -1;
    int   ownerId  = -1;

    float population = 1.0f;             // millions
    float maxPopulation = 6.0f;

    // Sliders removed per user request. Kept for save compatibility only.
    std::array<float, 5> allocation = {20.0f, 10.0f, 40.0f, 15.0f, 15.0f}; // ignored now

    // === Outputs from last turn (recalculated) ===
    float foodNet          = 0.0f;   // surplus after feeding pop (positive = growth fuel)
    float productionOutput = 0.0f;   // total industry produced this turn
    float researchOutput   = 0.0f;
    float pollution        = 0.0f;   // remaining pollution after ecology
    float netProduction    = 0.0f;   // available after ship/defense/ecology claims

    // Building queue (functional in Phase 2)
    std::string currentProject = "None";
    float       projectProgress = 0.0f;
    int         projectCost = 0;

    // Completed buildings that give permanent bonuses
    std::vector<std::string> completedBuildings;

    // Simple project catalog (cost in "industry units")
    static constexpr const char* PROJECTS[] = {
        "None",
        "Colony Ship (180)",
        "Missile Base (65)",
        "Factory (90)",
        "Outpost Ship (120)"
    };
    static constexpr int PROJECT_COSTS[] = {0, 180, 65, 90, 120};

    [[nodiscard]] float totalAllocation() const {
        float sum = 0.0f;
        for (float v : allocation) sum += v;
        return sum;
    }

    // Force allocations to exactly 100%
    void normalizeAllocations() {
        float sum = totalAllocation();
        if (sum <= 0.001f) {
            allocation = {15.0f, 5.0f, 40.0f, 20.0f, 20.0f};
            return;
        }
        for (float& v : allocation) v = (v / sum) * 100.0f;
    }

    // === Simplified formula-based economy (no sliders) ===
    // Industrial output is now determined entirely by a clear formula.
    // Call this every turn and live from the UI for preview.
    // techBonus and racialProdMod come from the caller.
    void recalculateOutputs(PlanetSize size, PlanetType type, Richness richness,
                            uint32_t traits, float planetMaxPop,
                            float techBonus = 1.0f, float racialProdMod = 1.0f) {

        const float pop = population;
        if (pop < 0.1f) {
            foodNet = productionOutput = researchOutput = pollution = netProduction = 0.0f;
            return;
        }

        // --- Food & Growth (simple and transparent) ---
        float baseFood = pop * 1.0f;
        if (type >= PlanetType::Ocean) baseFood += pop * 0.35f;
        if (traits & static_cast<uint32_t>(PlanetTrait::Fertile)) baseFood *= 1.3f;
        float consumption = pop * 0.85f;
        foodNet = baseFood - consumption;

        // --- Research (simple) ---
        float researchBonus = (traits & static_cast<uint32_t>(PlanetTrait::Artifacts)) ? 1.35f : 1.0f;
        researchOutput = pop * 0.55f * researchBonus;

        // --- Industry: Clean formula ---
        // Base from population
        float baseIndustry = pop * 0.8f;

        // Planet quality factors (transparent)
        float richFactor = 0.65f + (static_cast<int>(richness) - 1) * 0.28f;
        float sizeFactor = 0.8f + static_cast<int>(size) * 0.09f;
        float typeFactor = 1.0f;
        if (type == PlanetType::Gaia)      typeFactor = 1.6f;
        else if (type >= PlanetType::Ocean) typeFactor = 1.25f;
        else if (type <= PlanetType::Radiated) typeFactor = 0.6f;

        float planetQuality = richFactor * sizeFactor * typeFactor;

        float grossIndustry = baseIndustry * planetQuality;

        // Building bonuses
        float buildingBonus = 0.0f;
        for (const auto& b : completedBuildings) {
            if (b.find("Factory") != std::string::npos) buildingBonus += 0.13f;
        }
        grossIndustry *= (1.0f + buildingBonus);

        // External multipliers (tech + race) applied here for clarity
        grossIndustry *= techBonus;
        grossIndustry *= racialProdMod;

        // Simple maintenance / overhead (no more slider tax)
        float maintenance = grossIndustry * 0.22f;   // 22% always goes to basic upkeep & defense
        netProduction = std::max(0.0f, grossIndustry - maintenance);

        productionOutput = grossIndustry;

        // Simple pollution model (reduced by any completed ecology-style buildings)
        pollution = grossIndustry * 0.28f;
        float ecoClean = 0.0f;
        for (const auto& b : completedBuildings) {
            if (b.find("Ecology") != std::string::npos || b.find("Clean") != std::string::npos) ecoClean += 0.12f;
        }
        pollution = std::max(0.0f, pollution * (1.0f - ecoClean));
    }
};

} // namespace orion