#pragma once

#include <string>
#include <cstdint>

namespace orion {

// Very early ship concept for colonization gameplay
enum class ShipType : uint8_t {
    ColonyShip = 0,
    Scout,
    Destroyer,
    // more later
};

enum class WeaponType : uint8_t {
    None = 0,
    Laser,
    MassDriver,
    ParticleCannon
};

struct Ship {
    int         id = -1;
    ShipType    type = ShipType::ColonyShip;
    int         ownerId = 0;

    int         locationSystemId = -1;   // current star system
    int         destinationSystemId = -1;

    float       travelProgress = 0.0f;   // 0.0 to 1.0
    bool        isMoving = false;

    std::string name = "Colony Ship";

    // Phase 2: Link to a saved design (empty = generic / not from designer)
    std::string designName = "";

    // Travel characteristics (computed at spawn from design + tech at time of construction)
    float effectiveSpeed = 1.0f;   // parsecs per turn (base, before global tech)
    float maxRange       = 100.0f; // maximum jump distance in parsecs

    // Combat characteristics (Phase 2 groundwork)
    int weaponPower = 0;
    int shieldStrength = 0;
    WeaponType primaryWeapon = WeaponType::None;

    [[nodiscard]] bool hasArrived() const {
        return !isMoving || travelProgress >= 1.0f;
    }

    [[nodiscard]] bool isFromDesign() const {
        return !designName.empty();
    }
};

} // namespace orion