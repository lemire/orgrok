#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"

#ifdef IMGUI_ENABLE_FREETYPE
#include "misc/freetype/imgui_freetype.h"
#endif

// Game core (Phase 1)
#include "core/GameState.hpp"
#include "core/GalaxyGeneration.hpp"
#include "core/Enums.hpp"
#include "entities/Ship.hpp"
#include "entities/ShipDesign.hpp"
#include "core/Technology.hpp"
#include "core/SaveLoad.hpp"
#include "core/GameSimulation.hpp"

#include <algorithm>
#include <cmath>
#include <vector>
#include <random>
#include <string>
#include <cstdint>
#include <cstdio>
#include <format>
#include <map>
#include <functional>
#include <iostream>
#include <filesystem>

// === Phase 1 Game State (real MoO-like data) ===
// non-static so it matches the extern declaration in core/GameSimulation.hpp (required for orion_core linkage)
orion::GameState gGameState;
static bool gShowDebugPanel = true;
static float gZoom = 1.45f;  // Fixed zoom level (big enough to clearly see ownership flags)
static Vector2 gCameraOffset = {0.0f, 0.0f};
static int gHoveredStarId = -1;
static int gHoveredPlanetIndex = -1;     // for star system view
static int gHoveredSystemShipId = -1;    // for star system view

static bool gShowColonyWindow = false;
static int  gSelectedColonyIndex = -1;   // index into gGameState.colonies

// View mode
static bool gInSystemView = false;
static int  gViewedSystemId = -1;        // which star system we are currently zoomed into

// Planet Detail View (drill-down from star system view)
static bool gInPlanetView = false;
static int  gViewedPlanetSystemId = -1;
static int  gViewedPlanetIndex = -1;     // index into the system's planets vector

// System View planet selection
static int gSelectedPlanetIndex = -1;    // index within viewedSys->planets, -1 = none

// === New Game / Race Selection (Phase 2) ===
static bool gInRaceSelection = true;
static std::string gChosenRace = "Psilon";  // default if user just starts without choosing

// Simple combat log for Phase 2 groundwork (external linkage for core lib access)
std::vector<std::string> gCombatLog;

// End Turn confirmation for unset production
static bool gShowEndTurnConfirmation = false;
static std::vector<int> gColoniesWithoutProduction; // indices into colonies

// Phase 3 Turn Report system (external linkage so GameSimulation can access)
std::vector<std::string> gTurnReportMessages;
bool gShowTurnReport = false;

// Per-colony buildings focused window
static bool gShowColonyBuildingsWindow = false;
static int gSelectedColonyForBuildings = -1;

// === Animated Astronomical Background ===
struct BackgroundStar {
    float x, y;           // screen-space base position (we'll apply camera offset lightly)
    float size;
    float twinkleSpeed;
    float phase;
    Color baseColor;
};

struct ShootingStar {
    float x, y;
    float vx, vy;
    float life;
    float maxLife;
    Color color;
};

static std::vector<BackgroundStar> gBackgroundStars;
static std::vector<ShootingStar> gActiveShootingStars;

static void InitAstronomicalBackground() {
    if (!gBackgroundStars.empty()) return; // already initialized

    gBackgroundStars.reserve(420);

    // Create a nice mix of star colors and sizes
    for (int i = 0; i < 420; ++i) {
        BackgroundStar s;
        s.x = (float)(rand() % 2000) - 200.0f;   // generous range
        s.y = (float)(rand() % 1400) - 150.0f;

        // Most stars are small and faint
        float r = (float)rand() / RAND_MAX;
        if (r < 0.65f) {
            s.size = 1.1f + ((float)rand() / RAND_MAX) * 0.9f;
            s.baseColor = { 220, 230, 255, 255 };           // cool white-blue
        } else if (r < 0.88f) {
            s.size = 1.4f + ((float)rand() / RAND_MAX) * 1.1f;
            s.baseColor = { 255, 245, 220, 255 };           // warm white-yellow
        } else {
            s.size = 1.8f + ((float)rand() / RAND_MAX) * 1.4f;
            s.baseColor = { 180, 210, 255, 255 };           // bluish giant
        }

        s.twinkleSpeed = 0.8f + ((float)rand() / RAND_MAX) * 2.2f;
        s.phase = ((float)rand() / RAND_MAX) * 6.28f;

        gBackgroundStars.push_back(s);
    }

    gActiveShootingStars.reserve(4);
}

// === Phase 3 Sounds ===
static Sound sfxClick;
static Sound sfxEndTurn;
static Sound sfxShipOrder;
static Sound sfxColonize;
static Sound sfxEvent;

static void PlayUISound() { PlaySound(sfxClick); }

// === Custom fonts (loaded from assets/fonts/ if present) ===
static ImFont* gFontMain = nullptr;   // body / normal UI text
static ImFont* gFontTitle = nullptr;  // larger for headers and race names

// === Planet textures loaded from SVG-generated PNGs in assets/planets/textures/ ===
// One base + variant per PlanetType (stable selection per planet instance)
static Texture2D gPlanetTextures[9][2] = {{{0}}};
static bool gPlanetTexturesLoaded = false;

// Building catalog with descriptions (for per-planet buildings screen)
struct BuildingDef {
    std::string name;
    std::string description;
    std::string effect;
};

static const std::vector<BuildingDef> BUILDING_DEFS = {
    {"Factory", "Increases industrial output of the colony.", "Each Factory gives +15% to gross production."},
    {"Missile Base", "Provides planetary defense against attacks.", "Improves system defense (future combat)."},
    {"Colony Ship", "Allows colonization of new worlds.", "Consumes the ship upon successful colonization."},
    {"Outpost Ship", "Establishes an outpost (future feature).", "Useful for claiming systems."}
};

// === Phase 3: Leaders / Events types and globals now come from core/GameSimulation.hpp
// (moved for extraction so initializeGame/processEndOfTurn can live in orion_core)
static bool gShowLeadersWindow = false;

std::vector<Leader> gLeaders;
std::vector<GameEvent> gPossibleEvents;
bool gShowEventPopup = false;
GameEvent gCurrentEvent;

// Ship interaction state
static int gSelectedShipId = -1;         // -1 = none selected

// Ship Designer state (Phase 2)
static bool gShowShipDesigner = false;

// Ships management window (Phase 2 - full)
static bool gShowShipsWindow = false;

// Ship order mode (new "Send To" flow)
static bool gShipOrderMode = false;
static int  gShipInOrderMode = -1;   // which ship ID is being given orders
static int  gOrderModeHoveredSystem = -1; // for tooltip (set in input, read in ImGui)

// Tech tree state (Phase 2 - expanded) - external linkage for core simulation
bool gShowTechChoice = false;
std::vector<int> gAvailableTechChoices;

// Simple helper to get a nice color for a planet type (procedural, no textures)
static Color planetColor(orion::PlanetType type) {
    using PT = orion::PlanetType;
    switch (type) {
        case PT::Radiated: return {140, 110, 90, 255};
        case PT::Barren:   return {160, 140, 115, 255};
        case PT::Desert:   return {210, 170, 90, 255};
        case PT::Steppe:   return {160, 175, 95, 255};
        case PT::Arid:     return {185, 165, 105, 255};
        case PT::Swamp:    return {80, 130, 95, 255};
        case PT::Ocean:    return {70, 120, 185, 255};
        case PT::Terran:   return {65, 145, 80, 255};
        case PT::Gaia:     return {90, 200, 130, 255};
        default:           return WHITE;
    }
}

// === Planet texture helpers (SVG-sourced) ===

static std::string planetTypeToAssetName(orion::PlanetType t) {
    using PT = orion::PlanetType;
    switch (t) {
        case PT::Radiated: return "radiated";
        case PT::Barren:   return "barren";
        case PT::Desert:   return "desert";
        case PT::Steppe:   return "steppe";
        case PT::Arid:     return "arid";
        case PT::Swamp:    return "swamp";
        case PT::Ocean:    return "ocean";
        case PT::Terran:   return "terran";
        case PT::Gaia:     return "gaia";
        default:           return "terran";
    }
}

static void LoadPlanetTextures() {
    if (gPlanetTexturesLoaded) return;

    std::vector<std::string> searchDirs;
    searchDirs.push_back("assets/planets/textures/");
    searchDirs.push_back("../assets/planets/textures/");
    searchDirs.push_back("../../assets/planets/textures/");

    const char* appDir = GetApplicationDirectory();
    if (appDir && appDir[0]) {
        searchDirs.push_back(std::string(appDir) + "/assets/planets/textures/");
        searchDirs.push_back(std::string(appDir) + "/../assets/planets/textures/");
        searchDirs.push_back(std::string(appDir) + "/../../assets/planets/textures/");
        searchDirs.push_back(std::string(appDir) + "/orion-reborn/assets/planets/textures/");
    }

    int loadedCount = 0;
    for (int ti = 0; ti < 9; ++ti) {
        auto t = static_cast<orion::PlanetType>(ti);
        std::string tname = planetTypeToAssetName(t);
        for (int v = 0; v < 2; ++v) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "planet_%s_%02d.png", tname.c_str(), v + 1);
            std::string fname = buf;
            bool found = false;
            for (const auto& dir : searchDirs) {
                std::string full = dir + fname;
                if (FileExists(full.c_str())) {
                    gPlanetTextures[ti][v] = LoadTexture(full.c_str());
                    if (IsTextureValid(gPlanetTextures[ti][v])) {
                        SetTextureFilter(gPlanetTextures[ti][v], TEXTURE_FILTER_BILINEAR);
                        ++loadedCount;
                        found = true;
                        TraceLog(LOG_INFO, "Loaded planet texture: %s", full.c_str());
                    } else {
                        TraceLog(LOG_WARNING, "Failed to ready planet texture: %s", full.c_str());
                    }
                    break;
                }
            }
            if (!found) {
                TraceLog(LOG_WARNING, "Missing planet texture: %s (will use procedural fallback)", fname.c_str());
            }
        }
    }
    gPlanetTexturesLoaded = true;
    TraceLog(LOG_INFO, "Planet textures: %d/18 loaded from assets/planets/textures/", loadedCount);
}

static Texture2D GetPlanetTexture(const orion::Planet& pl) {
    int ti = static_cast<int>(pl.type);
    if (ti < 0 || ti >= 9) ti = 7; // Terran fallback

    // Stable variant selection from name + properties (same planet always same art)
    uint32_t h = 2166136261u; // FNV-ish
    for (unsigned char c : pl.name) h = (h ^ c) * 16777619u;
    h ^= static_cast<uint32_t>(pl.size) * 0x9e3779b1u;
    h ^= static_cast<uint32_t>(pl.type) * 1013904223u;
    int v = (h % 2u);

    Texture2D tex = gPlanetTextures[ti][v];
    if (IsTextureValid(tex)) return tex;
    tex = gPlanetTextures[ti][1 - v];
    if (IsTextureValid(tex)) return tex;
    return {0};
}

// Draw a planet texture centered at (x,y) with given on-screen radius (supports rotation for animation)
static void DrawTexturedPlanet(float x, float y, float radius, Texture2D tex, float rotationDeg = 0.0f, Color tint = WHITE) {
    if (!IsTextureValid(tex)) {
        DrawCircleV({x, y}, radius, tint);
        DrawCircleV({x - radius*0.28f, y - radius*0.26f}, radius * 0.32f, Color{255,255,255,40});
        return;
    }
    float texSize = (float)tex.width;
    Rectangle srcRec = { 0.0f, 0.0f, texSize, texSize };
    Rectangle dstRec = { x - radius, y - radius, 2.0f * radius, 2.0f * radius };
    Vector2 origin = { 0.0f, 0.0f };
    DrawTexturePro(tex, srcRec, dstRec, origin, rotationDeg, tint);
}

// Draw a visually distinct star with spectral-type variety (no textures)
static void DrawStarVaried(float x, float y, float zoom, bool owned, bool selected, bool hovered, int seed, orion::SystemSpecial special = orion::SystemSpecial::None) {
    // Use seed for stable but varied appearance
    int type = seed % 7; // 0-6 spectral-ish types

    Color core, glow;
    float coreR, glowR;

    switch (type) {
        case 0: // Hot blue (O/B)
            core = {140, 200, 255, 255};
            glow = {100, 160, 255, 70};
            coreR = 3.2f; glowR = 8.5f;
            break;
        case 1: // White-blue
            core = {220, 235, 255, 255};
            glow = {170, 195, 255, 65};
            coreR = 3.0f; glowR = 7.8f;
            break;
        case 2: // Yellow-white (like Sol)
            core = {255, 240, 200, 255};
            glow = {255, 210, 140, 60};
            coreR = 3.4f; glowR = 8.0f;
            break;
        case 3: // Orange (K)
            core = {255, 195, 120, 255};
            glow = {255, 160, 80, 55};
            coreR = 3.1f; glowR = 7.5f;
            break;
        case 4: // Red dwarf
            core = {255, 130, 100, 255};
            glow = {255, 90, 70, 50};
            coreR = 2.6f; glowR = 6.5f;
            break;
        case 5: // Red giant (bigger, warmer)
            core = {255, 160, 110, 255};
            glow = {255, 110, 70, 65};
            coreR = 4.3f; glowR = 10.0f;
            break;
        default: // Bright white
            core = {255, 250, 240, 255};
            glow = {200, 210, 255, 55};
            coreR = 3.0f; glowR = 7.2f;
            break;
    }

    if (owned) {
        // Slight golden shift for player systems
        core.r = (core.r + 255) / 2;
        core.g = (core.g + 220) / 2;
    }

    float cr = coreR * (selected ? 1.25f : 1.0f) * zoom;
    float gr = glowR * (selected ? 1.15f : 1.0f) * zoom;

    // Outer soft glow
    DrawCircleV({x, y}, gr, glow);
    // Main core
    DrawCircleV({x, y}, cr, core);
    // Hot inner pinpoint
    DrawCircleV({x, y}, cr * 0.45f, {255, 255, 255, 200});

    // Selection / hover rings
    if (selected) {
        float pulse = 0.85f + 0.15f * sinf(GetTime() * 3.5f);
        DrawCircleLines(x, y, 18.0f * zoom * pulse, Color{130, 210, 255, 220});
        DrawCircleLines(x, y, 24.0f * zoom, Color{90, 170, 230, 110});
    } else if (hovered) {
        DrawCircleLines(x, y, 16.0f * zoom, Color{180, 210, 255, 160});
    }

    // Subtle ownership indicator ring for owned systems
    if (owned && !selected) {
        DrawCircleLines(x, y, 11.5f * zoom, Color{255, 220, 120, 90});
    }

    // === Special system visual flair (pulsing colored ring / aura) ===
    if (special != orion::SystemSpecial::None) {
        Color accent{200, 180, 255, 180}; // default mystery purple
        float baseRadius = 15.5f * zoom;
        float pulseSpeed = 2.8f;
        bool danger = false;

        switch (special) {
            case orion::SystemSpecial::PirateHaven:
            case orion::SystemSpecial::BiohazardZone:
            case orion::SystemSpecial::AutomatedDefense:
            case orion::SystemSpecial::UnstableStar:
                accent = {255, 90, 70, 195}; // red/orange danger
                danger = true;
                pulseSpeed = 4.2f;
                break;
            case orion::SystemSpecial::WormholeNexus:
            case orion::SystemSpecial::NebulaShroud:
                accent = {160, 120, 255, 185}; // deep violet
                pulseSpeed = 3.6f;
                break;
            case orion::SystemSpecial::HyperRichWorld:
            case orion::SystemSpecial::DerelictMegastructure:
                accent = {255, 215, 90, 200}; // rich gold/amber
                break;
            case orion::SystemSpecial::PrecursorRuins:
            case orion::SystemSpecial::RogueAI:
                accent = {80, 220, 200, 185}; // teal / ancient tech
                break;
            case orion::SystemSpecial::PrimitiveSpecies:
            case orion::SystemSpecial::RebelColony:
                accent = {140, 230, 140, 175}; // vibrant green (life/diplomacy)
                break;
            default: break;
        }

        float pulse = 0.82f + 0.18f * sinf(GetTime() * pulseSpeed + (seed * 0.7f));
        float r = baseRadius * pulse;

        // Outer thin ring
        DrawCircleLines(x, y, r, accent);
        // Inner highlight ring for extra pop on danger/high value
        if (danger || special == orion::SystemSpecial::WormholeNexus || special == orion::SystemSpecial::DerelictMegastructure) {
            DrawCircleLines(x, y, r * 0.72f, Color{accent.r, accent.g, accent.b, (unsigned char)(accent.a * 0.55f)});
        }
    }
}

// =====================================================================================
// Detailed Animated Planet View (new immersive colony surface visualization)
// =====================================================================================
// Called when the player drills down into a specific planet.
// Features:
// - SVG-sourced planet textures for ALL planet types (Radiated..Gaia) + variants
// - Slow rotation animation of the textured surface
// - Population-driven city lights (density + twinkling) overlaid
// - Animated cloud / haze layers + type special effects (lava, ice caps, aurora, etc.)
// - Hooks for future building visualization (factories, shields, etc.)
static void DrawDetailedAnimatedPlanet(float cx, float cy, float radius,
                                       const orion::Planet& pl,
                                       float population,
                                       float maxPopulation,
                                       float time) {
    using PT = orion::PlanetType;

    const float popRatio = maxPopulation > 0.1f ? (population / maxPopulation) : 0.0f;
    const float popFactor = std::clamp(popRatio * popRatio * 1.15f, 0.0f, 1.0f); // more lights at high pop

    // === Base color from planet type (richer palette) ===
    Color base = planetColor(pl.type);
    Color darkBase = { (uint8_t)(base.r * 0.45f), (uint8_t)(base.g * 0.42f), (uint8_t)(base.b * 0.48f), 255 };

    // Subtle rotation offset for the whole planet (used for overlays + texture)
    // Slowed down for clarity: this is the planet spinning on its axis, not something orbiting a star.
    float rot = time * 0.012f + (pl.name.length() * 0.4f);   // stable per planet, slow majestic spin
    float planetRotationDeg = rot * (180.0f / PI);

    // 1. Soft outer atmosphere glow (perfect circle, runtime only - SVGs no longer bake outer glow)
    float atmSize = radius * (pl.type >= PT::Ocean ? 1.22f : 1.12f);
    Color atmColor = (pl.type == PT::Gaia)   ? Color{115, 255, 165, 48} :
                     (pl.type >= PT::Ocean)  ? Color{85, 155, 255, 42} :
                     (pl.type <= PT::Barren) ? Color{170, 130, 85, 32} : Color{130, 120, 150, 36};
    DrawCircleV({cx, cy}, atmSize, atmColor);

    // 2. Main planet body from improved SVG texture (tight circular content, transparent margins)
    Texture2D ptex = GetPlanetTexture(pl);
    DrawTexturedPlanet(cx, cy, radius, ptex, planetRotationDeg, WHITE);

    // 3. Dynamic terminator shading (fixed lighting direction for 3D pop; no baked shade in SVGs)
    float shadeOffsetX = radius * 0.15f;
    float shadeOffsetY = radius * 0.10f;
    DrawCircleV({cx + shadeOffsetX, cy + shadeOffsetY}, radius * 0.985f, Color{darkBase.r, darkBase.g, darkBase.b, 78});

    // Subtle edge definition to help planet read as a globe (on top of texture)
    DrawCircleV({cx, cy}, radius + 0.6f, Color{0, 0, 0, 22});

    int featureSeed = (int)pl.name.length() * 31 + static_cast<int>(pl.size) * 7 + static_cast<int>(pl.type) * 3;

    // 5. Cloud / haze layer (smaller, closer to disk so they don't read as separate orbs)
    if (pl.type >= PT::Arid) {
        float cloudRot = time * 0.055f + (featureSeed * 0.3f);
        Color cloudCol = (pl.type == PT::Gaia) ? Color{255, 255, 255, 46} :
                         (pl.type >= PT::Ocean) ? Color{235, 245, 255, 40} : Color{220, 210, 190, 34};

        for (int c = 0; c < 3; ++c) {
            float ca = cloudRot + c * 2.1f;
            float cd = radius * (0.32f + c * 0.08f);
            float cloudX = cx + cosf(ca) * cd * 0.6f;
            float cloudY = cy + sinf(ca * 0.7f) * cd * 0.48f;
            float cloudR = radius * (0.38f + (c % 2) * 0.08f);
            DrawCircleV({cloudX, cloudY}, cloudR, cloudCol);
        }
    }

    // 6. City lights / civilization (population driven, on the night side)
    if (popFactor > 0.02f) {
        int lightCount = (int)(popFactor * 38.0f) + (population > 4.0f ? 6 : 0);
        for (int l = 0; l < lightCount; ++l) {
            // Use stable "random" positions based on planet + light index
            uint32_t h = (featureSeed * 31u + l * 17u + (uint32_t)(time * 0.3f)) % 360;
            float la = (h * 0.01745f) + rot * 0.9f;   // rotate with planet

            // Bias lights toward the "night" side (roughly right side in our fake lighting)
            float lx = cx + cosf(la) * (radius * 0.72f);
            float ly = cy + sinf(la) * (radius * 0.62f);

            // Only draw lights on the darker half
            float lightSide = cosf(la - 0.8f); // rough night side test
            if (lightSide < 0.35f) {
                float twinkle = 0.65f + 0.35f * sinf(time * 3.8f + l * 1.7f + featureSeed);
                float lsize = 1.1f + (popFactor * 1.6f) * (0.6f + (l % 3) * 0.2f);

                Color lightCol = (pl.type == PT::Gaia) ? Color{180, 255, 200, (uint8_t)(200 * twinkle)} :
                                 (pl.type >= PT::Ocean) ? Color{160, 210, 255, (uint8_t)(185 * twinkle)} :
                                 Color{255, 220, 120, (uint8_t)(210 * twinkle)};

                DrawCircleV({lx, ly}, lsize, lightCol);

                // Occasional brighter core
                if ((l % 3) == 0) {
                    DrawCircleV({lx, ly}, lsize * 0.45f, Color{255, 255, 240, (uint8_t)(120 * twinkle)});
                }
            }
        }
    }

    // 7. Type-specific special effects
    if (pl.type == PT::Radiated) {
        // Lava / radioactive glow cracks
        for (int v = 0; v < 4; ++v) {
            float va = rot * 0.8f + v * 1.6f;
            float vx = cx + cosf(va) * radius * 0.6f;
            float vy = cy + sinf(va) * radius * 0.52f;
            Color lava = {255, 80, 30, (uint8_t)(90 + 40 * sinf(time * 4.0f + v))};
            DrawCircleV({vx, vy}, 2.2f + sinf(time + v) * 0.8f, lava);
        }
    }

    if (pl.type == PT::Gaia || pl.type == PT::Terran) {
        // Gentle aurora / life shimmer near poles (tight)
        float auroraPhase = sinf(time * 0.7f) * 0.5f + 0.5f;
        DrawCircleV({cx - radius * 0.22f, cy - radius * 0.72f}, radius * 0.16f,
                    Color{120, 255, 180, (uint8_t)(32 * auroraPhase)});
        DrawCircleV({cx + radius * 0.18f, cy + radius * 0.71f}, radius * 0.13f,
                    Color{140, 230, 255, (uint8_t)(26 * auroraPhase)});
    }

    if (pl.type <= PT::Desert && pl.type != PT::Swamp) {
        // Ice caps / polar regions (pulled in, smaller so they sit on the globe, not separate)
        float iceAlpha = (pl.type == PT::Radiated) ? 28 : 58;
        DrawCircleV({cx, cy - radius * 0.71f}, radius * 0.21f, Color{235, 245, 255, (uint8_t)iceAlpha});
        DrawCircleV({cx, cy + radius * 0.71f}, radius * 0.17f, Color{235, 245, 255, (uint8_t)(iceAlpha * 0.75f)});
    }

    // 8. Final bright highlight (specular)
    DrawCircleV({cx - radius * 0.32f, cy - radius * 0.30f}, radius * 0.28f, Color{255, 255, 255, 38});

    // 9. Subtle surface grid / texture suggestion at high population (future building hint)
    if (popFactor > 0.45f) {
        for (int g = 0; g < 7; ++g) {
            float ga = rot * 0.4f + g * 0.9f;
            float gx = cx + cosf(ga) * radius * 0.55f;
            float gy = cy + sinf(ga) * radius * 0.48f;
            DrawCircleLines(gx, gy, 1.5f, Color{255, 255, 255, 22});
        }
    }

    // Selection / focus ring
    DrawCircleLines(cx, cy, radius + 6.0f, Color{100, 180, 255, 80});
    DrawCircleLines(cx, cy, radius + 10.0f, Color{70, 140, 210, 45});
}

// === Animated deep space background ===
static void DrawAstronomicalBackground(int screenW, int screenH, bool inSystemView) {
    // Very dark space background
    ClearBackground(Color{4, 5, 14, 255});

    float time = static_cast<float>(GetTime());

    // === Twinkling background stars ===
    float parallax = inSystemView ? 0.08f : 0.35f; // less movement in system view

    for (const auto& star : gBackgroundStars) {
        float bx = star.x + gCameraOffset.x * parallax;
        float by = star.y + gCameraOffset.y * parallax;

        // Wrap around screen edges for infinite feel
        bx = fmodf(bx, (float)screenW + 400.0f);
        by = fmodf(by, (float)screenH + 300.0f);
        if (bx < -200) bx += screenW + 400.0f;
        if (by < -150) by += screenH + 300.0f;

        // Twinkle
        float brightness = 0.55f + 0.45f * sinf(time * star.twinkleSpeed + star.phase);
        if (brightness < 0.15f) brightness = 0.15f;

        unsigned char alpha = (unsigned char)(brightness * 255.0f);
        Color c = star.baseColor;
        c.a = alpha;

        float sz = star.size * (inSystemView ? 0.7f : 1.0f);

        if (sz > 1.6f) {
            // Bigger stars get a tiny soft glow
            Color glow = { c.r, c.g, c.b, (unsigned char)(alpha * 0.25f) };
            DrawCircleV({bx, by}, sz * 1.8f, glow);
        }

        DrawCircleV({bx, by}, sz, c);
    }

    // === Very faint slow-pulsing nebulae (adds nice atmosphere) ===
    if (!inSystemView) {
        // Only in galaxy view for now
        float n1 = 0.08f + 0.03f * sinf(time * 0.07f);
        float n2 = 0.06f + 0.025f * sinf(time * 0.11f + 1.7f);

        DrawCircleV({screenW * 0.18f, screenH * 0.35f}, 180.0f, Color{80, 60, 140, (unsigned char)(n1 * 255)});
        DrawCircleV({screenW * 0.78f, screenH * 0.72f}, 140.0f, Color{60, 90, 130, (unsigned char)(n2 * 255)});
    }

    // === Occasional shooting stars ===
    // Spawn new ones rarely
    if ((rand() % 180) == 0 && gActiveShootingStars.size() < 3) {
        ShootingStar ss;
        ss.x = (float)(rand() % screenW);
        ss.y = (float)(rand() % (screenH / 2));
        float speed = 380.0f + (rand() % 220);
        float angle = -0.35f + ((rand() % 100) / 100.0f) * 0.7f; // mostly left to right downward
        ss.vx = cosf(angle) * speed;
        ss.vy = sinf(angle) * speed;
        ss.maxLife = 0.45f + ((rand() % 100) / 100.0f) * 0.35f;
        ss.life = ss.maxLife;
        ss.color = {220, 235, 255, 230};
        gActiveShootingStars.push_back(ss);
    }

    // Update and draw active shooting stars
    for (size_t i = 0; i < gActiveShootingStars.size(); ) {
        auto& ss = gActiveShootingStars[i];
        ss.x += ss.vx * GetFrameTime();
        ss.y += ss.vy * GetFrameTime();
        ss.life -= GetFrameTime();

        if (ss.life <= 0.0f) {
            gActiveShootingStars.erase(gActiveShootingStars.begin() + i);
            continue;
        }

        float t = ss.life / ss.maxLife;
        unsigned char a = (unsigned char)(t * 230.0f);

        // Draw the streak
        Vector2 start = {ss.x, ss.y};
        Vector2 end = {ss.x - ss.vx * 0.018f, ss.y - ss.vy * 0.018f};

        DrawLineEx(start, end, 1.6f, Color{ss.color.r, ss.color.g, ss.color.b, a});

        // Bright head
        DrawCircleV(start, 1.8f, Color{255, 255, 255, (unsigned char)(a * 0.9f)});

        ++i;
    }
}

// Draw the classic MoO Colony Management window with 5 sliders
static void DrawColonyManagementWindow() {
    if (!gShowColonyWindow || gSelectedColonyIndex < 0 ||
        gSelectedColonyIndex >= static_cast<int>(gGameState.colonies.size())) {
        gShowColonyWindow = false;
        return;
    }

    auto& colony = gGameState.colonies[gSelectedColonyIndex];

    // Find the planet for live calculations
    orion::Planet* planetPtr = nullptr;
    for (auto& sys : gGameState.galaxy.systems) {
        for (auto& pl : sys.planets) {
            if (sys.starId == colony.planetId) {
                planetPtr = &pl;
                break;
            }
        }
        if (planetPtr) break;
    }

    if (planetPtr) {
        float techBonus = gGameState.technology.getIndustryBonus();
        float researchTechBonus = gGameState.technology.getResearchBonus();
        // Apply owner racial production mod to preview for honesty
        float ownerProdMod = 1.0f;
        for (const auto& emp : gGameState.empires) {
            if (emp.id == colony.ownerId) { ownerProdMod = emp.productionMod; break; }
        }
        colony.recalculateOutputs(planetPtr->size, planetPtr->type, planetPtr->richness,
                                  planetPtr->traits, static_cast<float>(planetPtr->maxPopulation),
                                  techBonus, ownerProdMod);
        // Research gets its own bonus
        colony.researchOutput *= researchTechBonus;
    }

    ImGui::SetNextWindowSize(ImVec2(540, 520), ImGuiCond_FirstUseEver);  // Taller to avoid scrolling when queuing production
    ImGui::Begin("Colony Management", &gShowColonyWindow, ImGuiWindowFlags_NoCollapse);

    // Header
    ImGui::TextColored(ImVec4(0.9f, 0.85f, 0.6f, 1.0f), "Colony on %s",
                       planetPtr ? planetPtr->name.c_str() : "Unknown Planet");
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", planetPtr ? to_string(planetPtr->type).data() : "");
    ImGui::Text("Population: %.1f / %.0f million", colony.population, colony.maxPopulation);

    // Population growth preview
    float growthPreview = (colony.foodNet > 0.8f) ? 0.11f : (colony.foodNet > 0.2f ? 0.05f : 0.01f);
    if (planetPtr && (planetPtr->traits & static_cast<uint32_t>(orion::PlanetTrait::Fertile))) growthPreview *= 1.25f;
    ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.6f, 1.0f), "Est. pop growth next turn: +%.2f", growthPreview);
    ImGui::Separator();

    // === Simplified Economy (no sliders) ===
    ImGui::TextColored(ImVec4(0.85f, 0.92f, 1.0f, 1.0f), "Colony Output (Formula Driven)");

    float pop = colony.population;

    // Food (quick transparent view)
    {
        float baseFood = pop * 1.0f;
        if (planetPtr && planetPtr->type >= orion::PlanetType::Ocean) baseFood += pop * 0.3f;
        if (planetPtr && (planetPtr->traits & static_cast<uint32_t>(orion::PlanetTrait::Fertile))) baseFood *= 1.25f;
        float consumption = pop * 0.8f;
        ImGui::Text("Food: %.1f produced - %.1f consumed = %.1f surplus", baseFood, consumption, colony.foodNet);
    }

    // Clear formula display (no sliders)
    ImGui::TextColored(ImVec4(0.85f, 0.92f, 1.0f, 1.0f), "Industrial Output Formula (no allocation)");

    float base = pop * 0.8f;
    auto* p = planetPtr;
    float richF = p ? (0.65f + (static_cast<int>(p->richness)-1)*0.28f) : 1.0f;
    float sizeF = p ? (0.8f + static_cast<int>(p->size)*0.09f) : 1.0f;
    float typeF = 1.0f;
    if (p) {
        if (p->type == orion::PlanetType::Gaia) typeF = 1.6f;
        else if (p->type >= orion::PlanetType::Ocean) typeF = 1.25f;
    }
    float planetQ = richF * sizeF * typeF;

    float bBonus = 0.0f;
    for (const auto& b : colony.completedBuildings) {
        if (b.find("Factory") != std::string::npos) bBonus += 0.13f;
    }

    float tB = gGameState.technology.getIndustryBonus();
    float rMod = 1.0f;
    for (const auto& emp : gGameState.empires) {
        if (emp.id == colony.ownerId) { rMod = emp.productionMod; break; }
    }

    float gross = base * planetQ * (1.0f + bBonus) * tB * rMod;
    float maint = gross * 0.22f;
    float net = std::max(0.0f, gross - maint);

    ImGui::Text("Base (%.1f) × Planet (%.2f) × Buildings (%.2f) × Tech (%.2f) × Racial (%.2f)",
                base, planetQ, (1.0f + bBonus), tB, rMod);
    ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.3f, 1.0f), "Gross Production: %.1f", gross);
    ImGui::Text("Maintenance (22%%): -%.1f", maint);
    ImGui::TextColored(ImVec4(0.4f, 0.95f, 0.5f, 1.0f), "Net available for current project: %.1f", net);

    ImGui::Separator();

    ImGui::Separator();

    ImGui::TextColored(ImVec4(0.85f, 0.9f, 1.0f, 1.0f), "What to Build (one project at a time)");

    // Simple projects as direct buttons (no queue)
    for (int i = 1; i < IM_ARRAYSIZE(orion::Colony::PROJECTS); ++i) {
        if (ImGui::Button(orion::Colony::PROJECTS[i])) {
            colony.currentProject = orion::Colony::PROJECTS[i];
            colony.projectCost = orion::Colony::PROJECT_COSTS[i];
            colony.projectProgress = 0.0f;
        }
        ImGui::SameLine();
    }

    ImGui::NewLine();
    ImGui::Text("Ship Designs");

    if (orion::playerDesigns.empty()) {
        ImGui::TextDisabled("No designs saved yet. Open Ship Designer (F2).");
    } else {
        for (size_t i = 0; i < orion::playerDesigns.size(); ++i) {
            auto& d = orion::playerDesigns[i];
            if (ImGui::Button(("Build " + d.name).c_str())) {
                colony.currentProject = "Build: " + d.name;
                colony.projectCost = d.buildCost;
                colony.projectProgress = 0.0f;
            }
            ImGui::SameLine();
        }
    }

    if (colony.projectCost > 0) {
        float pct = colony.projectProgress / colony.projectCost;
        ImGui::ProgressBar(pct, ImVec2(-1, 0), (std::to_string((int)(pct * 100)) + "%").c_str());
        ImGui::TextDisabled("Cost: %d  |  Remaining: %.0f", colony.projectCost, colony.projectCost - colony.projectProgress);

        // Show estimated turns to completion
        if (colony.netProduction > 0.1f) {
            float remaining = colony.projectCost - colony.projectProgress;
            int turns = (int)ceilf(remaining / colony.netProduction);
            if (turns <= 1) {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "ETA: 1 turn (next turn)");
            } else {
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.6f, 1.0f), "ETA: %d turns", turns);
            }
        } else {
            ImGui::TextDisabled("ETA: N/A (low production)");
        }
    } else {
        ImGui::TextDisabled("No active project");
    }

    ImGui::Separator();

    // Show completed buildings and their effects
    if (!colony.completedBuildings.empty()) {
        ImGui::Text("Completed Buildings:");
        for (const auto& b : colony.completedBuildings) {
            ImGui::BulletText("%s", b.c_str());
        }
    }

    ImGui::TextDisabled("Sliders + queue are live. Press End Turn to apply production.");

    ImGui::End();
}

// Dedicated Colony Buildings window (distinct per-planet screen)
static void DrawColonyBuildingsWindow() {
    if (!gShowColonyBuildingsWindow || gSelectedColonyForBuildings < 0 ||
        gSelectedColonyForBuildings >= static_cast<int>(gGameState.colonies.size())) {
        gShowColonyBuildingsWindow = false;
        return;
    }

    auto& colony = gGameState.colonies[gSelectedColonyForBuildings];

    // Find system name
    auto* sys = gGameState.galaxy.findSystemById(colony.planetId);
    std::string sysName = sys ? sys->name : "Unknown System";

    ImGui::SetNextWindowSize(ImVec2(480, 380), ImGuiCond_FirstUseEver);
    ImGui::Begin(("Colony - " + sysName).c_str(), &gShowColonyBuildingsWindow, ImGuiWindowFlags_NoCollapse);

    ImGui::TextColored(ImVec4(0.9f, 0.85f, 0.6f, 1.0f), "Buildings & Infrastructure on %s", sysName.c_str());
    ImGui::Text("Population: %.1f / %.0f", colony.population, colony.maxPopulation);
    ImGui::Separator();

    ImGui::Text("Completed Buildings:");
    if (colony.completedBuildings.empty()) {
        ImGui::TextDisabled("No buildings completed yet.");
    } else {
        for (const auto& b : colony.completedBuildings) {
            // Find description
            std::string desc = "A useful structure for the colony.";
            std::string effect = "";
            for (const auto& def : BUILDING_DEFS) {
                if (b.find(def.name) != std::string::npos) {
                    desc = def.description;
                    effect = def.effect;
                    break;
                }
            }
            ImGui::Bullet();
            ImGui::Text("%s", b.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("- %s", desc.c_str());
            if (!effect.empty()) {
                ImGui::Indent();
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Effect: %s", effect.c_str());
                ImGui::Unindent();
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Queue New Building:");

    // Show available projects with ETA
    for (int i = 1; i < IM_ARRAYSIZE(orion::Colony::PROJECTS); ++i) {
        const char* proj = orion::Colony::PROJECTS[i];
        int cost = orion::Colony::PROJECT_COSTS[i];

        float eta = (colony.netProduction > 0.1f) ? (cost / colony.netProduction) : 999.0f;

        if (ImGui::Button(proj, ImVec2(200, 0))) {
            colony.currentProject = proj;
            colony.projectCost = cost;
            colony.projectProgress = 0.0f;
        }
        ImGui::SameLine();
        if (eta < 999) {
            ImGui::TextColored(ImVec4(0.85f, 0.9f, 0.6f, 1.0f), "ETA: %.0f turns", ceilf(eta));
        } else {
            ImGui::TextDisabled("ETA: N/A");
        }
    }

    ImGui::Separator();

    if (colony.projectCost > 0) {
        float pct = colony.projectProgress / colony.projectCost;
        ImGui::ProgressBar(pct, ImVec2(-1, 0), (std::to_string((int)(pct * 100)) + "%").c_str());
        ImGui::Text("Current: %s  |  Remaining: %.0f", colony.currentProject.c_str(), colony.projectCost - colony.projectProgress);
    } else {
        ImGui::TextDisabled("No building queued. Select one above to start production.");
    }

    ImGui::End();
}

// === Race Selection Screen (Phase 2) ===
// Returns true if the player clicked a "Play as" button and we should exit race selection.
static bool DrawRaceSelectionWindow() {
    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();

    bool exitRequested = false;

    // Title
    ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f - 280, 60), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(560, 90), ImGuiCond_Always);
    ImGui::Begin("##Title", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);

    if (gFontTitle) ImGui::PushFont(gFontTitle, 24.0f);   // loaded larger for better spacing
    ImGui::TextColored(ImVec4(0.88f, 0.92f, 1.0f, 1.0f), "ORION REBORN");
    if (gFontTitle) ImGui::PopFont();

    ImGui::TextColored(ImVec4(0.62f, 0.72f, 0.88f, 1.0f), "Choose Your Race");
    ImGui::End();

    // Race cards
    struct RaceInfo {
        std::string name;
        std::string flavor;
        std::string bonuses;
        Color accent;
    };

    std::vector<RaceInfo> races = {
        {"Psilon", "Highly intelligent and curious. Masters of research but grow slowly.",
         "Research +40%  |  Growth -15%", {100, 180, 255, 255}},

        {"Human", "Versatile and adaptable. A balanced choice for any strategy.",
         "Growth +10%  |  Research +5%  |  Balanced overall", {80, 200, 140, 255}},

        {"Mrrshan", "Fierce warriors with a proud martial tradition.",
         "Combat +30%  |  Growth +10%", {255, 90, 70, 255}},

        {"Silicoid", "Silicon-based lifeforms. Extremely hardy and industrious.",
         "Production +25%  |  Growth -20%  |  Research -10%", {180, 160, 130, 255}},
    };

    float startY = 160.0f;
    float cardHeight = 120.0f;
    float cardWidth = 520.0f;

    for (size_t i = 0; i < races.size(); ++i) {
        const auto& r = races[i];

        ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f - cardWidth * 0.5f, startY + i * (cardHeight + 12)), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cardWidth, cardHeight), ImGuiCond_Always);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.13f, 0.18f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(r.accent.r / 255.0f, r.accent.g / 255.0f, r.accent.b / 255.0f, 0.6f));
        ImGui::Begin(("##race" + std::to_string(i)).c_str(), nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

        if (gFontTitle) ImGui::PushFont(gFontTitle, 22.0f);   // render 26pt font smaller → more character spacing
        ImGui::TextColored(ImVec4(r.accent.r / 255.0f, r.accent.g / 255.0f, r.accent.b / 255.0f, 1.0f), "%s", r.name.c_str());
        if (gFontTitle) ImGui::PopFont();

        // Wrap long flavor text to prevent overflow
        ImGui::PushTextWrapPos(cardWidth - 20.0f);
        ImGui::TextDisabled("%s", r.flavor.c_str());
        ImGui::PopTextWrapPos();

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.75f, 0.85f, 0.95f, 1.0f), "%s", r.bonuses.c_str());

        ImGui::Dummy(ImVec2(0, 4));

        if (ImGui::Button(("Play as " + r.name).c_str(), ImVec2(180, 28))) {
            gChosenRace = r.name;
            PlaySound(sfxColonize);  // nice positive confirmation
            initializeGame(gChosenRace);

            // Reset any UI state
            gSelectedShipId = -1;
            gSelectedPlanetIndex = -1;
            gInSystemView = false;
            gShowColonyWindow = false;

            exitRequested = true;
        }

        ImGui::End();
        ImGui::PopStyleColor(2);
    }

    // Footer hint
    ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f - 200, screenH - 70), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400, 40), ImGuiCond_Always);
    ImGui::Begin("##footer", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
    ImGui::TextDisabled("Classic Master of Orion experience • Different races have unique strengths");
    ImGui::End();

    return exitRequested;
}



// processEndOfTurn() has been moved to src/core/GameSimulation.cpp (staged extraction for testability).
// The declaration in GameSimulation.hpp makes it available here via the include we added earlier.

int main(int argc, char** argv) {
    // Window setup
    const int screenWidth = 1280;
    const int screenHeight = 800;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);

    InitWindow(screenWidth, screenHeight, "Orion Reborn - Master of Orion (Phase 2)");
    SetTargetFPS(120);
    InitAudioDevice();           // Phase 3 - sounds

    InitAstronomicalBackground(); // nice twinkling stars + occasional shooting stars

    // Generate simple procedural sound effects (no external files needed) - improved variety for retro 4X feel
    auto GenWaveSine = [](float frequency, float duration, int sampleRate) -> Wave {
        int frameCount = (int)(duration * sampleRate);
        short* data = (short*)malloc(frameCount * sizeof(short));
        for (int i = 0; i < frameCount; i++) {
            float t = (float)i / sampleRate;
            // Add slight 2nd harmonic for richer tone (less pure sine)
            float s = sinf(2.0f * PI * frequency * t) + 0.18f * sinf(2.0f * PI * frequency * 2.0f * t);
            data[i] = (short)(28000.0f * s);
            float fade = 1.0f - (float)i / frameCount;
            data[i] = (short)(data[i] * fade * 0.92f);
        }
        Wave wave = { 0 };
        wave.frameCount = frameCount;
        wave.sampleRate = sampleRate;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = data;
        return wave;
    };

    // Quick "square-ish" blip for UI clicks (more 8-bit retro character)
    auto GenBlip = [](float frequency, float duration, int sampleRate) -> Wave {
        int frameCount = (int)(duration * sampleRate);
        short* data = (short*)malloc(frameCount * sizeof(short));
        for (int i = 0; i < frameCount; i++) {
            float t = (float)i / sampleRate;
            float s = sinf(2.0f * PI * frequency * t) > 0 ? 1.0f : -0.7f; // square-like
            data[i] = (short)(30000.0f * s);
            float fade = 1.0f - (float)i / (frameCount * 0.7f);
            if (fade < 0) fade = 0;
            data[i] = (short)(data[i] * fade * 0.85f);
        }
        Wave wave = { 0 };
        wave.frameCount = frameCount;
        wave.sampleRate = sampleRate;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = data;
        return wave;
    };

    {
        // Short crisp UI click / select (high square blip)
        Wave waveClick = GenBlip(1050.0f, 0.045f, 16000);
        sfxClick = LoadSoundFromWave(waveClick);
        UnloadWave(waveClick);
        SetSoundVolume(sfxClick, 0.65f);

        // End Turn / confirm (deeper, solid low tone)
        Wave waveEnd = GenWaveSine(320.0f, 0.18f, 16000);
        sfxEndTurn = LoadSoundFromWave(waveEnd);
        UnloadWave(waveEnd);
        SetSoundVolume(sfxEndTurn, 0.75f);

        // Ship order / move command (mid bright blip)
        Wave waveOrder = GenBlip(620.0f, 0.07f, 16000);
        sfxShipOrder = LoadSoundFromWave(waveOrder);
        UnloadWave(waveOrder);
        SetSoundVolume(sfxShipOrder, 0.6f);

        // Colonization / positive success (pleasant bright rising feel)
        Wave waveCol = GenWaveSine(980.0f, 0.09f, 16000);
        sfxColonize = LoadSoundFromWave(waveCol);
        UnloadWave(waveCol);
        SetSoundVolume(sfxColonize, 0.7f);

        // Event / notification / enter system (soft mid tone)
        Wave waveEvent = GenWaveSine(740.0f, 0.11f, 16000);
        sfxEvent = LoadSoundFromWave(waveEvent);
        UnloadWave(waveEvent);
        SetSoundVolume(sfxEvent, 0.65f);
    }

    rlImGuiSetup(true);

    // === Load custom fonts from assets/fonts/ (you can pick any good .ttf you like) ===
    // Drop a font file in orion-reborn/assets/fonts/ and restart the game.
    // See assets/fonts/README.md for recommended fonts + detailed instructions.
    // (Inter and Exo 2 are especially nice for this kind of game.)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();

        // Robust font discovery:
        // We build a list of directories to search because the current working directory
        // can be almost anything depending on how the user launched the game.
        const char* fontNames[] = {
            "Inter-Regular.ttf", "Inter.ttf",
            "Exo2-Regular.ttf", "Exo2.ttf",
            "Roboto-Regular.ttf", "Roboto.ttf",
            "Rajdhani-Regular.ttf", "Orbitron-Regular.ttf",
            nullptr
        };

        std::vector<std::string> searchDirs;

        // Always try these relative to CWD first (good for development from project root)
        searchDirs.push_back("assets/fonts/");
        searchDirs.push_back("../assets/fonts/");
        searchDirs.push_back("../../assets/fonts/");

        // Then try relative to where the executable actually lives
        const char* appDir = GetApplicationDirectory();
        if (appDir && appDir[0]) {
            TraceLog(LOG_INFO, "Font loader: application directory = %s", appDir);

            // Common layouts when running the binary directly:
            // build/bin/orion-reborn  ->  ../../assets/fonts/
            searchDirs.push_back(std::string(appDir) + "/assets/fonts/");
            searchDirs.push_back(std::string(appDir) + "/../assets/fonts/");
            searchDirs.push_back(std::string(appDir) + "/../../assets/fonts/");
            searchDirs.push_back(std::string(appDir) + "/../../../assets/fonts/");
            searchDirs.push_back(std::string(appDir) + "/orion-reborn/assets/fonts/");
        }

        // === High-quality font configuration (addresses blurry/aliased fonts) ===
        // - Oversampling (stb_truetype only): 3xH/1xV is a great sweet spot.
        // - FreeType (when enabled in CMake): much sharper + hinting, oversampling ignored.
        // - PixelSnapH + RasterizerMultiply improve crispness and small-font brightness.
        ImFontConfig fontCfg;
        fontCfg.OversampleH = 3;
        fontCfg.OversampleV = 1;
        fontCfg.PixelSnapH  = true;
        fontCfg.RasterizerMultiply = 1.25f;   // brighten thin glyphs slightly (1.0 = neutral)

#ifdef IMGUI_ENABLE_FREETYPE
        // Force auto-hinter often gives the best results for UI sizes on many fonts.
        fontCfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_ForceAutoHint;
        // Other useful options you can experiment with:
        // fontCfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LightHinting;
        // fontCfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bold; // synthetic
#endif

        std::string loadedFullPath;
        bool found = false;

        // Font loading.
        // We intentionally load the font at a slightly larger size than we render it.
        // This gives more breathing room between characters and improves readability.
        // You can tune the "rendered size" in the PushFont calls below if you want
        // even more or less spacing.
        for (const auto& dir : searchDirs) {
            for (int n = 0; fontNames[n] != nullptr; ++n) {
                std::string fullPath = dir + fontNames[n];
                if (FileExists(fullPath.c_str())) {
                    // Load at a larger physical size than we will display.
                    // The extra "virtual" space between glyphs improves readability.
                    gFontMain  = io.Fonts->AddFontFromFileTTF(fullPath.c_str(), 18.0f, &fontCfg);
                    gFontTitle = io.Fonts->AddFontFromFileTTF(fullPath.c_str(), 26.0f, &fontCfg);

                    loadedFullPath = fullPath;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }

        if (found) {
            TraceLog(LOG_INFO, "Loaded custom font: %s", loadedFullPath.c_str());
#ifdef IMGUI_ENABLE_FREETYPE
            TraceLog(LOG_INFO, "  (using FreeType rasterizer for best quality)");
#else
            TraceLog(LOG_INFO, "  (using stb_truetype + oversampling; install FreeType for even better results)");
#endif
        } else {
            // Fallback: still use a tuned config for the embedded default font
            ImFontConfig defaultCfg;
            defaultCfg.PixelSnapH = true;
            defaultCfg.RasterizerMultiply = 1.2f;
            gFontMain  = io.Fonts->AddFontDefault(&defaultCfg);
            gFontTitle = gFontMain;
            TraceLog(LOG_INFO, "Using default ImGui font (no .ttf in assets/fonts/)");
        }

        // Make the loaded (slightly larger) font the default so most UI text benefits
        // from the extra breathing room between characters.
        if (gFontMain) {
            io.FontDefault = gFontMain;
        }
    }

    // Load planet artwork (SVG rasterized PNGs) - robust path search like fonts
    LoadPlanetTextures();

    // Main game loop
    while (!WindowShouldClose()) {
        // === Race Selection Menu (Phase 2) ===
        if (gInRaceSelection) {
            BeginDrawing();
            DrawAstronomicalBackground(GetScreenWidth(), GetScreenHeight(), false);

            rlImGuiBegin();

            bool exitRace = DrawRaceSelectionWindow();

            rlImGuiEnd();
            EndDrawing();

            if (exitRace) {
                gInRaceSelection = false;

                // One guaranteed clean frame with no race selection windows.
                // This prevents the previous race UI draw data from lingering
                // visually when the normal (mostly commented) game loop takes over.
                BeginDrawing();
                DrawAstronomicalBackground(GetScreenWidth(), GetScreenHeight(), false);
                rlImGuiBegin();
                // Submit an empty ImGui frame
                rlImGuiEnd();
                EndDrawing();
                continue;
            }

            continue;  // Skip normal game logic until a race is chosen
        }
        const float dt = GetFrameTime();
        const int screenW = GetScreenWidth();
        const int screenH = GetScreenHeight();

        // === Input ===
        if (IsKeyPressed(KEY_F1)) gShowDebugPanel = !gShowDebugPanel;
        if (IsKeyPressed(KEY_F2)) gShowShipDesigner = !gShowShipDesigner;
        if (IsKeyPressed(KEY_F3)) gShowShipsWindow = !gShowShipsWindow;
        if (IsKeyPressed(KEY_F4)) gShowLeadersWindow = !gShowLeadersWindow;

        if (IsKeyPressed(KEY_ESCAPE)) {
            if (gShipOrderMode) {
                gShipOrderMode = false;
                gShipInOrderMode = -1;
            } else if (gInPlanetView) {
                // Back out one level: Planet → Star System
                gInPlanetView = false;
                gSelectedPlanetIndex = gViewedPlanetIndex;
            } else if (gInSystemView) {
                gInSystemView = false;
                gSelectedPlanetIndex = -1;
            }
        }

        // If we're in ship order mode, force back to galaxy view
        if (gShipOrderMode && gInSystemView) {
            gInSystemView = false;
            gSelectedPlanetIndex = -1;
        }

        // Right-click backs out of planet view (or system view)
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            if (gInPlanetView) {
                gInPlanetView = false;
                gSelectedPlanetIndex = gViewedPlanetIndex;
            } else if (gInSystemView && !gShipOrderMode) {
                gInSystemView = false;
                gSelectedPlanetIndex = -1;
            }
        }

        // Cursor handling for ship order mode
        if (gShipOrderMode) {
            HideCursor();
        } else if (!IsCursorHidden()) {
            // Only show if we previously hid it (simple state management)
            // We will show it when exiting order mode below
        }

        // Camera (WASD/arrows + wheel)
        const float panSpeed = 520.0f * dt;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) gCameraOffset.x -= panSpeed;
        if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) gCameraOffset.x += panSpeed;
        if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) gCameraOffset.y -= panSpeed;
        if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) gCameraOffset.y += panSpeed;

        // Mouse wheel zoom disabled - using fixed magnification level instead
        // (const float wheel = GetMouseWheelMove(); ... removed per user request)

        // Click to select star systems (disabled while inside a star system view)
        if (!gInSystemView && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            float bestDist = 30.0f / gZoom;
            int bestId = -1;
            for (const auto& sys : gGameState.galaxy.systems) {
                float sx = sys.position.x + gCameraOffset.x;
                float sy = sys.position.y + gCameraOffset.y;
                float d2 = (mouse.x - sx)*(mouse.x - sx) + (mouse.y - sy)*(mouse.y - sy);
                if (d2 < bestDist * bestDist) {
                    bestDist = std::sqrt(d2);
                    bestId = sys.starId;
                }
            }
            if (bestId != -1) {
                gGameState.selectedStarId = bestId;
                PlaySound(sfxClick);
            }

            // Double-click to enter detailed Star System View
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                static double lastClickTime = 0.0;
                double now = GetTime();
                if (now - lastClickTime < 0.25) { // double click
                    if (gGameState.selectedStarId != -1) {
                        gViewedSystemId = gGameState.selectedStarId;
                        gInSystemView = true;
                        gSelectedShipId = -1;
                        gSelectedPlanetIndex = -1; // reset planet selection
                        PlaySound(sfxEvent);
                    }
                }
                lastClickTime = now;
            }

            // Click on ships to select them (simple distance check) - only on galaxy map
            if (!gInSystemView && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 mouse = GetMousePosition();
                int closestShip = -1;
                float bestDist = 22.0f;

                for (const auto& sh : gGameState.ships) {
                    // Find current visual position of the ship
                    auto* locSys = gGameState.galaxy.findSystemById(sh.locationSystemId);
                    if (!locSys) continue;

                    float baseX = locSys->position.x + gCameraOffset.x;
                    float baseY = locSys->position.y + gCameraOffset.y;

                    // If moving, interpolate a bit for click feel (simple)
                    float sx = baseX;
                    float sy = baseY;

                    if (sh.isMoving) {
                        auto* dest = gGameState.galaxy.findSystemById(sh.destinationSystemId);
                        if (dest) {
                            float dx = dest->position.x + gCameraOffset.x;
                            float dy = dest->position.y + gCameraOffset.y;
                            sx = baseX + (dx - baseX) * sh.travelProgress;
                            sy = baseY + (dy - baseY) * sh.travelProgress;
                        }
                    }

                    float dx = mouse.x - sx;
                    float dy = mouse.y - sy;
                    float d = sqrtf(dx*dx + dy*dy);
                    if (d < bestDist) {
                        bestDist = d;
                        closestShip = sh.id;
                    }
                }

                if (closestShip != -1) {
                    gSelectedShipId = closestShip;
                    gGameState.selectedStarId = -1;
                    PlaySound(sfxShipOrder);
                }
            }

            // Right-click to order selected ship to a star system
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && gSelectedShipId != -1) {
                Vector2 mouse = GetMousePosition();
                int targetStar = -1;
                float bestDist = 28.0f / gZoom;

                for (const auto& sys : gGameState.galaxy.systems) {
                    float sx = sys.position.x + gCameraOffset.x;
                    float sy = sys.position.y + gCameraOffset.y;
                    float dx = mouse.x - sx;
                    float dy = mouse.y - sy;
                    float d = sqrtf(dx*dx + dy*dy);
                    if (d < bestDist) {
                        bestDist = d;
                        targetStar = sys.starId;
                    }
                }

                if (targetStar != -1) {
                    for (auto& sh : gGameState.ships) {
                        if (sh.id == gSelectedShipId) {
                            if (sh.locationSystemId != targetStar) {
                                // Range check for direct right-click orders too
                                auto* fromSys = gGameState.galaxy.findSystemById(sh.locationSystemId);
                                auto* toSys   = gGameState.galaxy.findSystemById(targetStar);
                                float d = (fromSys && toSys) ? GetSystemDistance(fromSys, toSys) : 99999.0f;
                                if (d <= sh.maxRange + 0.5f) {
                                    sh.destinationSystemId = targetStar;
                                    sh.isMoving = true;
                                    sh.travelProgress = 0.0f;
                                    PlaySound(sfxShipOrder);
                                } else {
                                    PlaySound(sfxClick); // feedback even on out-of-range attempt
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }

        // === New Ship Order Mode (cursor becomes ship, hover shows destination) ===
        if (gShipOrderMode && gShipInOrderMode != -1 && !gInSystemView) {
            // Find the ship we're ordering
            orion::Ship* orderingShip = nullptr;
            for (auto& sh : gGameState.ships) {
                if (sh.id == gShipInOrderMode) {
                    orderingShip = &sh;
                    break;
                }
            }

            if (orderingShip) {
                // Hover detection only (tooltip moved to ImGui section)
                gOrderModeHoveredSystem = -1;
                float bestDist = 26.0f / gZoom;

                Vector2 mouse = GetMousePosition();
                for (const auto& sys : gGameState.galaxy.systems) {
                    float sx = sys.position.x + gCameraOffset.x;
                    float sy = sys.position.y + gCameraOffset.y;
                    float d2 = (mouse.x - sx)*(mouse.x - sx) + (mouse.y - sy)*(mouse.y - sy);
                    if (d2 < bestDist * bestDist) {
                        bestDist = sqrtf(d2);
                        gOrderModeHoveredSystem = sys.starId;
                    }
                }

                // Left click to confirm order (only if within ship's max range)
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && gOrderModeHoveredSystem != -1) {
                    auto* cur = gGameState.galaxy.findSystemById(orderingShip->locationSystemId);
                    auto* tgt = gGameState.galaxy.findSystemById(gOrderModeHoveredSystem);
                    float dist = (cur && tgt) ? GetSystemDistance(cur, tgt) : 99999.0f;
                    bool canReach = (dist <= orderingShip->maxRange + 0.5f);

                    if (gOrderModeHoveredSystem != orderingShip->locationSystemId && canReach) {
                        orderingShip->destinationSystemId = gOrderModeHoveredSystem;
                        orderingShip->isMoving = true;
                        orderingShip->travelProgress = 0.0f;
                    } else if (!canReach) {
                        // Do nothing (or could play a deny sound later)
                    }

                    // Exit order mode either way
                    gShipOrderMode = false;
                    gShipInOrderMode = -1;
                    gOrderModeHoveredSystem = -1;
                    ShowCursor();
                }

                // Right click or ESC to cancel
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    gShipOrderMode = false;
                    gShipInOrderMode = -1;
                    gOrderModeHoveredSystem = -1;
                    ShowCursor();
                }
            } else {
                // Ship no longer exists
                gShipOrderMode = false;
                gShipInOrderMode = -1;
                gOrderModeHoveredSystem = -1;
                ShowCursor();
            }
        } else if (IsCursorHidden()) {
            ShowCursor();
        }

        // === System View Planet Clicking ===
        if (gInSystemView && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            auto* viewedSys = gGameState.galaxy.findSystemById(gViewedSystemId);
            if (viewedSys) {
                Vector2 mouse = GetMousePosition();
                float cx = screenW * 0.5f;
                float cy = screenH * 0.42f;
                float time = static_cast<float>(GetTime());

                int num = static_cast<int>(viewedSys->planets.size());
                int clickedIndex = -1;

                for (int i = 0; i < num; ++i) {
                    const auto& pl = viewedSys->planets[i];

                    float orbitRadius = 55.0f + (i * 28.0f);
                    if (num > 4) orbitRadius *= 0.9f;

                    float orbitSpeed = 0.12f + (i % 3) * 0.04f;
                    float angle = (i * 1.35f) + (viewedSys->starId * 0.4f) + (time * orbitSpeed);

                    float px = cx + cosf(angle) * orbitRadius;
                    float py = cy + sinf(angle) * orbitRadius * 0.55f;

                    float pr = 5.0f;
                    switch (pl.size) {
                        case orion::PlanetSize::Tiny:   pr = 3.5f; break;
                        case orion::PlanetSize::Small:  pr = 4.5f; break;
                        case orion::PlanetSize::Medium: pr = 6.0f; break;
                        case orion::PlanetSize::Large:  pr = 8.0f; break;
                        case orion::PlanetSize::Huge:   pr = 10.5f; break;
                        default: break;
                    }

                    float dx = mouse.x - px;
                    float dy = mouse.y - py;
                    float d2 = dx*dx + dy*dy;
                    float hitRadius = pr + 6.0f;   // generous click area

                    if (d2 < hitRadius * hitRadius) {
                        clickedIndex = i;
                    }
                }

                if (clickedIndex != -1) {
                    gSelectedPlanetIndex = clickedIndex;
                    const auto& clickedPl = viewedSys->planets[clickedIndex];

                    if (clickedPl.isColonized() && clickedPl.ownerEmpireId == 0) {
                        // Drill down into immersive Planet View
                        gViewedPlanetSystemId = viewedSys->starId;
                        gViewedPlanetIndex = clickedIndex;
                        gInPlanetView = true;

                        // Also keep colony window data in sync (for now)
                        gSelectedColonyIndex = -1;
                        for (int c = 0; c < static_cast<int>(gGameState.colonies.size()); ++c) {
                            if (gGameState.colonies[c].planetId == viewedSys->starId) {
                                gSelectedColonyIndex = c;
                                break;
                            }
                        }
                        gShowColonyWindow = false; // we'll show integrated UI in planet view instead
                    } else if (!clickedPl.isColonized() && clickedPl.canBeColonized()) {
                        // Just select the planet. Colonization now requires explicit confirmation
                        // via the Star System Menu (no automatic colonization on click).
                        gSelectedPlanetIndex = clickedIndex;
                    }
                } else {
                    // Clicked empty space in system view - deselect planet
                    gSelectedPlanetIndex = -1;
                }
            }
        }

        // === System View Ship Clicking ===
        // Clicking an orbiting ship in the detailed view selects it (for Send To orders etc.)
        if (gInSystemView && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            auto* viewedSys = gGameState.galaxy.findSystemById(gViewedSystemId);
            if (viewedSys) {
                Vector2 mouse = GetMousePosition();
                float cx = screenW * 0.5f;
                float cy = screenH * 0.42f;
                float time = static_cast<float>(GetTime());

                float fleetRadius = 185.0f;
                float fleetSpeed  = 0.028f;

                int shipIndex = 0;
                for (auto& sh : gGameState.ships) {
                    if (sh.locationSystemId != gViewedSystemId || sh.ownerId != 0) continue;

                    bool hasOrders = sh.isMoving || sh.destinationSystemId != -1;
                    float thisRadius = hasOrders ? fleetRadius * 1.28f : fleetRadius;

                    float shipAngle = (shipIndex * 1.95f) + (viewedSys->starId * 0.65f) + (time * fleetSpeed);

                    float sx = cx + cosf(shipAngle) * thisRadius;
                    float sy = cy + sinf(shipAngle) * thisRadius * 0.46f;

                    float hitRadius = 22.0f; // generous for the drawn ship visuals + propulsion
                    float dx = mouse.x - sx;
                    float dy = mouse.y - sy;
                    if ((dx*dx + dy*dy) < hitRadius * hitRadius) {
                        gSelectedShipId = sh.id;
                        gSelectedPlanetIndex = -1; // deselect planet when selecting ship
                        PlaySound(sfxShipOrder);
                        break;
                    }
                    shipIndex++;
                }
            }
        }

        // Hover detection
        gHoveredStarId = -1;
        gHoveredPlanetIndex = -1;
        gHoveredSystemShipId = -1;
        {
            Vector2 mouse = GetMousePosition();
            for (const auto& sys : gGameState.galaxy.systems) {
                float sx = sys.position.x + gCameraOffset.x;
                float sy = sys.position.y + gCameraOffset.y;
                float d2 = (mouse.x - sx)*(mouse.x - sx) + (mouse.y - sy)*(mouse.y - sy);
                if (d2 < (26.0f / gZoom)*(26.0f / gZoom)) {
                    gHoveredStarId = sys.starId;
                    break;
                }
            }
        }

        // === Drawing ===
        BeginDrawing();
        DrawAstronomicalBackground(screenW, screenH, gInSystemView || gInPlanetView);

        if (gInPlanetView) {
            // ==================== PLANET DETAIL VIEW (immersive animated surface) ====================
            auto* viewedSys = gGameState.galaxy.findSystemById(gViewedPlanetSystemId);
            if (!viewedSys || gViewedPlanetIndex < 0 ||
                gViewedPlanetIndex >= static_cast<int>(viewedSys->planets.size())) {
                gInPlanetView = false;
            } else {
                const auto& planet = viewedSys->planets[gViewedPlanetIndex];

                // Find the colony for population data
                float pop = 0.0f;
                float maxPop = static_cast<float>(planet.maxPopulation);
                for (const auto& col : gGameState.colonies) {
                    if (col.planetId == viewedSys->starId && col.ownerId == 0) {
                        pop = col.population;
                        maxPop = col.maxPopulation;
                        break;
                    }
                }

                float time = static_cast<float>(GetTime());
                float planetRadius = 135.0f;   // Nice large size for detail

                // Draw the beautiful animated planet (positioned to leave room for left UI panel while feeling central)
                float planetDrawX = screenW * 0.55f;
                DrawDetailedAnimatedPlanet(planetDrawX, screenH * 0.52f, planetRadius,
                                           planet, pop, maxPop, time);

                // Clear explanatory labels so it's obvious this is a close-up of ONE planet's surface (not a star + orbiting planet)
                DrawTextEx(GetFontDefault(),
                           "PLANET SURFACE VIEW",
                           {planetDrawX - 130, screenH * 0.52f - planetRadius - 45},
                           22.0f, 1.0f, Color{180, 210, 255, 255});
                DrawTextEx(GetFontDefault(),
                           "(globe slowly rotates to reveal surface; fixed lighting from upper-left)",
                           {planetDrawX - 200, screenH * 0.52f - planetRadius - 22},
                           13.0f, 1.0f, Color{150, 170, 200, 200});

                // Subtle label under the planet
                DrawTextEx(GetFontDefault(),
                           TextFormat("%s  •  %s  •  Pop %.1fM",
                                      planet.name.c_str(),
                                      to_string(planet.type).data(),
                                      pop),
                           {planetDrawX - 140, screenH * 0.52f + planetRadius + 18},
                           18.0f, 1.0f, Color{200, 210, 230, 230});
            }
        } else if (gInSystemView) {
            // ==================== STAR SYSTEM VIEW (raylib part only) ====================
            auto* viewedSys = gGameState.galaxy.findSystemById(gViewedSystemId);
            if (!viewedSys) {
                gInSystemView = false;
            } else {
                float cx = screenW * 0.5f;
                float cy = screenH * 0.42f;

                // Draw the central star big
                DrawStarVaried(cx, cy, 3.2f, viewedSys->ownerEmpireId == 0, true, false, viewedSys->starId * 7, viewedSys->specialStatus);

                // Draw planets orbiting the star (slow animation + clickable)
                const auto& planets = viewedSys->planets;
                int num = static_cast<int>(planets.size());
                float time = static_cast<float>(GetTime());

                for (int i = 0; i < num; ++i) {
                    const auto& pl = planets[i];

                    // Orbit radius (spread them out nicely)
                    float orbitRadius = 55.0f + (i * 28.0f);
                    if (num > 4) orbitRadius *= 0.9f;

                    // Slow orbital motion - different speeds for different planets
                    float orbitSpeed = 0.12f + (i % 3) * 0.04f;   // outer planets slower
                    float angle = (i * 1.35f) + (viewedSys->starId * 0.4f) + (time * orbitSpeed);

                    float px = cx + cosf(angle) * orbitRadius;
                    float py = cy + sinf(angle) * orbitRadius * 0.55f; // slight vertical squash

                    // Planet visual size based on real data
                    float pr = 5.0f;
                    switch (pl.size) {
                        case orion::PlanetSize::Tiny:   pr = 3.5f; break;
                        case orion::PlanetSize::Small:  pr = 4.5f; break;
                        case orion::PlanetSize::Medium: pr = 6.0f; break;
                        case orion::PlanetSize::Large:  pr = 8.0f; break;
                        case orion::PlanetSize::Huge:   pr = 10.5f; break;
                        default: break;
                    }

                    Color pc = planetColor(pl.type);

                    // Hover detection for tooltip
                    Vector2 mouse = GetMousePosition();
                    float dx = mouse.x - px;
                    float dy = mouse.y - py;
                    if ((dx*dx + dy*dy) < (pr + 6.0f) * (pr + 6.0f)) {
                        gHoveredPlanetIndex = i;
                    }

                    // Draw faint orbit
                    DrawCircleLines(cx, cy, orbitRadius, Color{40, 50, 70, 80});

                    // Selection ring if this planet is clicked
                    bool isSelectedPlanet = (i == gSelectedPlanetIndex);
                    if (isSelectedPlanet) {
                        DrawCircleLines(px, py, pr + 5.0f, Color{120, 200, 255, 200});
                        DrawCircleLines(px, py, pr + 7.5f, Color{80, 160, 230, 110});
                    }

                    // Draw the planet (SVG texture with slow rotation for life)
                    Texture2D ptex = GetPlanetTexture(pl);
                    float prot = (float)(pl.name.length() * 11 + i * 7) + (float)(GetTime() * (0.6f + (i % 4) * 0.12f));
                    DrawTexturedPlanet(px, py, pr, ptex, prot, WHITE);

                    // Subtle highlight on top (works for both textured + fallback)
                    DrawCircleV({px - pr*0.3f, py - pr*0.25f}, pr * 0.38f, Color{255,255,255,38});

                    // Gas giant rings
                    bool gasGiant = (pl.type == orion::PlanetType::Swamp || pl.type == orion::PlanetType::Ocean) && pr > 7.0f;
                    if (gasGiant) {
                        DrawEllipseLines(px, py, pr * 1.9f, pr * 0.6f, Color{pc.r, pc.g, pc.b, 130});
                    }

                    // Colonized indicator
                    if (pl.isColonized()) {
                        DrawCircleLines(px, py, pr + 3.0f, Color{255, 230, 140, 220});
                        DrawCircleV({px + pr*0.35f, py - pr*0.2f}, 1.8f, Color{255, 255, 220, 255});
                    }
                }

                // === Draw ships present in this system - slow orbiting fleet (further out + detailed + propulsion) ===
                float fleetRadius = 185.0f;     // Further from the star so they feel like a proper fleet
                float fleetSpeed  = 0.028f;     // Very slow, majestic movement

                int shipIndex = 0;
                for (auto& sh : gGameState.ships) {
                    if (sh.locationSystemId != gViewedSystemId || sh.ownerId != 0) continue;

                    // Ships with orders (Send To / moving) orbit further out to indicate they are leaving
                    bool hasOrders = sh.isMoving || sh.destinationSystemId != -1;
                    float thisRadius = hasOrders ? fleetRadius * 1.28f : fleetRadius;

                    // Spread ships around the orbit
                    float shipAngle = (shipIndex * 1.95f) + (viewedSys->starId * 0.65f) + (time * fleetSpeed);

                    float sx = cx + cosf(shipAngle) * thisRadius;
                    float sy = cy + sinf(shipAngle) * thisRadius * 0.46f; // slight perspective squash

                    // Hover detection for tooltip
                    Vector2 mouse = GetMousePosition();
                    float dxs = mouse.x - sx;
                    float dys = mouse.y - sy;
                    if ((dxs*dxs + dys*dys) < 28.0f * 28.0f) {
                        gHoveredSystemShipId = sh.id;
                    }

                    bool isSel = (sh.id == gSelectedShipId);

                    // Base color
                    Color shipCol;
                    if (sh.type == orion::ShipType::Scout) {
                        shipCol = {110, 195, 255, 255};           // Cool blue for scouts
                    } else {
                        shipCol = {255, 195, 70, 255};            // Warm gold for colony ships
                    }
                    if (sh.isFromDesign()) shipCol = {160, 255, 175, 255}; // Designed ships get a nice green tint
                    if (isSel) shipCol = {255, 255, 130, 255};

                    float scale = isSel ? 1.15f : 1.0f;
                    if (sh.type == orion::ShipType::ColonyShip) scale *= 1.35f; // Colony ships are larger
                    float s = 8.5f * scale;

                    // Calculate facing direction (tangent to orbit for nice fleet look)
                    float dirX = -sinf(shipAngle);
                    float dirY =  cosf(shipAngle) * 0.46f;   // match the vertical squash

                    float perpX = -dirY * 0.9f;
                    float perpY =  dirX * 0.9f;

                    // === Draw detailed ship hull ===
                    if (sh.type == orion::ShipType::Scout) {
                        // Sleek scout: longer body + small wings
                        Vector2 nose   = { sx + dirX * s * 1.1f, sy + dirY * s * 1.1f };
                        Vector2 tail   = { sx - dirX * s * 0.7f, sy - dirY * s * 0.7f };
                        Vector2 wingL  = { sx + perpX * s * 0.55f, sy + perpY * s * 0.55f };
                        Vector2 wingR  = { sx - perpX * s * 0.55f, sy - perpY * s * 0.55f };

                        DrawTriangle(nose, wingL, tail, shipCol);
                        DrawTriangle(nose, tail, wingR, shipCol);

                        // Cockpit highlight
                        DrawCircleV({ sx + dirX * s * 0.35f, sy + dirY * s * 0.35f }, s * 0.28f, Color{200, 230, 255, 200});
                    } else {
                        // Colony ship: bulkier with command section
                        Vector2 nose   = { sx + dirX * s * 0.95f, sy + dirY * s * 0.95f };
                        Vector2 left   = { sx + perpX * s * 0.85f - dirX * s * 0.35f, sy + perpY * s * 0.85f - dirY * s * 0.35f };
                        Vector2 right  = { sx - perpX * s * 0.85f - dirX * s * 0.35f, sy - perpY * s * 0.85f - dirY * s * 0.35f };
                        Vector2 tailL  = { sx - dirX * s * 0.65f + perpX * s * 0.45f, sy - dirY * s * 0.65f + perpY * s * 0.45f };
                        Vector2 tailR  = { sx - dirX * s * 0.65f - perpX * s * 0.45f, sy - dirY * s * 0.65f - perpY * s * 0.45f };

                        DrawTriangle(nose, left, right, shipCol);           // main hull
                        DrawTriangle(left, tailL, tailR, Color{
                            static_cast<unsigned char>(shipCol.r * 0.85f + 0.5f),
                            static_cast<unsigned char>(shipCol.g * 0.85f + 0.5f),
                            static_cast<unsigned char>(shipCol.b * 0.85f + 0.5f),
                            255
                        }); // rear section

                        // Command module
                        DrawCircleV({ sx + dirX * s * 0.25f, sy + dirY * s * 0.25f }, s * 0.32f, Color{255, 255, 255, 160});
                    }

                    // === Propulsion / Engine glow ===
                    float engineDirX = -dirX;
                    float engineDirY = -dirY;

                    float engineIntensity = 0.75f + 0.25f * sinf(time * 18.0f + shipIndex * 2.3f); // nice flicker

                    Color coreCol   = (sh.type == orion::ShipType::Scout) ? Color{140, 220, 255, 220} : Color{255, 160, 60, 230};
                    Color outerCol  = (sh.type == orion::ShipType::Scout) ? Color{80, 160, 255, 90}  : Color{255, 110, 30, 110};

                    // Main engine glow (behind the ship)
                    float ex = sx + engineDirX * (s * 0.55f);
                    float ey = sy + engineDirY * (s * 0.55f);

                    // Bright core
                    DrawCircleV({ex, ey}, s * 0.38f * engineIntensity, coreCol);
                    // Softer outer flame
                    DrawCircleV({ex + engineDirX * s * 0.15f, ey + engineDirY * s * 0.15f}, s * 0.72f * engineIntensity, outerCol);

                    // Extra thin thruster lines for detail
                    float tLen = s * (1.1f + 0.4f * engineIntensity);
                    DrawLineEx({ex, ey},
                               {ex + engineDirX * tLen * 0.6f, ey + engineDirY * tLen * 0.6f},
                               1.6f * gZoom, coreCol);

                    // Selection ring
                    if (isSel) {
                        DrawCircleLines(sx, sy, 17.0f, Color{255, 245, 150, 210});
                        DrawCircleLines(sx, sy, 21.0f, Color{255, 230, 100, 110});
                    }

                    shipIndex++;
                }
            }
        } else {
            // ==================== NORMAL GALAXY VIEW ====================
            // Real galaxy rendering - only stars visible here
            for (const auto& sys : gGameState.galaxy.systems) {
                float sx = sys.position.x + gCameraOffset.x;
                float sy = sys.position.y + gCameraOffset.y;

                bool selected = (sys.starId == gGameState.selectedStarId);
                bool hovered  = (sys.starId == gHoveredStarId);
                bool owned    = (sys.ownerEmpireId == 0);

                // Draw only the star/sun
                DrawStarVaried(sx, sy, gZoom, owned, selected, hovered, sys.starId, sys.specialStatus);

                // Colony ownership flag (colored by empire)
                if (sys.hasColony()) {
                    float flagX = sx + 9.5f * gZoom;
                    float flagY = sy - 9.0f * gZoom;
                    DrawLineEx({flagX, flagY + 4.0f*gZoom}, {flagX, flagY - 3.5f*gZoom}, 1.5f * gZoom, Color{220, 210, 170, 230});

                    // Find empire color
                    Color flagColor = {80, 180, 120, 235}; // default green
                    for (const auto& emp : gGameState.empires) {
                        if (emp.id == sys.ownerEmpireId) {
                            flagColor = emp.color;
                            flagColor.a = 235;
                            break;
                        }
                    }
                    DrawRectangle(flagX, flagY - 3.5f*gZoom, 5.5f * gZoom, 2.8f * gZoom, flagColor);
                }

                // Small ship presence icon next to star (Phase 3 request)
                // Only count ships that are currently stationed here (not moving away)
                int playerShipsHere = 0;
                for (const auto& sh : gGameState.ships) {
                    if (sh.ownerId == 0 && sh.locationSystemId == sys.starId && !sh.isMoving) {
                        playerShipsHere++;
                    }
                }
                if (playerShipsHere > 0) {
                    float iconX = sx + 22.0f * gZoom;
                    float iconY = sy - 5.0f * gZoom;
                    // Small triangle as ship icon
                    DrawTriangle({iconX, iconY - 4.0f*gZoom}, {iconX - 4.0f*gZoom, iconY + 4.0f*gZoom}, {iconX + 4.0f*gZoom, iconY + 4.0f*gZoom}, Color{100, 180, 255, 220});
                    // Count
                    DrawText(std::to_string(playerShipsHere).c_str(), (int)(iconX + 6*gZoom), (int)(iconY - 4*gZoom), 10, Color{200, 220, 255, 255});
                }

                // Draw ships at or moving from this system (improved motion visuals)
                for (const auto& sh : gGameState.ships) {
                    // Moving ships no longer "belong" to their origin system for display purposes
                    bool drawAtThisSystem = false;
                    if (!sh.isMoving) {
                        if (sh.locationSystemId == sys.starId) drawAtThisSystem = true;
                    } else {
                        // Only show moving ships at their *destination* system on the map icons
                        if (sh.destinationSystemId == sys.starId) drawAtThisSystem = true;
                    }
                    if (!drawAtThisSystem) continue;

                    float baseX = sx;
                    float baseY = sy;

                    float drawX = baseX;
                    float drawY = baseY;
                    float dirX = 0.0f;
                    float dirY = -1.0f; // default upward

                    if (sh.isMoving && sh.destinationSystemId != -1) {
                        auto* destSys = gGameState.galaxy.findSystemById(sh.destinationSystemId);
                        if (destSys) {
                            float dx = destSys->position.x + gCameraOffset.x;
                            float dy = destSys->position.y + gCameraOffset.y;

                            // Smooth interpolated position
                            float t = sh.travelProgress;
                            drawX = baseX + (dx - baseX) * t;
                            drawY = baseY + (dy - baseY) * t;

                            // Direction vector for ship orientation
                            float len = sqrtf((dx-baseX)*(dx-baseX) + (dy-baseY)*(dy-baseY));
                            if (len > 0.001f) {
                                dirX = (dx - baseX) / len;
                                dirY = (dy - baseY) / len;
                            }
                        }
                    }

                    bool isSelected = (sh.id == gSelectedShipId);

                    // Distinct visuals per ship type
                    Color shipCol;
                    float size = isSelected ? 8.0f : 6.0f;

                    if (sh.type == orion::ShipType::Scout) {
                        shipCol = {120, 200, 255, 255}; // Light blue for scouts
                        if (sh.isFromDesign()) shipCol = {150, 230, 255, 255};
                    } else {
                        shipCol = {255, 200, 80, 255}; // Gold for colony ships
                        if (sh.isFromDesign()) shipCol = {200, 255, 150, 255};
                    }
                    if (isSelected) shipCol = {255, 255, 120, 255};

                    float px = drawX;
                    float py = drawY;

                    // Draw different shapes
                    if (sh.type == orion::ShipType::Scout) {
                        // Small fast scout: diamond shape
                        float s = size * gZoom;
                        DrawLineEx({px, py - s}, {px + s*0.6f, py}, 2.0f * gZoom, shipCol);
                        DrawLineEx({px + s*0.6f, py}, {px, py + s*0.6f}, 2.0f * gZoom, shipCol);
                        DrawLineEx({px, py + s*0.6f}, {px - s*0.6f, py}, 2.0f * gZoom, shipCol);
                        DrawLineEx({px - s*0.6f, py}, {px, py - s}, 2.0f * gZoom, shipCol);
                    } else {
                        // Colony ship: classic triangle
                        float perpX = -dirY;
                        float perpY = dirX;
                        Vector2 p1 = { px + dirX * size * gZoom, py + dirY * size * gZoom };
                        Vector2 p2 = { px - dirX * size*0.55f * gZoom + perpX * size*0.7f * gZoom,
                                       py - dirY * size*0.55f * gZoom + perpY * size*0.7f * gZoom };
                        Vector2 p3 = { px - dirX * size*0.55f * gZoom - perpX * size*0.7f * gZoom,
                                       py - dirY * size*0.55f * gZoom - perpY * size*0.7f * gZoom };
                        DrawTriangle(p1, p2, p3, shipCol);
                    }

                    // Selection ring + movement line
                    if (isSelected) {
                        DrawCircleLines(px, py, 12.0f * gZoom, Color{255, 240, 140, 220});

                        if (sh.isMoving && sh.destinationSystemId != -1) {
                            auto* destSys = gGameState.galaxy.findSystemById(sh.destinationSystemId);
                            if (destSys) {
                                float dx = destSys->position.x + gCameraOffset.x;
                                float dy = destSys->position.y + gCameraOffset.y;
                                DrawLineEx({px, py}, {dx, dy}, 1.5f * gZoom, Color{255, 230, 120, 160});
                            }
                        }
                    }
                }

                // Draw custom ship cursor when in order mode
                if (gShipOrderMode && gShipInOrderMode != -1) {
                    Vector2 mouse = GetMousePosition();
                    float cx = mouse.x;
                    float cy = mouse.y;
                    float s = 8.0f;

                    // Draw a small ship icon as cursor
                    DrawTriangle({cx, cy - s}, {cx - s*0.6f, cy + s*0.7f}, {cx + s*0.6f, cy + s*0.7f}, Color{255, 220, 100, 230});
                    DrawCircleLines(cx, cy, 12.0f, Color{255, 240, 140, 180});
                }
            }

            // Background grid (galaxy view only)
            const int gstep = static_cast<int>(50 * gZoom);
            if (gstep > 13) {
                Color gc{26, 30, 46, 52};
                for (int gx = -3; gx < 27; ++gx) {
                    int x = static_cast<int>(55 + gx * gstep + fmodf(gCameraOffset.x, gstep));
                    DrawLine(x, 48, x, screenH - 48, gc);
                }
                for (int gy = -2; gy < 17; ++gy) {
                    int y = static_cast<int>(48 + gy * gstep + fmodf(gCameraOffset.y, gstep));
                    DrawLine(28, y, screenW - 28, y, gc);
                }
            }
        }

    // ==================== COMMON UI (both views) ====================
    // ImGui
    rlImGuiBegin();

        // ==================== Tooltips ====================

        // --- Star system tooltip (main galaxy map) ---
        if (!gInSystemView && gHoveredStarId != -1) {
            if (auto* sys = gGameState.galaxy.findSystemById(gHoveredStarId)) {
                ImGui::BeginTooltip();

                ImGui::TextColored(ImVec4(0.85f, 0.9f, 1.0f, 1.0f), "%s", sys->name.c_str());
                ImGui::Text("%zu planets", sys->planets.size());

                if (sys->ownerEmpireId == 0) {
                    ImGui::TextColored(ImVec4(0.4f, 0.95f, 0.5f, 1.0f), "Colonized by you");
                } else if (sys->ownerEmpireId == -1) {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Unexplored");
                } else {
                    std::string ownerName = "Unknown Empire";
                    for (const auto& emp : gGameState.empires) {
                        if (emp.id == sys->ownerEmpireId) {
                            ownerName = emp.name;
                            break;
                        }
                    }
                    ImGui::Text("Owned by %s", ownerName.c_str());
                }

                // Special system status (rare & flavorful)
                if (sys->specialStatus != orion::SystemSpecial::None) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "★ %s", to_string(sys->specialStatus).data());
                    // Short one-line flavor hook (keep tooltip compact)
                    switch (sys->specialStatus) {
                        case orion::SystemSpecial::PirateHaven:           ImGui::TextDisabled("Pirate syndicate stronghold — expect raids"); break;
                        case orion::SystemSpecial::PrecursorRuins:        ImGui::TextDisabled("Ancient ruins — high research value, risks"); break;
                        case orion::SystemSpecial::NebulaShroud:          ImGui::TextDisabled("Dense nebula — stealth & hazards"); break;
                        case orion::SystemSpecial::HyperRichWorld:        ImGui::TextDisabled("Mineral wealth — geological instability"); break;
                        case orion::SystemSpecial::PrimitiveSpecies:      ImGui::TextDisabled("Native civilization — diplomacy or conquest"); break;
                        case orion::SystemSpecial::AutomatedDefense:      ImGui::TextDisabled("Lethal automated defenses — huge rewards"); break;
                        case orion::SystemSpecial::BiohazardZone:         ImGui::TextDisabled("Plague world — bio-research opportunity"); break;
                        case orion::SystemSpecial::WormholeNexus:         ImGui::TextDisabled("Strategic wormhole hub — major mobility"); break;
                        case orion::SystemSpecial::DerelictMegastructure: ImGui::TextDisabled("Megastructure remnants — long-term power"); break;
                        case orion::SystemSpecial::RogueAI:               ImGui::TextDisabled("Rogue AI entity — advanced tech or threat"); break;
                        case orion::SystemSpecial::RebelColony:           ImGui::TextDisabled("Breakaway colony — negotiable, unique assets"); break;
                        case orion::SystemSpecial::UnstableStar:          ImGui::TextDisabled("Unstable star — exotic research, catastrophe risk"); break;
                        default: break;
                    }
                }

                // Ships present
                std::vector<std::string> shipsHere;
                for (const auto& sh : gGameState.ships) {
                    if (sh.ownerId == 0 && sh.locationSystemId == gHoveredStarId && !sh.isMoving) {
                        shipsHere.push_back(sh.name);
                    }
                }
                if (!shipsHere.empty()) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "Your ships here:");
                    for (const auto& sname : shipsHere) {
                        ImGui::BulletText("%s", sname.c_str());
                    }
                }

                ImGui::Separator();
                ImGui::TextDisabled("Double-click to enter system view");
                ImGui::EndTooltip();
            }
        }

        // --- Planet tooltip (star system view) ---
        if (gInSystemView && gHoveredPlanetIndex != -1) {
            auto* viewedSys = gGameState.galaxy.findSystemById(gViewedSystemId);
            if (viewedSys && gHoveredPlanetIndex < (int)viewedSys->planets.size()) {
                const auto& pl = viewedSys->planets[gHoveredPlanetIndex];

                ImGui::BeginTooltip();
                ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", pl.name.c_str());
                ImGui::Text("%s  •  %s  •  %s", to_string(pl.size).data(), to_string(pl.type).data(), to_string(pl.richness).data());
                ImGui::Text("Gravity: %s", to_string(pl.gravity).data());

                if (pl.isColonized()) {
                    ImGui::TextColored(ImVec4(0.4f, 0.95f, 0.5f, 1.0f), "Colonized — Pop %.1f / %.0f", pl.population, (float)pl.maxPopulation);
                } else if (pl.canBeColonized()) {
                    ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.6f, 1.0f), "Habitable — ready for colonization");
                } else {
                    ImGui::TextDisabled("Not suitable for colonization");
                }

                ImGui::EndTooltip();
            }
        }

        // --- Ship tooltip (star system view) ---
        if (gInSystemView && gHoveredSystemShipId != -1) {
            for (const auto& sh : gGameState.ships) {
                if (sh.id == gHoveredSystemShipId) {
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ImVec4(0.85f, 0.9f, 1.0f, 1.0f), "%s", sh.name.c_str());
                    if (sh.isFromDesign()) {
                        ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.7f, 1.0f), "Design: %s", sh.designName.c_str());
                    }

                    if (sh.isMoving) {
                        auto* dest = gGameState.galaxy.findSystemById(sh.destinationSystemId);
                        ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.5f, 1.0f), "En route to %s",
                                           dest ? dest->name.c_str() : "Unknown");
                    } else {
                        ImGui::TextDisabled("Stationed in this system");
                    }

                    if (sh.weaponPower > 0) {
                        ImGui::Text("Combat: %d WP / %d Shields", sh.weaponPower, sh.shieldStrength);
                    }

                    ImGui::EndTooltip();
                    break;
                }
            }
        }

        // Top bar (restored)
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(screenW), 46));
        ImGui::Begin("##TopBar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, 3));
        ImGui::TextColored(ImVec4(0.88f, 0.92f, 1.0f, 1.0f), "ORION REBORN");
        ImGui::SameLine();
        ImGui::Text("Turn %d", gGameState.currentTurn);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.35f, 0.45f, 0.55f, 0.9f), "|");

        const auto& plr = gGameState.playerEmpire();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.32f, 0.95f, 0.52f, 1.0f), "BC:%d", plr.treasury);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.42f, 0.70f, 1.0f, 1.0f), "RP:%d", plr.researchPool);
        ImGui::SameLine();
        // Only show player's colonies in the top bar
        size_t playerColCount = 0;
        for (const auto& c : gGameState.colonies) if (c.ownerId == 0) ++playerColCount;
        ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.35f, 1.0f), "Colonies:%zu", playerColCount);

        ImGui::SameLine(ImGui::GetWindowWidth() - 180);
        if (ImGui::Button("End Turn")) {
            gColoniesWithoutProduction = getPlayerColoniesWithoutProduction();

            if (!gColoniesWithoutProduction.empty()) {
                gShowEndTurnConfirmation = true;
            } else {
                processEndOfTurn();
                gShowEventPopup = false;
                gShowTurnReport = true;
            }
        }
        ImGui::PopStyleVar();
        ImGui::End();
        // End Turn confirmation dialog (restored)
        if (gShowEndTurnConfirmation) {
            ImGui::SetNextWindowSize(ImVec2(420, 200), ImGuiCond_Appearing);
            ImGui::Begin("End Turn Warning", &gShowEndTurnConfirmation, ImGuiWindowFlags_NoCollapse);

            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Some of your colonies have no active production project.");

            ImGui::TextWrapped("You can use the Colony Management window to set your 5 sliders and production. This is important for growth and expansion.");

            ImGui::Separator();

            if (!gColoniesWithoutProduction.empty()) {
                ImGui::Text("Colonies without production:");
                for (int idx : gColoniesWithoutProduction) {
                    if (idx < static_cast<int>(gGameState.colonies.size())) {
                        auto* sys = gGameState.galaxy.findSystemById(gGameState.colonies[idx].planetId);
                        ImGui::BulletText("%s", sys ? sys->name.c_str() : "Unknown");
                    }
                }
            }

            ImGui::Separator();

            if (ImGui::Button("End Turn Anyway", ImVec2(180, 0))) {
                gShowEndTurnConfirmation = false;
                processEndOfTurn();

                // Close other transient popups so the report is prominent
                gShowEventPopup = false;
                gShowTurnReport = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(180, 0))) {
                gShowEndTurnConfirmation = false;
            }

            ImGui::End();
        }

        // Ship order mode tooltip (must be inside valid ImGui frame)
//         if (gShipOrderMode && gShipInOrderMode != -1 && gOrderModeHoveredSystem != -1) {
//             orion::Ship* orderingShip = nullptr;
//             for (auto& sh : gGameState.ships) {
//                 if (sh.id == gShipInOrderMode) { orderingShip = &sh; break; }
//             }
//             auto* currentSys = gGameState.galaxy.findSystemById(orderingShip ? orderingShip->locationSystemId : -1);
//             auto* target = gGameState.galaxy.findSystemById(gOrderModeHoveredSystem);

//             if (orderingShip && target && currentSys) {
//                 float dist = GetSystemDistance(currentSys, target);
//                 int eta = GetTravelETA(dist, orderingShip->effectiveSpeed);
//                 bool inRange = (dist <= orderingShip->maxRange + 0.1f);

//                 ImGui::BeginTooltip();
//                 ImGui::TextColored(ImVec4(0.6f, 0.95f, 0.7f, 1.0f), "Send %s to %s",
//                                    orderingShip->name.c_str(), target->name.c_str());

//                 if (inRange) {
//                     ImGui::Text("Distance: %.0f parsecs", dist);
//                     ImGui::TextColored(ImVec4(0.9f, 0.95f, 0.6f, 1.0f), "ETA: %d turns", eta);
//                     ImGui::TextColored(ImVec4(0.5f, 0.95f, 0.6f, 1.0f), "Range OK (%.0f / %.0f)", dist, orderingShip->maxRange);
//                 } else {
//                     ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "OUT OF RANGE");
//                     ImGui::Text("Distance: %.0f parsecs  (max %.0f)", dist, orderingShip->maxRange);
//                     ImGui::TextDisabled("Research better engines or design a ship with longer range.");
//                 }
//                 ImGui::EndTooltip();
//             }
//         }

        // Tooltips for star systems on the main galaxy map
//         if (!gInSystemView && gHoveredStarId != -1) {
//             if (auto* sys = gGameState.galaxy.findSystemById(gHoveredStarId)) {
//                 ImGui::BeginTooltip();
//                 ImGui::TextColored(ImVec4(0.85f, 0.9f, 1.0f, 1.0f), "%s", sys->name.c_str());
//                 ImGui::Text("%zu planets", sys->planets.size());

//                 if (sys->ownerEmpireId == 0) {
//                     ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "Colonized by you");
//                 } else if (sys->ownerEmpireId == -1) {
//                     ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Unexplored");
//                 } else {
                    // Find empire name if possible
//                     std::string ownerName = "Unknown Empire";
//                     for (const auto& emp : gGameState.empires) {
//                         if (emp.id == sys->ownerEmpireId) {
//                             ownerName = emp.name;
//                             break;
//                         }
//                     }
//                     ImGui::Text("Owned by %s", ownerName.c_str());
//                 }

//                 ImGui::Separator();

                // List ships present in this system (Phase 3)
//                 std::vector<std::string> shipsHere;
//                 for (const auto& sh : gGameState.ships) {
//                     if (sh.ownerId == 0 && sh.locationSystemId == gHoveredStarId) {
//                         shipsHere.push_back(sh.name);
//                     }
//                 }
//                 if (!shipsHere.empty()) {
//                     ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "Ships here (%zu):", shipsHere.size());
//                     for (const auto& sname : shipsHere) {
//                         ImGui::BulletText("%s", sname.c_str());
//                     }
//                     ImGui::TextDisabled("Click near star to manage fleet");
//                 }

//                 ImGui::TextDisabled("Double-click to view system");
//                 ImGui::EndTooltip();
//             }
//         }

        // Left info panel
//         ImGui::SetNextWindowPos(ImVec2(10, 58), ImGuiCond_FirstUseEver);
//         ImGui::SetNextWindowSize(ImVec2(248, 205), ImGuiCond_FirstUseEver);
//         ImGui::Begin("Galaxy");
//         ImGui::Text("Systems: %zu   Zoom: %.2fx", gGameState.galaxy.systems.size(), gZoom);
//         if (ImGui::Button("Reset View")) { gZoom = 1.45f; gCameraOffset = {}; }  // Reset to preferred fixed zoom
//         ImGui::Separator();

//         if (auto* selSys = gGameState.galaxy.findSystemById(gGameState.selectedStarId)) {
//             ImGui::TextColored(ImVec4(0.78f, 0.88f, 1.0f, 1.0f), "%s", selSys->name.c_str());
//             ImGui::Text("%zu planets  •  %s", selSys->planets.size(), selSys->ownerEmpireId == 0 ? "Yours" : "Unexplored");

//             if (!selSys->planets.empty()) {
//                 const auto& p0 = selSys->planets[0];
//                 ImGui::Text("%s  %s  %s", to_string(p0.size).data(), to_string(p0.type).data(), to_string(p0.richness).data());
//                 ImGui::Text("Pop %.1f / %d   (%s g)", p0.population, p0.maxPopulation, to_string(p0.gravity).data());
//             }
//             if (selSys->ownerEmpireId != 0) {
//                 ImGui::TextDisabled("Unexplored system");
//                 ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "Send a Colony Ship here to colonize.");
//             } else {
//                 ImGui::TextColored(ImVec4(0.35f, 0.92f, 0.45f, 1.0f), "Colonized by you");

//                 if (ImGui::Button("Manage Colony##open", ImVec2(-1, 0))) {
//                     gSelectedColonyIndex = -1;
//                     for (int i = 0; i < static_cast<int>(gGameState.colonies.size()); ++i) {
//                         if (gGameState.colonies[i].planetId == selSys->starId) {
//                             gSelectedColonyIndex = i;
//                             break;
//                         }
//                     }
//                     gShowColonyWindow = (gSelectedColonyIndex >= 0);
//                 }
//             }
//         } else {
//             ImGui::Text("Click any star to inspect it.");
//         }

//         if (gSelectedShipId != -1) {
//             ImGui::Separator();
//             ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 1.0f), "Ship selected");
//             ImGui::TextDisabled("Right-click a star on the map to send it.");
//         }
//         ImGui::End();

        // Ship command panel
//         if (gSelectedShipId != -1) {
//             ImGui::SetNextWindowPos(ImVec2(10, 275), ImGuiCond_FirstUseEver);
//             ImGui::SetNextWindowSize(ImVec2(248, 115), ImGuiCond_FirstUseEver);
//             ImGui::Begin("Selected Ship");

//             for (auto& sh : gGameState.ships) {
//                 if (sh.id == gSelectedShipId) {
//                     ImGui::Text("%s", sh.name.c_str());
//                     if (sh.isFromDesign()) {
//                         ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "(from design)");
//                     }
//                     auto* loc = gGameState.galaxy.findSystemById(sh.locationSystemId);
//                     ImGui::Text("At: %s", loc ? loc->name.c_str() : "Unknown");

//                     if (sh.isMoving) {
//                         auto* dest = gGameState.galaxy.findSystemById(sh.destinationSystemId);
//                         auto* from = gGameState.galaxy.findSystemById(sh.locationSystemId);
//                         ImGui::Text("Moving to %s (%.0f%%)", dest ? dest->name.c_str() : "?", sh.travelProgress * 100);

//                         if (from && dest) {
//                             float d = GetSystemDistance(from, dest);
//                             float spd = std::max(0.1f, sh.effectiveSpeed);
//                             int etaTurns = (int)ceilf((1.0f - sh.travelProgress) * (1.0f / (spd * 0.26f)));
//                             ImGui::TextColored(ImVec4(0.85f, 0.9f, 0.6f, 1.0f), "  %.0f pc  •  ETA ~%d turns", d, etaTurns);
//                         }
//                     } else {
//                         ImGui::TextDisabled("No orders. Right-click a star to move.");
//                     }

                    // Show combat stats for military ships (Phase 2 depth)
//                     if (sh.type == orion::ShipType::Destroyer || sh.weaponPower > 0 || sh.shieldStrength > 0) {
//                         ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.5f, 1.0f), "Combat: WP %d  |  Shields %d",
//                                            sh.weaponPower, sh.shieldStrength);
//                     }

                    // Player-controlled retreat (Phase 2 combat depth)
//                     if (!sh.isMoving && (sh.type == orion::ShipType::Destroyer || sh.weaponPower > 0)) {
                        // Check if there are enemy ships in the same system
//                         bool inCombat = false;
//                         for (const auto& other : gGameState.ships) {
//                             if (other.ownerId != 0 && other.locationSystemId == sh.locationSystemId) {
//                                 inCombat = true;
//                                 break;
//                             }
//                         }
//                         if (inCombat) {
//                             if (ImGui::Button("Retreat from Combat")) {
                                // Find nearest player-owned system
//                                 auto* current = gGameState.galaxy.findSystemById(sh.locationSystemId);
//                                 int bestTarget = -1;
//                                 float bestDist = 99999.0f;

//                                 for (const auto& sys : gGameState.galaxy.systems) {
//                                     if (sys.ownerEmpireId != 0) continue;
//                                     if (sys.starId == sh.locationSystemId) continue;

//                                     float d = GetSystemDistance(current, &sys);
//                                     if (d < bestDist) {
//                                         bestDist = d;
//                                         bestTarget = sys.starId;
//                                     }
//                                 }

//                                 if (bestTarget != -1) {
//                                     sh.destinationSystemId = bestTarget;
//                                     sh.isMoving = true;
//                                     sh.travelProgress = 0.0f;
//                                 } else {
                                    // Fallback: just stop in place if no safe system found
//                                     sh.isMoving = false;
//                                 }
//                             }
//                             ImGui::SameLine();
//                             ImGui::TextDisabled("(disengage)");
//                         }
//                     }

                    // Colonization action when a colony ship has arrived at a valid system
//                     if (!sh.isMoving && sh.type == orion::ShipType::ColonyShip) {
//                         auto* here = gGameState.galaxy.findSystemById(sh.locationSystemId);
//                         if (here && here->ownerEmpireId != 0) {
//                             bool canColonize = false;
//                             for (const auto& p : here->planets) {
//                                 if (!p.isColonized() && p.canBeColonized()) { canColonize = true; break; }
//                             }
//                             if (canColonize) {
//                                 if (ImGui::Button("Colonize System")) {
                                    // Perform colonization
//                                     for (auto& p : here->planets) {
//                                         if (!p.isColonized() && p.canBeColonized()) {
//                                             float startingPop = 1.2f;

                                            // Apply bonus from the colony ship's design (if any)
//                                             for (const auto& sh : gGameState.ships) {
//                                                 if (sh.id == gSelectedShipId && !sh.designName.empty()) {
//                                                     for (const auto& d : orion::playerDesigns) {
//                                                         if (d.name == sh.designName) {
//                                                             startingPop += d.totalColonyPopBonus * 0.4f;
//                                                             break;
//                                                         }
//                                                     }
//                                                     break;
//                                                 }
//                                             }

//                                             p.ownerEmpireId = 0;
//                                             p.population = startingPop;
//                                             here->ownerEmpireId = 0;

//                                             orion::Colony newCol{};
//                                             newCol.ownerId = 0;
//                                             newCol.population = startingPop;
//                                             newCol.planetId = here->starId;
//                                             gGameState.colonies.push_back(newCol);

                                            // Consume the colony ship
//                                             for (size_t s = 0; s < gGameState.ships.size(); ++s) {
//                                                 if (gGameState.ships[s].id == gSelectedShipId) {
//                                                     gGameState.ships.erase(gGameState.ships.begin() + s);
//                                                     break;
//                                                 }
//                                             }
//                                             gSelectedShipId = -1;
//                                             break;
//                                         }
//                                     }
//                                 }
//                                 ImGui::SameLine();
//                                 ImGui::TextDisabled("(uses the ship)");
//                             }
//                         }
//                     }

//                     if (ImGui::Button("Cancel Orders")) {
//                         sh.isMoving = false;
//                         sh.destinationSystemId = -1;
//                         sh.travelProgress = 0.0f;
//                     }
//                     ImGui::SameLine();
//                     if (ImGui::Button("Send To")) {
//                         gShipInOrderMode = sh.id;
//                         gShipOrderMode = true;
//                         gSelectedShipId = sh.id;
//                         gInSystemView = false;   // go back to main map
//                         gShowShipsWindow = false;
//                     }
//                     ImGui::SameLine();
//                     if (ImGui::Button("Deselect")) gSelectedShipId = -1;
//                     break;
//                 }
//             }
//             ImGui::End();
//         }

        // ==================== Star System View UI (when active) ====================
//         if (gInSystemView) {
//             auto* viewedSys = gGameState.galaxy.findSystemById(gViewedSystemId);
//             if (viewedSys) {
//                 ImGui::SetNextWindowPos(ImVec2(40, 50), ImGuiCond_FirstUseEver);
//                 ImGui::SetNextWindowSize(ImVec2(380, 480), ImGuiCond_FirstUseEver);
//                 ImGui::Begin("Star System", nullptr, ImGuiWindowFlags_NoCollapse);

//                 ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s System", viewedSys->name.c_str());
//                 ImGui::Text("%zu planets", viewedSys->planets.size());
//                 ImGui::Separator();

                // Check if we have a colony ship in this system (for colonize buttons)
//                 bool hasColonyShip = false;
//                 for (const auto& sh : gGameState.ships) {
//                     if (sh.ownerId == 0 && sh.locationSystemId == gViewedSystemId && sh.type == orion::ShipType::ColonyShip) {
//                         hasColonyShip = true;
//                         break;
//                     }
//                 }

//                 for (size_t i = 0; i < viewedSys->planets.size(); ++i) {
//                     auto& pl = viewedSys->planets[i];
//                     ImGui::PushID((int)i);

//                     bool isSel = ((int)i == gSelectedPlanetIndex);
//                     if (isSel) {
//                         ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.85f, 1.0f, 1.0f));
//                     }

//                     if (ImGui::Selectable(pl.name.c_str(), isSel)) {
//                         gSelectedPlanetIndex = (int)i;
//                     }

//                     ImGui::Text("  %s • %s • %s • %s g",
//                                 to_string(pl.size).data(),
//                                 to_string(pl.type).data(),
//                                 to_string(pl.richness).data(),
//                                 to_string(pl.gravity).data());

//                     if (pl.isColonized()) {
//                         ImGui::TextColored(ImVec4(0.4f, 0.95f, 0.5f, 1.0f), "  COLONIZED (Pop %.1f)", pl.population);
//                         if (pl.ownerEmpireId == 0) {
//                             ImGui::SameLine();
//                             if (ImGui::SmallButton("Manage")) {
//                                 gSelectedPlanetIndex = (int)i;
//                                 gSelectedColonyIndex = -1;
//                                 for (int c = 0; c < static_cast<int>(gGameState.colonies.size()); ++c) {
//                                     if (gGameState.colonies[c].planetId == viewedSys->starId) {
//                                         gSelectedColonyIndex = c;
//                                         break;
//                                     }
//                                 }
//                                 gShowColonyWindow = (gSelectedColonyIndex >= 0);
//                             }
//                         }
//                     } else if (pl.canBeColonized()) {
//                         if (hasColonyShip) {
//                             ImGui::TextColored(ImVec4(0.5f, 0.95f, 0.6f, 1.0f), "  Colonizable");
//                             ImGui::SameLine();
//                             if (ImGui::SmallButton("Colonize")) {
                                // Perform colonization using first available colony ship in system
//                                 for (size_t s = 0; s < gGameState.ships.size(); ++s) {
//                                     auto& sh = gGameState.ships[s];
//                                     if (sh.locationSystemId == gViewedSystemId && sh.type == orion::ShipType::ColonyShip && sh.ownerId == 0) {
//                                         pl.ownerEmpireId = 0;
//                                         pl.population = 1.2f;
//                                         viewedSys->ownerEmpireId = 0;

//                                         orion::Colony newCol{};
//                                         newCol.ownerId = 0;
//                                         newCol.population = 1.2f;
//                                         newCol.planetId = gViewedSystemId;
//                                         gGameState.colonies.push_back(newCol);

//                                         gGameState.ships.erase(gGameState.ships.begin() + s);
//                                         break;
//                                     }
//                                 }
//                             }
//                         } else {
//                             ImGui::TextDisabled("  Uncolonized - Send a colony ship here");
//                         }
//                     } else {
//                         ImGui::TextDisabled("  Hostile / unsuitable");
//                     }

//                     if (isSel) {
//                         ImGui::PopStyleColor();
//                     }

//                     ImGui::Separator();
//                     ImGui::PopID();
//                 }

//                 ImGui::Separator();
        // ==================== Star System Menu (when inside a system) ====================
        // Hide when drilled into a planet so the immersive view + Planet Surface panel are the focus.
        if (gInSystemView && !gInPlanetView) {
            auto* viewedSys = gGameState.galaxy.findSystemById(gViewedSystemId);
            if (viewedSys) {
                ImGui::SetNextWindowPos(ImVec2(30, 40), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(360, 420), ImGuiCond_FirstUseEver);
                ImGui::Begin("Star System", nullptr, ImGuiWindowFlags_NoCollapse);

                ImGui::TextColored(ImVec4(0.75f, 0.85f, 1.0f, 1.0f), "%s System", viewedSys->name.c_str());
                ImGui::Text("%zu planets  •  Owner: %s",
                           viewedSys->planets.size(),
                           (viewedSys->ownerEmpireId == 0) ? "You" : "Unclaimed / Other");

                if (viewedSys->specialStatus != orion::SystemSpecial::None) {
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "★ Special: %s", to_string(viewedSys->specialStatus).data());
                }

                ImGui::Separator();

                ImGui::Text("Planets:");
                for (size_t i = 0; i < viewedSys->planets.size(); ++i) {
                    const auto& pl = viewedSys->planets[i];
                    ImGui::PushID((int)i);

                    bool isSelected = ((int)i == gSelectedPlanetIndex);
                    if (isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.9f, 1.0f, 1.0f));
                    }

                    std::string label = pl.name + "  (" + std::string(to_string(pl.size)) + " " + std::string(to_string(pl.type)) + ")";
                    if (ImGui::Selectable(label.c_str(), isSelected)) {
                        gSelectedPlanetIndex = (int)i;
                    }

                    if (isSelected) {
                        ImGui::PopStyleColor();
                    }

                    if (pl.isColonized()) {
                        ImGui::TextColored(ImVec4(0.4f, 0.95f, 0.5f, 1.0f), "   Colonized (Pop: %.1f)", pl.population);
                    } else if (pl.canBeColonized()) {
                        ImGui::TextDisabled("   Uncolonized - send a colony ship");
                    } else {
                        ImGui::TextDisabled("   Hostile environment");
                    }

                    ImGui::PopID();
                }

                ImGui::Separator();
                ImGui::Spacing();

                // === Colonization Menu (explicit confirmation only) ===
                bool hasColonyShip = false;
                for (const auto& sh : gGameState.ships) {
                    if (sh.ownerId == 0 && sh.locationSystemId == gViewedSystemId && sh.type == orion::ShipType::ColonyShip) {
                        hasColonyShip = true;
                        break;
                    }
                }

                std::vector<int> colonizableIndices;
                for (size_t i = 0; i < viewedSys->planets.size(); ++i) {
                    const auto& pl = viewedSys->planets[i];
                    if (!pl.isColonized() && pl.canBeColonized()) {
                        colonizableIndices.push_back((int)i);
                    }
                }

                if (hasColonyShip && !colonizableIndices.empty()) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.5f, 0.95f, 0.6f, 1.0f), "Colonization Opportunities");
                    ImGui::TextDisabled("Select a planet and confirm to colonize (consumes 1 colony ship).");

                    for (int idx : colonizableIndices) {
                        const auto& pl = viewedSys->planets[idx];
                        ImGui::PushID(idx);

                        if (ImGui::Button(("Colonize " + pl.name).c_str(), ImVec2(-1, 0))) {
                            // Perform colonization immediately on confirmation
                            for (size_t s = 0; s < gGameState.ships.size(); ++s) {
                                auto& sh = gGameState.ships[s];
                                if (sh.locationSystemId == gViewedSystemId && sh.type == orion::ShipType::ColonyShip && sh.ownerId == 0) {
                                    auto& targetPlanet = viewedSys->planets[idx];
                                    targetPlanet.ownerEmpireId = 0;
                                    targetPlanet.population = 1.2f;
                                    viewedSys->ownerEmpireId = 0;

                                    orion::Colony newCol{};
                                    newCol.ownerId = 0;
                                    newCol.population = 1.2f;
                                    newCol.planetId = gViewedSystemId;
                                    newCol.maxPopulation = static_cast<float>(targetPlanet.maxPopulation);
                                    gGameState.colonies.push_back(newCol);

                                    gGameState.ships.erase(gGameState.ships.begin() + s);
                                    PlaySound(sfxColonize);

                                    gTurnReportMessages.push_back("Colonized " + targetPlanet.name + "!");
                                    break;
                                }
                            }
                        }
                        ImGui::PopID();
                    }
                }

                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Return to Galaxy Map", ImVec2(-1, 0))) {
                    gInSystemView = false;
                    gInPlanetView = false;
                    gSelectedPlanetIndex = -1;
                }

                ImGui::End();
            }
        }

        // ==================== PLANET VIEW UI (immersive colony management) ====================
        if (gInPlanetView) {
            auto* viewedSys = gGameState.galaxy.findSystemById(gViewedPlanetSystemId);
            if (viewedSys && gViewedPlanetIndex >= 0 &&
                gViewedPlanetIndex < static_cast<int>(viewedSys->planets.size())) {

                const auto& planet = viewedSys->planets[gViewedPlanetIndex];

                // Find matching colony
                orion::Colony* colonyPtr = nullptr;
                int colonyIdx = -1;
                for (int c = 0; c < static_cast<int>(gGameState.colonies.size()); ++c) {
                    if (gGameState.colonies[c].planetId == viewedSys->starId) {
                        colonyPtr = &gGameState.colonies[c];
                        colonyIdx = c;
                        break;
                    }
                }

                ImGui::SetNextWindowPos(ImVec2(30, 40), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(420, 480), ImGuiCond_FirstUseEver);
                ImGui::Begin("Planet Surface", nullptr, ImGuiWindowFlags_NoCollapse);

                ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "CLOSE-UP VIEW - Planet slowly rotating on axis");
                ImGui::TextWrapped("The large sphere is the planet you clicked. Blue = oceans/water; green/brown = continents/land (Terran/Gaia). Surface features drift as the globe spins under fixed lighting. Not a star + orbiting body.");
                ImGui::Separator();

                ImGui::TextColored(ImVec4(0.9f, 0.95f, 1.0f, 1.0f), "%s", planet.name.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("(%s • %s)", to_string(planet.type).data(), to_string(planet.size).data());

                if (viewedSys->specialStatus != orion::SystemSpecial::None) {
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "★ %s", to_string(viewedSys->specialStatus).data());
                }

                ImGui::Separator();

                if (colonyPtr) {
                    ImGui::Text("Population: %.1f / %.0f million", colonyPtr->population, colonyPtr->maxPopulation);

                    // Quick growth preview
                    float growth = (colonyPtr->foodNet > 0.8f) ? 0.11f : (colonyPtr->foodNet > 0.2f ? 0.05f : 0.01f);
                    if (planet.traits & static_cast<uint32_t>(orion::PlanetTrait::Fertile)) growth *= 1.25f;
                    ImGui::TextColored(ImVec4(0.5f, 0.95f, 0.6f, 1.0f), "Est. growth: +%.2fM / turn", growth);

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.85f, 0.92f, 1.0f, 1.0f), "Output this turn");

                    // Recalculate for live numbers
                    float techB = gGameState.technology.getIndustryBonus();
                    float researchB = gGameState.technology.getResearchBonus();
                    float ownerMod = 1.0f;
                    for (const auto& e : gGameState.empires) if (e.id == colonyPtr->ownerId) { ownerMod = e.productionMod; break; }

                    colonyPtr->recalculateOutputs(planet.size, planet.type, planet.richness, planet.traits,
                                                  colonyPtr->maxPopulation, techB, ownerMod);

                    ImGui::Text("Industry: %.1f   Research: %.1f   Food Net: %.1f",
                                colonyPtr->productionOutput, colonyPtr->researchOutput * researchB, colonyPtr->foodNet);

                    ImGui::Separator();
                    ImGui::Text("Production Project");

                    // Project selection buttons (reused from old logic)
                    for (int p = 0; p < 5; ++p) {
                        const char* proj = orion::Colony::PROJECTS[p];
                        int cost = orion::Colony::PROJECT_COSTS[p];

                        bool isCurrent = (colonyPtr->currentProject == proj);

                        if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.4f, 1.0f));

                        if (ImGui::Button(proj, ImVec2(-1, 0))) {
                            colonyPtr->currentProject = proj;
                            colonyPtr->projectCost = cost;
                            colonyPtr->projectProgress = 0.0f;
                            PlaySound(sfxClick);
                        }

                        if (isCurrent) {
                            ImGui::PopStyleColor();
                            if (cost > 0) {
                                float pct = colonyPtr->projectCost > 0 ?
                                    (colonyPtr->projectProgress / colonyPtr->projectCost) : 0.0f;
                                ImGui::ProgressBar(std::clamp(pct, 0.0f, 1.0f), ImVec2(-1, 0),
                                                   TextFormat("%.0f / %d", colonyPtr->projectProgress, cost));
                            }
                        }
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("Click a project above to queue it. Production applies on End Turn.");
                } else {
                    ImGui::TextDisabled("Uncolonized planet");
                    ImGui::Text("Send a colony ship to settle here.");
                }

                ImGui::Spacing();
                if (ImGui::Button("Return to Star System", ImVec2(-1, 0))) {
                    gInPlanetView = false;
                    // Keep the system view open so user stays in context
                    gSelectedPlanetIndex = gViewedPlanetIndex;
                }

                ImGui::End();
            }
        }

        // Right empire panel (restored)
        if (gShowDebugPanel) {
            ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() - 262, 58), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(248, 200), ImGuiCond_FirstUseEver);
            ImGui::Begin("Empire", nullptr, ImGuiWindowFlags_NoCollapse);

            const auto& _plr = gGameState.playerEmpire();
            ImGui::TextColored(ImVec4(0.95f, 0.9f, 0.55f, 1.0f), "%s", _plr.name.c_str());
            ImGui::Separator();
            ImGui::Text("Treasury: %d BC", _plr.treasury);
            ImGui::Text("Research: %d RP", _plr.researchPool);
            // Count only the player's own colonies
            size_t playerColonyCount = 0;
            for (const auto& c : gGameState.colonies)
                if (c.ownerId == 0) ++playerColonyCount;
            ImGui::Text("Your Colonies: %zu", playerColonyCount);
            ImGui::Text("Current Turn: %d", gGameState.currentTurn);
            ImGui::Separator();

            // Racial + tech bonuses
            ImGui::Text("Racial Mods:");
            ImGui::Text("  Growth: %.2fx  Research: %.2fx", _plr.populationGrowthMod, _plr.researchMod);
            ImGui::Text("  Production: %.2fx", _plr.productionMod);

            float resTech = gGameState.technology.getResearchBonus();
            float indTech = gGameState.technology.getIndustryBonus();
            float spdTech = gGameState.technology.getShipSpeedMultiplier();
            ImGui::Text("Tech Bonuses:");
            ImGui::Text("  Research: %.2fx  Industry: %.2fx", resTech, indTech);
            ImGui::Text("  Ship Speed: %.2fx", spdTech);
            ImGui::Separator();

            if (ImGui::Button("Manage Ships (F3)")) {
                gShowShipsWindow = true;
            }

            if (ImGui::Button("Save Game")) {
                std::filesystem::create_directories("saves");
                if (saveGame(gGameState, "saves/orion_save.json")) {
                    std::cout << "Game saved successfully.\n";
                } else {
                    std::cout << "Failed to save game.\n";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Load Game")) {
                if (loadGame(gGameState, "saves/orion_save.json")) {
                    std::cout << "Game loaded.\n";
                    gSelectedShipId = -1;
                    gSelectedPlanetIndex = -1;
                    gInSystemView = false;
                } else {
                    std::cout << "Failed to load game.\n";
                }
            }

            ImGui::TextWrapped("F1 = toggle this panel. LMB = select star.");

            ImGui::End();
        }
        // Colony Management window (restored) — hide when we're in the immersive planet view
        if (!gInPlanetView) {
            DrawColonyManagementWindow();
        }

        // Dedicated per-colony buildings screen
//         DrawColonyBuildingsWindow();

        // ==================== Tech Choice Window (Phase 2) ====================
//         if (gShowTechChoice) {
//             ImGui::SetNextWindowSize(ImVec2(420, 260), ImGuiCond_FirstUseEver);
//             ImGui::Begin("Research Complete - Choose One", &gShowTechChoice);

//             ImGui::Text("Your scientists have made a breakthrough!");
//             ImGui::TextDisabled("Researching a tech costs 120 RP from your pool.");
//             ImGui::Separator();

//             for (int techIdx : gAvailableTechChoices) {
//                 if (techIdx < 0 || techIdx >= static_cast<int>(orion::TECH_TREE.size())) continue;
//                 const auto& tech = orion::TECH_TREE[techIdx];

                // Show concrete gameplay effect
//                 std::string effect = "Improves your empire in this area.";
//                 switch (techIdx) {
//                     case 0: effect = "+15% research output from colonies."; break;
//                     case 1: effect = "Better targeting (future combat bonus)."; break;
//                     case 2: effect = "+15% industry output & cheaper ships."; break;
//                     case 3: effect = "Unlocks Large hulls + strong factory bonus."; break;
//                     case 4: case 5: effect = "Stronger ship and planetary defenses."; break;
//                     case 6: effect = "Slightly improves planet habitability."; break;
//                     case 7: effect = "Better terraforming potential later."; break;
//                     case 8: effect = "+38% ship speed and +50 pc jump range."; break;
//                     case 9: effect = "Major speed (+65%) and huge range bonus."; break;
//                     case 10: effect = "Basic ship weapons available in designer."; break;
//                     case 11: effect = "Unlocks Particle Beams in the Ship Designer."; break;
//                 }

//                 ImGui::PushID(techIdx);
//                 if (ImGui::Button(tech.name.c_str())) {
//                     gGameState.technology.researchedTechIndices.push_back(techIdx);
//                     auto& emp = gGameState.playerEmpire();
//                     emp.researchPool = std::max(0, emp.researchPool - 120);

//                     int cat = static_cast<int>(tech.category);
//                     gGameState.technology.level[cat]++;

//                     gShowTechChoice = false;
//                     gAvailableTechChoices.clear();
//                 }
//                 ImGui::SameLine();
//                 ImGui::TextDisabled("[%s]", to_string(tech.category).data());
//                 ImGui::TextWrapped("%s", effect.c_str());
//                 ImGui::TextDisabled("%s", tech.description.c_str());
//                 ImGui::Separator();
//                 ImGui::PopID();
//             }

//             if (ImGui::Button("Research Later")) {
//                 gShowTechChoice = false;
//             }

//             ImGui::End();
//         }

        // ==================== Phase 3: Random Event Popup ====================
        // // // if (gShowEventPopup) {
// // //             ImGui::SetNextWindowSize(ImVec2(380, 160), ImGuiCond_FirstUseEver);
// // //             ImGui::Begin(gCurrentEvent.title.c_str(), &gShowEventPopup, ImGuiWindowFlags_NoCollapse);
// // // 
// // //             ImGui::TextWrapped("%s", gCurrentEvent.description.c_str());
// // //             ImGui::Separator();
// // // 
// // //             if (gCurrentEvent.isGood) {
// // //                 ImGui::TextColored(ImVec4(0.5f, 0.95f, 0.6f, 1.0f), "This is a positive event.");
// // //             } else {
// // //                 ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.5f, 1.0f), "This is a negative event.");
// // //             }
// // // 
// // //             if (ImGui::Button("Understood", ImVec2(-1, 0))) {
// // //                 gShowEventPopup = false;
// // //             }
// // // 
// // //             ImGui::End();
// // //         }

// Forward declaration (definition moved to file scope below to avoid scope issues)
void DrawEndOfTurnReportWindow();

// ==================== Phase 3: Leaders Window ====================
        // // if (gShowLeadersWindow) {
// //             ImGui::SetNextWindowSize(ImVec2(420, 280), ImGuiCond_FirstUseEver);
// //             ImGui::Begin("Leaders", &gShowLeadersWindow);
// // 
// //             ImGui::TextColored(ImVec4(0.9f, 0.85f, 0.6f, 1.0f), "Available Leaders");
// //             ImGui::Separator();
// // 
// //             for (size_t i = 0; i < gLeaders.size(); ++i) {
// //                 auto& leader = gLeaders[i];
// //                 ImGui::PushID((int)i);
// // 
// //                 ImGui::Text("%s (%s)", leader.name.c_str(), leader.title.c_str());
// //                 ImGui::TextDisabled("%s", leader.bonusDesc.c_str());
// // 
// //                 if (leader.assignedTo == -1) {
// //                     if (ImGui::Button("Assign")) {
// //                         // Simple: assign to first colony or unassign logic
// //                         if (!gGameState.colonies.empty()) {
// //                             leader.assignedTo = 0; // assign to first colony for demo
// //                         }
// //                     }
// //                 } else {
// //                     if (ImGui::Button("Unassign")) {
// //                         leader.assignedTo = -1;
// //                     }
// //                     ImGui::SameLine();
// //                     ImGui::TextDisabled("(Assigned)");
// //                 }
// //                 ImGui::Separator();
// //                 ImGui::PopID();
// //             }
// // 
// //             ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "Note: Full assignment to specific colonies/fleets coming in later updates.");
// //             ImGui::End();
// //         }

        // ==================== Ships Window (restored) ====================
        if (gShowShipsWindow) {
            ImGui::SetNextWindowSize(ImVec2(520, 380), ImGuiCond_FirstUseEver);
            ImGui::Begin("Ships", &gShowShipsWindow);

            ImGui::TextColored(ImVec4(0.85f, 0.9f, 1.0f, 1.0f), "Your Ships");
            ImGui::Separator();

            bool hasShips = false;
            for (auto& sh : gGameState.ships) {
                if (sh.ownerId != 0) continue;
                hasShips = true;

                ImGui::PushID(sh.id);

                bool isSelected = (sh.id == gSelectedShipId);
                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.95f, 0.6f, 1.0f));
                }

                ImGui::Text("%s", sh.name.c_str());
                if (sh.isFromDesign()) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.7f, 1.0f), "[%s]", sh.designName.c_str());
                }

                auto* locSys = gGameState.galaxy.findSystemById(sh.locationSystemId);
                ImGui::Text("  Location: %s", locSys ? locSys->name.c_str() : "Unknown");

                if (sh.isMoving) {
                    auto* destSys = gGameState.galaxy.findSystemById(sh.destinationSystemId);
                    float spd = std::max(0.1f, sh.effectiveSpeed);
                    int eta = sh.travelProgress >= 1.0f ? 0 : (int)ceilf((1.0f - sh.travelProgress) * (1.0f / (spd * 0.26f)));
                    ImGui::TextColored(ImVec4(0.9f, 0.85f, 0.5f, 1.0f), "  En route to %s (ETA: %d turns)",
                                       destSys ? destSys->name.c_str() : "?", eta);
                } else {
                    ImGui::TextDisabled("  Idle - Right-click a star on the map to move");
                }

                if (sh.weaponPower > 0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.5f, 1.0f), "  Combat: %d WP / %d Shields",
                                       sh.weaponPower, sh.shieldStrength);
                }

                if (isSelected) ImGui::PopStyleColor();

                if (ImGui::Button("Select")) {
                    gSelectedShipId = sh.id;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel Orders")) {
                    sh.isMoving = false;
                    sh.destinationSystemId = -1;
                    sh.travelProgress = 0.0f;
                }

                ImGui::SameLine();
                if (ImGui::Button("Send To")) {
                    gShipInOrderMode = sh.id;
                    gShipOrderMode = true;
                    gShowShipsWindow = false;
                    gInSystemView = false;
                    gSelectedShipId = sh.id;
                }

                ImGui::Separator();
                ImGui::PopID();
            }

            if (!hasShips) {
                ImGui::TextDisabled("You have no ships yet. Build some in your colonies.");
            }

            ImGui::End();
        }
        // ==================== Ship Designer (Phase 2 - Expanded) ====================
//         if (gShowShipDesigner) {
//             ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
//             ImGui::Begin("Ship Designer", &gShowShipDesigner);

//             ImGui::TextColored(ImVec4(0.85f, 0.9f, 1.0f, 1.0f), "Ship Designer - Phase 2");
//             ImGui::Separator();

            // Current working design (simple local state for the window)
//             static orion::ShipDesign currentDesign;

            // Hull selection
//             ImGui::Text("Hull");
//             int hullIdx = static_cast<int>(currentDesign.hull);
//             if (ImGui::Combo("Hull Size", &hullIdx, "Small (Frigate)\0Medium (Cruiser)\0Large (Battleship)\0")) {
//                 currentDesign.hull = static_cast<orion::HullSize>(hullIdx);
//                 currentDesign.recalculateStats();
//             }

//             const auto& hullData = orion::HULLS[hullIdx];
//             ImGui::Text("Space: %d / %d   Power: %d / %d",
//                         currentDesign.totalSpaceUsed, hullData.baseSpace,
//                         currentDesign.totalPowerUsed, hullData.basePower);

//             if (!currentDesign.isValid()) {
//                 ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "OVER CAPACITY!");
//             }

//             ImGui::Separator();

            // Add components by category
//             ImGui::Text("Add Components");

//             for (int cat = 0; cat < static_cast<int>(orion::ComponentCategory::Count); ++cat) {
//                 auto category = static_cast<orion::ComponentCategory>(cat);
//                 const char* catName = (category == orion::ComponentCategory::Engine)  ? "Engines" :
//                                       (category == orion::ComponentCategory::Weapon)  ? "Weapons" :
//                                       (category == orion::ComponentCategory::Shield)  ? "Shields" : "Special";

//                 if (ImGui::CollapsingHeader(catName)) {
//                     for (int i = 0; i < static_cast<int>(orion::AVAILABLE_COMPONENTS.size()); ++i) {
//                         const auto& comp = orion::AVAILABLE_COMPONENTS[i];
//                         if (comp.category == category) {
                            // Expanded tech gating for Phase 2
//                             bool unlocked = true;

                            // Engines
//                             if (comp.name == "Improved Engines" && !gGameState.technology.hasBetterEngines()) unlocked = false;
//                             if (comp.name == "Fusion Engines" && !gGameState.technology.hasTech(9)) unlocked = false;

                            // Weapons
//                             if (comp.name == "Mass Driver" && !gGameState.technology.hasTech(10)) unlocked = false;
//                             if (comp.name == "Particle Cannon" && !gGameState.technology.hasParticleBeams()) unlocked = false;

                            // Shields
//                             if (comp.name == "Deflector Shields" && !gGameState.technology.hasTech(4)) unlocked = false;
//                             if (comp.name == "Energy Shields" && !gGameState.technology.hasTech(5)) unlocked = false;

                            // Special
//                             if (comp.name == "Improved Colony Pod" && !gGameState.technology.hasTech(6)) unlocked = false;
//                             if (comp.name == "Advanced Scanner" && !gGameState.technology.hasTech(0)) unlocked = false; // Needs Computers

//                             if (!unlocked) {
//                                 ImGui::TextDisabled("%s (locked by tech)", comp.name.c_str());
//                                 continue;
//                             }

//                             char label[128];
//                             snprintf(label, sizeof(label), "%s (%dsp %dpw)", comp.name.c_str(), comp.spaceCost, comp.powerCost);

//                             if (ImGui::Button(label)) {
//                                 currentDesign.componentIndices.push_back(i);
//                                 currentDesign.recalculateStats();
//                             }
//                             ImGui::SameLine();
//                             ImGui::TextDisabled("%s", comp.description.c_str());
//                         }
//                     }
//                 }
//             }

//             ImGui::Separator();

            // Current design components
//             ImGui::Text("Current Design Components");
//             for (size_t i = 0; i < currentDesign.componentIndices.size(); ++i) {
//                 int idx = currentDesign.componentIndices[i];
//                 if (idx >= 0 && idx < static_cast<int>(orion::AVAILABLE_COMPONENTS.size())) {
//                     ImGui::Text("- %s", orion::AVAILABLE_COMPONENTS[idx].name.c_str());
//                     ImGui::SameLine();
//                     if (ImGui::SmallButton(("Remove##" + std::to_string(i)).c_str())) {
//                         currentDesign.componentIndices.erase(currentDesign.componentIndices.begin() + i);
//                         currentDesign.recalculateStats();
//                         break;
//                     }
//                 }
//             }

            // Show additional design stats (Phase 2)
//             if (currentDesign.totalColonyPopBonus > 0 ||
//                 currentDesign.totalScannerBonus > 0.01f ||
//                 currentDesign.totalWeaponPower > 0 ||
//                 currentDesign.totalShieldStrength > 0)
//             {
//                 ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "Additional Stats:");
//                 if (currentDesign.totalColonyPopBonus > 0)
//                     ImGui::Text("  +%d starting pop on colonization", currentDesign.totalColonyPopBonus);
//                 if (currentDesign.totalScannerBonus > 0.01f)
//                     ImGui::Text("  Scanner: +%.1f", currentDesign.totalScannerBonus);
//                 if (currentDesign.totalWeaponPower > 0)
//                     ImGui::Text("  Weapon Power: %d", currentDesign.totalWeaponPower);
//                 if (currentDesign.totalShieldStrength > 0)
//                     ImGui::Text("  Shield Strength: %d", currentDesign.totalShieldStrength);
//             }

//             ImGui::Separator();

            // Name + Save
//             static char designName[64] = "My New Design";
//             ImGui::InputText("Design Name", designName, sizeof(designName));

//             if (ImGui::Button("Save Design") && currentDesign.isValid()) {
//                 currentDesign.name = designName;
//                 currentDesign.recalculateStats();
//                 orion::playerDesigns.push_back(currentDesign);

                // Reset for next design
//                 currentDesign = orion::ShipDesign{};
//                 currentDesign.recalculateStats();
//             }

//             ImGui::Separator();

            // Saved designs list
//             ImGui::Text("Saved Designs (%zu)", orion::playerDesigns.size());
//             for (size_t i = 0; i < orion::playerDesigns.size(); ++i) {
//                 auto& d = orion::playerDesigns[i];
//                 ImGui::Text("%s (%s) - Cost: %d", d.name.c_str(),
//                             orion::HULLS[static_cast<int>(d.hull)].name.c_str(), d.buildCost);
//             }

//             ImGui::End();
//         }

        // Footer
//         ImGui::SetNextWindowPos(ImVec2(0, GetScreenHeight() - 24));
//         ImGui::SetNextWindowSize(ImVec2(static_cast<float>(GetScreenWidth()), 24));
//         ImGui::Begin("##Footer", nullptr,
//             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
//             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
//         ImGui::TextDisabled("Orion Reborn • Phase 2 • C++23 + raylib 6.0 + ImGui (docking) • Procedural only • %zu stars",
//                             gGameState.galaxy.systems.size());
//         ImGui::End();

        rlImGuiEnd();
        EndDrawing();
    }

    // Clean shutdown
    UnloadSound(sfxClick);
    UnloadSound(sfxEndTurn);
    UnloadSound(sfxShipOrder);
    UnloadSound(sfxColonize);
    UnloadSound(sfxEvent);

    // Unload planet textures
    for (int ti = 0; ti < 9; ++ti) {
        for (int v = 0; v < 2; ++v) {
            if (IsTextureValid(gPlanetTextures[ti][v])) {
                UnloadTexture(gPlanetTextures[ti][v]);
            }
        }
    }

    CloseAudioDevice();
    rlImGuiShutdown();
    CloseWindow();

    return 0;
}

// =============================================================================
// Extracted Turn Report drawing function (used by both game and test harness)
// =============================================================================
void DrawEndOfTurnReportWindow() {
    if (!gShowTurnReport) return;

    float reportWidth = 620.0f;
    float reportHeight = 420.0f;

    ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() * 0.5f - reportWidth * 0.5f,
                                   GetScreenHeight() * 0.25f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(reportWidth, reportHeight), ImGuiCond_Appearing);

    ImGui::Begin("End of Turn Report", &gShowTurnReport,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    ImGui::TextColored(ImVec4(0.9f, 0.92f, 1.0f, 1.0f), "Turn %d Report", gGameState.currentTurn);
    ImGui::Separator();

    // Summary
    ImGui::TextColored(ImVec4(0.85f, 0.9f, 1.0f, 1.0f), "Summary");
    ImGui::BulletText("Treasury: %d BC", gGameState.playerEmpire().treasury);
    ImGui::BulletText("Research: %d RP", gGameState.playerEmpire().researchPool);
    ImGui::BulletText("Colonies: %zu", gGameState.colonies.size());

    ImGui::Separator();

    // What happened this turn
    ImGui::TextColored(ImVec4(0.85f, 0.9f, 1.0f, 1.0f), "What happened this turn");
    if (gTurnReportMessages.empty()) {
        ImGui::TextDisabled("No major events this turn.");
    } else {
        for (const auto& msg : gTurnReportMessages) {
            ImGui::BulletText("%s", msg.c_str());
        }
    }

    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));

    if (ImGui::Button("Close Report", ImVec2(-1, 36))) {
        gShowTurnReport = false;
    }

    ImGui::End();
}


