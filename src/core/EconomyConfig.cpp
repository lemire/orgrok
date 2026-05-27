#include "core/EconomyConfig.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace orion {

EconomyConfig EconomyConfig::load(const std::string& filepath) {
    EconomyConfig config;

    // Read entire file
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open economy config file: " + filepath);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string jsonData(size, '\0');
    if (!file.read(&jsonData[0], size)) {
        throw std::runtime_error("Failed to read economy config file: " + filepath);
    }

    // simdjson ondemand parsing
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(jsonData);

    auto root = doc.get_object();

    // base_parameters
    if (auto bp = root["base_parameters"]; !bp.error()) {
        auto obj = bp.get_object();
        double val;
        if (auto v = obj["worker_output"]; !v.error() && !v.get(val)) config.workerOutput = static_cast<float>(val);
        if (auto v = obj["factory_output"]; !v.error() && !v.get(val)) config.factoryOutput = static_cast<float>(val);
        int64_t ival;
        if (auto v = obj["base_factories_per_pop"]; !v.error() && !v.get(ival)) config.baseFactoriesPerPop = static_cast<int>(ival);
        if (auto v = obj["base_growth_rate"]; !v.error() && !v.get(val)) config.baseGrowthRate = static_cast<float>(val);
        if (auto v = obj["base_growth_points_cost"]; !v.error() && !v.get(val)) config.baseGrowthPointsCost = static_cast<float>(val);
        if (auto v = obj["growth_cost_multiplier"]; !v.error() && !v.get(val)) config.growthCostMultiplier = static_cast<float>(val);
    }

    // modifiers.planet_richness
    if (auto mods = root["modifiers"]; !mods.error()) {
        auto modObj = mods.get_object();
        if (auto pr = modObj["planet_richness"]; !pr.error()) {
            auto prObj = pr.get_object();
            for (auto field : prObj) {
                std::string_view key;
                if (field.unescaped_key().get(key)) continue;

                auto valObj = field.value();
                double mult = 1.0;
                if (auto m = valObj["production_multiplier"]; !m.error() && !m.get(mult)) {
                    config.planetRichnessProductionMultipliers[std::string(key)] = static_cast<float>(mult);
                }
            }
        }

        // buildings
        if (auto bld = modObj["buildings"]; !bld.error()) {
            auto bldObj = bld.get_object();
            for (auto field : bldObj) {
                std::string_view key;
                if (field.unescaped_key().get(key)) continue;

                auto valObj = field.value();
                double prod = 0.0;
                if (auto p = valObj["production_bonus"]; !p.error() && !p.get(prod)) {
                    config.buildingProductionBonuses[std::string(key)] = static_cast<float>(prod);
                }
                double growth = 0.0;
                if (auto g = valObj["growth_bonus"]; !g.error() && !g.get(growth)) {
                    config.buildingGrowthBonuses[std::string(key)] = static_cast<float>(growth);
                }
            }
        }
    }

    // pollution
    if (auto pol = root["pollution"]; !pol.error()) {
        auto polObj = pol.get_object();
        double val;
        if (auto v = polObj["base_pollution_per_factory"]; !v.error() && !v.get(val)) {
            config.basePollutionPerFactory = static_cast<float>(val);
        }
    }

    return config;
}

} // namespace orion