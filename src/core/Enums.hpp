#pragma once

#include <cstdint>
#include <string_view>

namespace orion {

// === Master of Orion 1 faithful enumerations ===

enum class PlanetSize : uint8_t {
    Tiny = 0,
    Small,
    Medium,
    Large,
    Huge,
    Count
};

enum class PlanetType : uint8_t {
    Radiated = 0,
    Barren,
    Desert,
    Steppe,
    Arid,
    Swamp,
    Ocean,
    Terran,
    Gaia,
    Count
};

enum class Richness : uint8_t {
    UltraPoor = 0,
    Poor,
    Abundant,
    Rich,
    UltraRich,
    Count
};

enum class Gravity : uint8_t {
    Low = 0,
    Normal,
    Heavy,
    Count
};

// Special planet traits (can be combined in future)
enum class PlanetTrait : uint32_t {
    None        = 0,
    Fertile     = 1 << 0,   // +25% population growth
    Hostile     = 1 << 1,   // -25% max pop
    Artifacts   = 1 << 2,   // Research bonus
    GasGiant    = 1 << 3,   // Cannot be colonized (future)
};

constexpr std::string_view to_string(PlanetSize s) {
    switch (s) {
        case PlanetSize::Tiny:   return "Tiny";
        case PlanetSize::Small:  return "Small";
        case PlanetSize::Medium: return "Medium";
        case PlanetSize::Large:  return "Large";
        case PlanetSize::Huge:   return "Huge";
        default:                 return "?";
    }
}

constexpr std::string_view to_string(PlanetType t) {
    switch (t) {
        case PlanetType::Radiated: return "Radiated";
        case PlanetType::Barren:   return "Barren";
        case PlanetType::Desert:   return "Desert";
        case PlanetType::Steppe:   return "Steppe";
        case PlanetType::Arid:     return "Arid";
        case PlanetType::Swamp:    return "Swamp";
        case PlanetType::Ocean:    return "Ocean";
        case PlanetType::Terran:   return "Terran";
        case PlanetType::Gaia:     return "Gaia";
        default:                   return "?";
    }
}

constexpr std::string_view to_string(Richness r) {
    switch (r) {
        case Richness::UltraPoor: return "Ultra Poor";
        case Richness::Poor:      return "Poor";
        case Richness::Abundant:  return "Abundant";
        case Richness::Rich:      return "Rich";
        case Richness::UltraRich: return "Ultra Rich";
        default:                  return "?";
    }
}

constexpr std::string_view to_string(Gravity g) {
    switch (g) {
        case Gravity::Low:    return "Low";
        case Gravity::Normal: return "Normal";
        case Gravity::Heavy:  return "Heavy";
        default:              return "?";
    }
}

// Max population base (in millions) by size - classic MoO feel
constexpr int baseMaxPop(PlanetSize size) {
    switch (size) {
        case PlanetSize::Tiny:   return 2;
        case PlanetSize::Small:  return 4;
        case PlanetSize::Medium: return 6;
        case PlanetSize::Large:  return 9;
        case PlanetSize::Huge:   return 12;
        default:                 return 5;
    }
}

} // namespace orion