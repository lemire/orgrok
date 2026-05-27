#include "core/TechTree.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace orion {

std::string TechTree::categoryToString(Category cat) {
    switch (cat) {
        case Category::Biology:     return "biology";
        case Category::Construction: return "construction";
        case Category::Computers:   return "computers";
        case Category::Energy:      return "energy";
        case Category::Propulsion:  return "propulsion";
        case Category::Physics:     return "physics";
        case Category::Ecology:     return "ecology";
        case Category::Sociology:   return "sociology";
        default: return "unknown";
    }
}

TechTree::Category TechTree::stringToCategory(const std::string& str) {
    if (str == "biology")     return Category::Biology;
    if (str == "construction") return Category::Construction;
    if (str == "computers")   return Category::Computers;
    if (str == "energy")      return Category::Energy;
    if (str == "propulsion")  return Category::Propulsion;
    if (str == "physics")     return Category::Physics;
    if (str == "ecology")     return Category::Ecology;
    if (str == "sociology")   return Category::Sociology;
    return Category::Count;
}

const std::vector<TechLevel>& TechTree::getLevels(Category cat) const {
    static const std::vector<TechLevel> empty;
    auto it = categories.find(cat);
    return (it != categories.end()) ? it->second : empty;
}

const TechLevel* TechTree::getLevel(Category cat, int level) const {
    const auto& levels = getLevels(cat);
    for (const auto& tl : levels) {
        if (tl.level == level) return &tl;
    }
    return nullptr;
}

double TechTree::getTotalNumericEffect(Category cat, const std::string& effectName) const {
    double total = 0.0;
    const auto& levels = getLevels(cat);
    for (const auto& tl : levels) {
        auto it = tl.numericEffects.find(effectName);
        if (it != tl.numericEffects.end()) {
            total += it->second;
        }
    }
    return total;
}

TechTree TechTree::load(const std::string& filepath) {
    TechTree tree;

    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open tech tree file: " + filepath);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string jsonData(size, '\0');
    if (!file.read(&jsonData[0], size)) {
        throw std::runtime_error("Failed to read tech tree file: " + filepath);
    }

    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(jsonData);
    auto root = doc.get_object();

    if (auto cats = root["categories"]; !cats.error()) {
        auto catsObj = cats.get_object();

        for (auto field : catsObj) {
            std::string_view catName;
            if (field.unescaped_key().get(catName)) continue;

            Category cat = stringToCategory(std::string(catName));
            if (cat == Category::Count) continue;

            std::vector<TechLevel> levelsVec;

            auto levelsArr = field.value();
            if (levelsArr.type() == simdjson::ondemand::json_type::array) {
                for (auto levelVal : levelsArr.get_array()) {
                    auto levelObj = levelVal.get_object();
                    TechLevel tl;

                    int64_t ival;
                    if (auto v = levelObj["level"]; !v.error() && !v.get(ival)) tl.level = static_cast<int>(ival);

                    std::string_view sv;
                    if (auto v = levelObj["name"]; !v.error() && !v.get(sv)) tl.name = std::string(sv);
                    if (auto v = levelObj["description"]; !v.error() && !v.get(sv)) tl.description = std::string(sv);

                    if (auto v = levelObj["base_research_cost"]; !v.error() && !v.get(ival)) {
                        tl.baseResearchCost = static_cast<int>(ival);
                    }

                    // effects
                    if (auto eff = levelObj["effects"]; !eff.error()) {
                        auto effObj = eff.get_object();
                        for (auto efield : effObj) {
                            std::string_view key;
                            if (efield.unescaped_key().get(key)) continue;

                            auto val = efield.value();

                            if (val.type() == simdjson::ondemand::json_type::number) {
                                double d;
                                if (!val.get(d)) tl.numericEffects[std::string(key)] = d;
                            } else if (val.type() == simdjson::ondemand::json_type::string) {
                                std::string_view s;
                                if (!val.get(s)) tl.stringEffects[std::string(key)] = std::string(s);
                            } else if (val.type() == simdjson::ondemand::json_type::boolean) {
                                bool b;
                                if (!val.get(b)) tl.boolEffects[std::string(key)] = b;
                            }
                        }
                    }

                    // prerequisites
                    if (auto prereq = levelObj["prerequisites"]; !prereq.error()) {
                        auto prArr = prereq.get_array();
                        for (auto p : prArr) {
                            std::string_view ps;
                            if (!p.get(ps)) tl.prerequisites.emplace_back(ps);
                        }
                    }

                    levelsVec.push_back(std::move(tl));
                }
            }

            tree.categories[cat] = std::move(levelsVec);
        }
    }

    return tree;
}

} // namespace orion