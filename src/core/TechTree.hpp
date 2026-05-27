#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <simdjson.h>

namespace orion {

/**
 * Full data-driven tech tree loaded from data/tech_tree.json.
 *
 * Usage:
 *   auto tree = TechTree::load("data/tech_tree.json");
 *   auto* level = tree.getLevel(TechTree::Category::Construction, 5);
 *
 * Effects are stored in flexible maps so gameplay code can interpret
 * numeric bonuses, unlock strings ("unlocks_building"), and boolean flags.
 */
struct TechLevel {
    int level = 0;
    std::string name;
    std::string description;
    int baseResearchCost = 0;

    // Numeric effects (most common): e.g. "growth_multiplier" -> 1.25, "factories_per_pop" -> 3
    std::map<std::string, double> numericEffects;

    // String effects for unlocks: e.g. "unlocks_building" -> "Cloning Center"
    std::map<std::string, std::string> stringEffects;

    // Boolean flags (true/false values in JSON)
    std::map<std::string, bool> boolEffects;

    // List of prerequisite tech IDs in the form "category.level" (e.g. "biology.2")
    std::vector<std::string> prerequisites;
};

/**
 * Full data-driven tech tree loaded from data/tech_tree.json.
 */
class TechTree {
public:
    enum class Category {
        Biology,
        Construction,
        Computers,
        Energy,
        Propulsion,
        Physics,
        Ecology,
        Sociology,
        Count
    };

    static std::string categoryToString(Category cat);
    static Category stringToCategory(const std::string& str);

    // Returns all levels for a given category (0-based or 1-based as defined in JSON)
    const std::vector<TechLevel>& getLevels(Category cat) const;

    // Convenience lookup
    const TechLevel* getLevel(Category cat, int level) const;

    /**
     * Returns the sum of a numeric effect across all levels in a category
     * (simple way to accumulate bonuses like factories_per_pop or growth).
     */
    double getTotalNumericEffect(Category cat, const std::string& effectName) const;

    /**
     * Loads the complete tech tree from a JSON file.
     */
    static TechTree load(const std::string& filepath);

private:
    std::unordered_map<Category, std::vector<TechLevel>> categories;
};

} // namespace orion