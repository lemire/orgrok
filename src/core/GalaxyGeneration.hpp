#pragma once

#include "core/Galaxy.hpp"
#include "core/Enums.hpp"
#include <random>
#include <algorithm>
#include <string>
#include <vector>
#include <array>

namespace orion {

// === Name pools (Phase 1 - will be expanded / loaded from data) ===
inline const std::array<std::string, 32> kStarNamePool = {
    "Sol", "Alpha Centauri", "Sirius", "Vega", "Altair", "Deneb", "Rigel", "Betelgeuse",
    "Procyon", "Aldebaran", "Capella", "Arcturus", "Pollux", "Spica", "Antares", "Regulus",
    "Bellatrix", "Elnath", "Miaplacidus", "Alphard", "Gacrux", "Acrux", "Mimosa", "Atria",
    "Kaus Australis", "Vega Minor", "Thuban", "Rasalhague", "Kocab", "Dubhe", "Merak", "Pherkad"
};

// Weighted planet count distribution (MoO1 had many systems with 2-4 planets)
constexpr std::array<int, 5> kPlanetCountWeights = {8, 22, 35, 25, 10}; // index = num planets - 1

// Generate a single planet with reasonable MoO-like distribution
inline Planet generatePlanet(std::mt19937& rng, int indexInSystem) {
    Planet p;
    p.name = "Planet " + std::to_string(indexInSystem + 1); // Will be improved with real names

    // Size distribution (larger planets less common)
    std::uniform_int_distribution<int> sizeRoll(0, 100);
    int s = sizeRoll(rng);
    if (s < 12)      p.size = PlanetSize::Tiny;
    else if (s < 32) p.size = PlanetSize::Small;
    else if (s < 62) p.size = PlanetSize::Medium;
    else if (s < 85) p.size = PlanetSize::Large;
    else             p.size = PlanetSize::Huge;

    // Planet type - heavily biased toward better worlds for fun in Phase 1
    std::uniform_int_distribution<int> typeRoll(0, 100);
    int t = typeRoll(rng);
    if (t < 5)       p.type = PlanetType::Radiated;
    else if (t < 14) p.type = PlanetType::Barren;
    else if (t < 26) p.type = PlanetType::Desert;
    else if (t < 40) p.type = PlanetType::Steppe;
    else if (t < 55) p.type = PlanetType::Arid;
    else if (t < 68) p.type = PlanetType::Swamp;
    else if (t < 82) p.type = PlanetType::Ocean;
    else if (t < 96) p.type = PlanetType::Terran;
    else             p.type = PlanetType::Gaia;

    // Richness
    std::uniform_int_distribution<int> richRoll(0, 100);
    int r = richRoll(rng);
    if (r < 8)       p.richness = Richness::UltraPoor;
    else if (r < 22) p.richness = Richness::Poor;
    else if (r < 58) p.richness = Richness::Abundant;
    else if (r < 84) p.richness = Richness::Rich;
    else             p.richness = Richness::UltraRich;

    // Gravity correlates loosely with size
    std::uniform_int_distribution<int> gravRoll(0, 100);
    int g = gravRoll(rng);
    if (p.size == PlanetSize::Tiny || p.size == PlanetSize::Small) {
        p.gravity = (g < 55) ? Gravity::Low : Gravity::Normal;
    } else if (p.size == PlanetSize::Huge) {
        p.gravity = (g < 25) ? Gravity::Normal : Gravity::Heavy;
    } else {
        p.gravity = (g < 18) ? Gravity::Low : (g < 82 ? Gravity::Normal : Gravity::Heavy);
    }

    // Occasional nice traits
    std::bernoulli_distribution fertile(0.12);
    std::bernoulli_distribution artifacts(0.07);
    if (fertile(rng)) p.traits |= static_cast<uint32_t>(PlanetTrait::Fertile);
    if (artifacts(rng) && (p.type == PlanetType::Terran || p.type == PlanetType::Ocean)) {
        p.traits |= static_cast<uint32_t>(PlanetTrait::Artifacts);
    }

    p.maxPopulation = baseMaxPop(p.size);
    if (p.gravity == Gravity::Heavy) p.maxPopulation = static_cast<int>(p.maxPopulation * 0.85f);
    if (p.gravity == Gravity::Low)   p.maxPopulation = static_cast<int>(p.maxPopulation * 1.10f);

    return p;
}

// Main galaxy generator - faithful starting point for MoO1-style maps
inline Galaxy generateGalaxy(int desiredStars, int seed) {
    Galaxy gal;
    gal.seed = seed;
    gal.width = 1100;
    gal.height = 820;

    std::mt19937 rng(seed);

    // Choose actual star count with some variance
    std::uniform_int_distribution<int> starCountVar(-6, 9);
    int starCount = std::clamp(desiredStars + starCountVar(rng), 28, 95);
    gal.systems.reserve(starCount);

    // Simple rejection sampling placement (minimum distance between stars)
    const float minDist = 38.0f;
    const float margin = 55.0f;

    std::uniform_real_distribution<float> xDist(margin, gal.width - margin);
    std::uniform_real_distribution<float> yDist(margin, gal.height - margin);

    int attempts = 0;
    while (gal.systems.size() < static_cast<size_t>(starCount) && attempts < 20000) {
        ++attempts;

        Vector2 candidate{xDist(rng), yDist(rng)};
        bool ok = true;
        for (const auto& existing : gal.systems) {
            float dx = candidate.x - existing.position.x;
            float dy = candidate.y - existing.position.y;
            if (dx * dx + dy * dy < minDist * minDist) {
                ok = false;
                break;
            }
        }
        if (!ok) continue;

        StarSystem sys;
        sys.position = candidate;
        sys.starId = static_cast<int>(gal.systems.size());

        // Pick name
        if (sys.starId < static_cast<int>(kStarNamePool.size())) {
            sys.name = kStarNamePool[sys.starId];
        } else {
            sys.name = "System " + std::to_string(sys.starId + 1);
        }

        // Planet count for this system
        std::discrete_distribution<int> planetCountDist(kPlanetCountWeights.begin(), kPlanetCountWeights.end());
        int numPlanets = planetCountDist(rng) + 1;

        sys.planets.reserve(numPlanets);
        for (int i = 0; i < numPlanets; ++i) {
            Planet pl = generatePlanet(rng, i);
            // Improve planet name
            pl.name = sys.name + " " + std::to_string(i + 1);
            sys.planets.push_back(std::move(pl));
        }

        // === Assign rare special status to a few systems (flavor + strategic variety) ===
        // ~18% chance per system yields ~8-12 specials in a typical 55-star galaxy.
        std::bernoulli_distribution specialRoll(0.18);
        if (specialRoll(rng)) {
            std::uniform_int_distribution<int> specPick(1, 12); // skip None
            int pick = specPick(rng);
            sys.specialStatus = static_cast<SystemSpecial>(pick);
        }

        gal.systems.push_back(std::move(sys));
    }

    return gal;
}

// Helper: create a minimal starting empire (player)
inline Empire createPlayerEmpire(const std::string& race = "Human") {
    Empire e;
    e.id = 0;
    e.treasury = 1250;
    e.researchPool = 65;
    e.isPlayer = true;

    if (race == "Psilon") {
        e.name = "Psilon League";
        e.raceName = "Psilon";
        e.researchMod = 1.40f;          // Excellent researchers
        e.populationGrowthMod = 0.85f;
    } else if (race == "Mrrshan") {
        e.name = "Mrrshan Empire";
        e.raceName = "Mrrshan";
        e.shipCombatMod = 1.30f;        // Warriors
        e.populationGrowthMod = 1.10f;
    } else if (race == "Silicoid") {
        e.name = "Silicoid Imperium";
        e.raceName = "Silicoid";
        e.productionMod = 1.25f;        // Tough industrialists, slow pop
        e.populationGrowthMod = 0.80f;
        e.researchMod = 0.90f;
    } else {
        e.name = "Human Republic";
        e.raceName = "Human";
        e.researchMod = 1.05f;          // Balanced
        e.populationGrowthMod = 1.10f;
    }
    return e;
}

} // namespace orion