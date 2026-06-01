#include "core/SaveLoad.hpp"
#include "entities/ShipDesign.hpp"

#include <simdjson.h>
#include <fstream>
#include <iostream>
#include <string>

namespace orion {

// === Serialization using simdjson builder API (as documented in builder.md) ===

static void write_planet(const Planet& p, simdjson::builder::string_builder& sb) {
    sb.start_object();
    sb.append_key_value("name", p.name);
    sb.append_comma();
    sb.append_key_value("size", static_cast<int64_t>(p.size));
    sb.append_comma();
    sb.append_key_value("type", static_cast<int64_t>(p.type));
    sb.append_comma();
    sb.append_key_value("richness", static_cast<int64_t>(p.richness));
    sb.append_comma();
    sb.append_key_value("gravity", static_cast<int64_t>(p.gravity));
    sb.append_comma();
    sb.append_key_value("traits", static_cast<int64_t>(p.traits));
    sb.append_comma();
    sb.append_key_value("ownerEmpireId", static_cast<int64_t>(p.ownerEmpireId));
    sb.append_comma();
    sb.append_key_value("population", p.population);
    sb.append_comma();
    sb.append_key_value("maxPopulation", static_cast<int64_t>(p.maxPopulation));
    sb.end_object();
}

static void write_star_system(const StarSystem& s, simdjson::builder::string_builder& sb) {
    sb.start_object();
    sb.append_key_value("name", s.name);
    sb.append_comma();

    sb.escape_and_append_with_quotes("position");
    sb.append_colon();
    sb.start_array();
    sb.append(s.position.x);
    sb.append_comma();
    sb.append(s.position.y);
    sb.end_array();
    sb.append_comma();

    sb.escape_and_append_with_quotes("planets");
    sb.append_colon();
    sb.start_array();
    for (size_t i = 0; i < s.planets.size(); ++i) {
        write_planet(s.planets[i], sb);
        if (i + 1 < s.planets.size()) sb.append_comma();
    }
    sb.end_array();
    sb.append_comma();

    sb.append_key_value("ownerEmpireId", static_cast<int64_t>(s.ownerEmpireId));
    sb.append_comma();
    sb.append_key_value("starId", static_cast<int64_t>(s.starId));
    sb.append_comma();
    sb.append_key_value("specialStatus", static_cast<int64_t>(static_cast<uint8_t>(s.specialStatus)));
    sb.end_object();
}

static void write_galaxy(const Galaxy& g, simdjson::builder::string_builder& sb) {
    sb.start_object();
    sb.append_key_value("seed", static_cast<int64_t>(g.seed));
    sb.append_comma();
    sb.append_key_value("width", static_cast<int64_t>(g.width));
    sb.append_comma();
    sb.append_key_value("height", static_cast<int64_t>(g.height));
    sb.append_comma();

    sb.escape_and_append_with_quotes("systems");
    sb.append_colon();
    sb.start_array();
    for (size_t i = 0; i < g.systems.size(); ++i) {
        write_star_system(g.systems[i], sb);
        if (i + 1 < g.systems.size()) sb.append_comma();
    }
    sb.end_array();
    sb.end_object();
}

static void write_empire(const Empire& e, simdjson::builder::string_builder& sb) {
    sb.start_object();
    sb.append_key_value("id", static_cast<int64_t>(e.id));
    sb.append_comma();
    sb.append_key_value("name", e.name);
    sb.append_comma();
    sb.append_key_value("raceName", e.raceName);
    sb.append_comma();
    sb.append_key_value("treasury", static_cast<int64_t>(e.treasury));
    sb.append_comma();
    sb.append_key_value("researchPool", static_cast<int64_t>(e.researchPool));
    sb.append_comma();
    sb.append_key_value("isPlayer", e.isPlayer);
    sb.append_comma();
    sb.append_key_value("populationGrowthMod", e.populationGrowthMod);
    sb.append_comma();
    sb.append_key_value("researchMod", e.researchMod);
    sb.append_comma();
    sb.append_key_value("shipCombatMod", e.shipCombatMod);
    sb.append_comma();
    sb.append_key_value("productionMod", e.productionMod);
    sb.append_comma();

    sb.escape_and_append_with_quotes("ownedStarIds");
    sb.append_colon();
    sb.start_array();
    for (size_t i = 0; i < e.ownedStarIds.size(); ++i) {
        sb.append(static_cast<int64_t>(e.ownedStarIds[i]));
        if (i + 1 < e.ownedStarIds.size()) sb.append_comma();
    }
    sb.end_array();
    sb.end_object();
}

static void write_colony(const Colony& c, simdjson::builder::string_builder& sb) {
    sb.start_object();
    sb.append_key_value("planetId", static_cast<int64_t>(c.planetId));
    sb.append_comma();
    sb.append_key_value("ownerId", static_cast<int64_t>(c.ownerId));
    sb.append_comma();
    sb.append_key_value("population", c.population);
    sb.append_comma();
    sb.append_key_value("maxPopulation", c.maxPopulation);
    sb.append_comma();

    sb.escape_and_append_with_quotes("allocation");
    sb.append_colon();
    sb.start_array();
    for (size_t i = 0; i < c.allocation.size(); ++i) {
        sb.append(c.allocation[i]);
        if (i + 1 < c.allocation.size()) sb.append_comma();
    }
    sb.end_array();
    sb.append_comma();

    sb.escape_and_append_with_quotes("completedBuildings");
    sb.append_colon();
    sb.start_array();
    for (size_t i = 0; i < c.completedBuildings.size(); ++i) {
        sb.append(c.completedBuildings[i]);
        if (i + 1 < c.completedBuildings.size()) sb.append_comma();
    }
    sb.end_array();
    sb.append_comma();

    sb.append_key_value("currentProject", c.currentProject);
    sb.append_comma();
    sb.append_key_value("projectProgress", c.projectProgress);
    sb.append_comma();
    sb.append_key_value("projectCost", static_cast<int64_t>(c.projectCost));
    sb.end_object();
}

static void write_ship(const Ship& s, simdjson::builder::string_builder& sb) {
    sb.start_object();
    sb.append_key_value("id", static_cast<int64_t>(s.id));
    sb.append_comma();
    sb.append_key_value("type", static_cast<int64_t>(s.type));
    sb.append_comma();
    sb.append_key_value("ownerId", static_cast<int64_t>(s.ownerId));
    sb.append_comma();
    sb.append_key_value("locationSystemId", static_cast<int64_t>(s.locationSystemId));
    sb.append_comma();
    sb.append_key_value("destinationSystemId", static_cast<int64_t>(s.destinationSystemId));
    sb.append_comma();
    sb.append_key_value("travelProgress", s.travelProgress);
    sb.append_comma();
    sb.append_key_value("isMoving", s.isMoving);
    sb.append_comma();
    sb.append_key_value("name", s.name);
    sb.append_comma();
    sb.append_key_value("designName", s.designName);
    sb.append_comma();
    sb.append_key_value("effectiveSpeed", s.effectiveSpeed);
    sb.append_comma();
    sb.append_key_value("maxRange", s.maxRange);
    sb.append_comma();
    sb.append_key_value("weaponPower", static_cast<int64_t>(s.weaponPower));
    sb.append_comma();
    sb.append_key_value("shieldStrength", static_cast<int64_t>(s.shieldStrength));
    sb.append_comma();
    sb.append_key_value("primaryWeapon", static_cast<int64_t>(s.primaryWeapon));
    sb.end_object();
}

static void write_design(const ShipDesign& d, simdjson::builder::string_builder& sb) {
    sb.start_object();
    sb.append_key_value("name", d.name);
    sb.append_comma();
    sb.append_key_value("hull", static_cast<int64_t>(d.hull));
    sb.append_comma();

    sb.escape_and_append_with_quotes("componentIndices");
    sb.append_colon();
    sb.start_array();
    for (size_t i = 0; i < d.componentIndices.size(); ++i) {
        sb.append(static_cast<int64_t>(d.componentIndices[i]));
        if (i + 1 < d.componentIndices.size()) sb.append_comma();
    }
    sb.end_array();
    sb.end_object();
}

static void write_tech(const TechnologyState& t, simdjson::builder::string_builder& sb) {
    sb.start_object();

    sb.escape_and_append_with_quotes("level");
    sb.append_colon();
    sb.start_array();
    for (size_t i = 0; i < t.level.size(); ++i) {
        sb.append(static_cast<int64_t>(t.level[i]));
        if (i + 1 < t.level.size()) sb.append_comma();
    }
    sb.end_array();
    sb.append_comma();

    sb.escape_and_append_with_quotes("researchedTechIndices");
    sb.append_colon();
    sb.start_array();
    for (size_t i = 0; i < t.researchedTechIndices.size(); ++i) {
        sb.append(static_cast<int64_t>(t.researchedTechIndices[i]));
        if (i + 1 < t.researchedTechIndices.size()) sb.append_comma();
    }
    sb.end_array();
    sb.end_object();
}

static void serialize_game_state(const GameState& gs, simdjson::builder::string_builder& sb) {
    sb.start_object();
    sb.append_key_value("currentTurn", static_cast<int64_t>(gs.currentTurn));
    sb.append_comma();
    sb.append_key_value("selectedStarId", static_cast<int64_t>(gs.selectedStarId));
    sb.append_comma();

    sb.escape_and_append_with_quotes("galaxy");
    sb.append_colon();
    write_galaxy(gs.galaxy, sb);
    sb.append_comma();

    sb.escape_and_append_with_quotes("empires");
    sb.append_colon();
    sb.start_array();
    for (size_t i = 0; i < gs.empires.size(); ++i) {
        write_empire(gs.empires[i], sb);
        if (i + 1 < gs.empires.size()) sb.append_comma();
    }
    sb.end_array();
    sb.append_comma();

    sb.escape_and_append_with_quotes("colonies");
    sb.append_colon();
    sb.start_array();
    for (size_t i = 0; i < gs.colonies.size(); ++i) {
        write_colony(gs.colonies[i], sb);
        if (i + 1 < gs.colonies.size()) sb.append_comma();
    }
    sb.end_array();
    sb.append_comma();

    sb.escape_and_append_with_quotes("ships");
    sb.append_colon();
    sb.start_array();
    for (size_t i = 0; i < gs.ships.size(); ++i) {
        write_ship(gs.ships[i], sb);
        if (i + 1 < gs.ships.size()) sb.append_comma();
    }
    sb.end_array();
    sb.append_comma();

    sb.escape_and_append_with_quotes("technology");
    sb.append_colon();
    write_tech(gs.technology, sb);
    sb.append_comma();

    sb.escape_and_append_with_quotes("playerDesigns");
    sb.append_colon();
    sb.start_array();
    for (size_t i = 0; i < playerDesigns.size(); ++i) {
        write_design(playerDesigns[i], sb);
        if (i + 1 < playerDesigns.size()) sb.append_comma();
    }
    sb.end_array();
    sb.end_object();
}

// === Loading using simdjson ondemand ===

static Planet read_planet(simdjson::ondemand::object obj) {
    Planet p;
    std::string_view sv;
    if (!obj["name"].get(sv)) p.name = std::string(sv);
    int64_t tmp;
    if (!obj["size"].get(tmp)) p.size = static_cast<PlanetSize>(tmp);
    if (!obj["type"].get(tmp)) p.type = static_cast<PlanetType>(tmp);
    if (!obj["richness"].get(tmp)) p.richness = static_cast<Richness>(tmp);
    if (!obj["gravity"].get(tmp)) p.gravity = static_cast<Gravity>(tmp);
    if (!obj["traits"].get(tmp)) p.traits = static_cast<uint32_t>(tmp);
    if (!obj["ownerEmpireId"].get(tmp)) p.ownerEmpireId = static_cast<int>(tmp);
    double d;
    if (!obj["population"].get(d)) p.population = static_cast<float>(d);
    if (!obj["maxPopulation"].get(tmp)) p.maxPopulation = static_cast<int>(tmp);
    return p;
}

static StarSystem read_star_system(simdjson::ondemand::object obj) {
    StarSystem s;
    std::string_view sv;
    if (!obj["name"].get(sv)) s.name = std::string(sv);
    int64_t tmp;
    if (!obj["ownerEmpireId"].get(tmp)) s.ownerEmpireId = static_cast<int>(tmp);
    if (!obj["starId"].get(tmp)) s.starId = static_cast<int>(tmp);
    if (!obj["specialStatus"].get(tmp)) s.specialStatus = static_cast<SystemSpecial>(tmp);

    // position array [x, y]
    simdjson::ondemand::array posArr;
    if (!obj["position"].get(posArr)) {
        size_t idx = 0;
        for (auto v : posArr) {
            double d; if (!v.get(d)) {
                if (idx == 0) s.position.x = static_cast<float>(d);
                else if (idx == 1) s.position.y = static_cast<float>(d);
            }
            ++idx;
        }
    }

    simdjson::ondemand::array planetsArr;
    if (!obj["planets"].get(planetsArr)) {
        for (auto pv : planetsArr) {
            auto po = pv.get_object().value();
            s.planets.push_back(read_planet(po));
        }
    }
    return s;
}

static Empire read_empire(simdjson::ondemand::object obj) {
    Empire e;
    int64_t tmp;
    if (!obj["id"].get(tmp)) e.id = static_cast<int>(tmp);
    std::string_view sv;
    if (!obj["name"].get(sv)) e.name = std::string(sv);
    if (!obj["raceName"].get(sv)) e.raceName = std::string(sv);
    if (!obj["treasury"].get(tmp)) e.treasury = static_cast<int>(tmp);
    if (!obj["researchPool"].get(tmp)) e.researchPool = static_cast<int>(tmp);
    bool b; if (!obj["isPlayer"].get(b)) e.isPlayer = b;
    double d;
    if (!obj["populationGrowthMod"].get(d)) e.populationGrowthMod = static_cast<float>(d);
    if (!obj["researchMod"].get(d)) e.researchMod = static_cast<float>(d);
    if (!obj["shipCombatMod"].get(d)) e.shipCombatMod = static_cast<float>(d);
    if (!obj["productionMod"].get(d)) e.productionMod = static_cast<float>(d);

    simdjson::ondemand::array owned;
    if (!obj["ownedStarIds"].get(owned)) {
        for (auto v : owned) {
            int64_t id; if (!v.get(id)) e.ownedStarIds.push_back(static_cast<int>(id));
        }
    }
    return e;
}

static Colony read_colony(simdjson::ondemand::object obj) {
    Colony c;
    int64_t tmp;
    if (!obj["planetId"].get(tmp)) c.planetId = static_cast<int>(tmp);
    if (!obj["ownerId"].get(tmp)) c.ownerId = static_cast<int>(tmp);
    double d;
    if (!obj["population"].get(d)) c.population = static_cast<float>(d);
    if (!obj["maxPopulation"].get(d)) c.maxPopulation = static_cast<float>(d);

    simdjson::ondemand::array allocArr;
    if (!obj["allocation"].get(allocArr)) {
        size_t i = 0;
        for (auto v : allocArr) {
            double val; if (!v.get(val) && i < 5) c.allocation[i++] = static_cast<float>(val);
        }
    }

    simdjson::ondemand::array bld;
    if (!obj["completedBuildings"].get(bld)) {
        for (auto v : bld) {
            std::string_view sv; if (!v.get(sv)) c.completedBuildings.emplace_back(sv);
        }
    }

    std::string_view sv;
    if (!obj["currentProject"].get(sv)) c.currentProject = std::string(sv);
    if (!obj["projectProgress"].get(d)) c.projectProgress = static_cast<float>(d);
    if (!obj["projectCost"].get(tmp)) c.projectCost = static_cast<int>(tmp);
    return c;
}

static Ship read_ship(simdjson::ondemand::object obj) {
    Ship s;
    int64_t tmp;
    if (!obj["id"].get(tmp)) s.id = static_cast<int>(tmp);
    if (!obj["type"].get(tmp)) s.type = static_cast<ShipType>(tmp);
    if (!obj["ownerId"].get(tmp)) s.ownerId = static_cast<int>(tmp);
    if (!obj["locationSystemId"].get(tmp)) s.locationSystemId = static_cast<int>(tmp);
    if (!obj["destinationSystemId"].get(tmp)) s.destinationSystemId = static_cast<int>(tmp);
    double d;
    if (!obj["travelProgress"].get(d)) s.travelProgress = static_cast<float>(d);
    bool b; if (!obj["isMoving"].get(b)) s.isMoving = b;
    std::string_view sv;
    if (!obj["name"].get(sv)) s.name = std::string(sv);
    if (!obj["designName"].get(sv)) s.designName = std::string(sv);
    double d2;
    if (!obj["effectiveSpeed"].get(d2)) s.effectiveSpeed = static_cast<float>(d2);
    if (!obj["maxRange"].get(d2)) s.maxRange = static_cast<float>(d2);
    int64_t i64;
    if (!obj["weaponPower"].get(i64)) s.weaponPower = static_cast<int>(i64);
    if (!obj["shieldStrength"].get(i64)) s.shieldStrength = static_cast<int>(i64);
    if (!obj["primaryWeapon"].get(i64)) s.primaryWeapon = static_cast<orion::WeaponType>(i64);
    return s;
}

static ShipDesign read_design(simdjson::ondemand::object obj) {
    ShipDesign d;
    std::string_view sv;
    if (!obj["name"].get(sv)) d.name = std::string(sv);
    int64_t tmp;
    if (!obj["hull"].get(tmp)) d.hull = static_cast<HullSize>(tmp);

    simdjson::ondemand::array comps;
    if (!obj["componentIndices"].get(comps)) {
        for (auto v : comps) {
            int64_t idx; if (!v.get(idx)) d.componentIndices.push_back(static_cast<int>(idx));
        }
    }
    d.recalculateStats();
    return d;
}

static TechnologyState read_tech(simdjson::ondemand::object obj) {
    TechnologyState t;
    simdjson::ondemand::array lvl;
    if (!obj["level"].get(lvl)) {
        size_t i = 0;
        for (auto v : lvl) {
            int64_t val; if (!v.get(val) && i < t.level.size()) t.level[i++] = static_cast<int>(val);
        }
    }
    simdjson::ondemand::array res;
    if (!obj["researchedTechIndices"].get(res)) {
        for (auto v : res) {
            int64_t idx; if (!v.get(idx)) t.researchedTechIndices.push_back(static_cast<int>(idx));
        }
    }
    return t;
}

bool loadGame(GameState& state, const std::string& filepath) {
    try {
        simdjson::ondemand::parser parser;
        simdjson::padded_string json;
        auto err = simdjson::padded_string::load(filepath).get(json);
        if (err) {
            std::cerr << "Failed to read save: " << simdjson::error_message(err) << std::endl;
            return false;
        }

        simdjson::ondemand::document doc;
        err = parser.iterate(json).get(doc);
        if (err) return false;

        auto root = doc.get_object().value();

        int64_t tmp;
        if (!root["currentTurn"].get(tmp)) state.currentTurn = static_cast<int>(tmp);
        if (!root["selectedStarId"].get(tmp)) state.selectedStarId = static_cast<int>(tmp);

        // Galaxy
        auto galObj = root["galaxy"].get_object().value();
        int64_t seed; if (!galObj["seed"].get(seed)) state.galaxy.seed = static_cast<uint32_t>(seed);
        int64_t w; if (!galObj["width"].get(w)) state.galaxy.width = static_cast<int>(w);
        int64_t h; if (!galObj["height"].get(h)) state.galaxy.height = static_cast<int>(h);

        state.galaxy.systems.clear();
        simdjson::ondemand::array sysArr;
        if (!galObj["systems"].get(sysArr)) {
            for (auto sv : sysArr) {
                auto so = sv.get_object().value();
                state.galaxy.systems.push_back(read_star_system(so));
            }
        }

        // Empires
        state.empires.clear();
        simdjson::ondemand::array empArr;
        if (!root["empires"].get(empArr)) {
            for (auto ev : empArr) {
                auto eo = ev.get_object().value();
                state.empires.push_back(read_empire(eo));
            }
        }

        // Colonies
        state.colonies.clear();
        simdjson::ondemand::array colArr;
        if (!root["colonies"].get(colArr)) {
            for (auto cv : colArr) {
                auto co = cv.get_object().value();
                state.colonies.push_back(read_colony(co));
            }
        }

        // Ships
        state.ships.clear();
        simdjson::ondemand::array shipArr;
        if (!root["ships"].get(shipArr)) {
            for (auto sv : shipArr) {
                auto so = sv.get_object().value();
                state.ships.push_back(read_ship(so));
            }
        }

        // Technology
        auto techObj = root["technology"].get_object().value();
        state.technology = read_tech(techObj);

        // Player designs (global)
        playerDesigns.clear();
        simdjson::ondemand::array desArr;
        if (!root["playerDesigns"].get(desArr)) {
            for (auto dv : desArr) {
                auto dObj = dv.get_object().value();
                playerDesigns.push_back(read_design(dObj));
            }
        }

        std::cout << "Game loaded using simdjson ondemand (full Phase 2 state).\n";

        // Enforce rule on load too: never show specials on systems that already have an owner
        // (covers old saves, edited saves, or future cases where homes get assigned post-gen).
        for (auto& sys : state.galaxy.systems) {
            if (sys.ownerEmpireId >= 0) {
                sys.specialStatus = SystemSpecial::None;
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Load failed: " << e.what() << std::endl;
        return false;
    }
}

bool saveGame(const GameState& state, const std::string& filepath) {
    try {
        simdjson::builder::string_builder sb;
        serialize_game_state(state, sb);

        std::ofstream f(filepath);
        if (!f.is_open()) return false;
        f << std::string_view(sb);
        std::cout << "Game saved using simdjson::builder::string_builder.\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save failed: " << e.what() << std::endl;
        return false;
    }
}

} // namespace orion