# Planet SVGs for Orion Reborn

This directory contains vector planet artwork (source SVGs) and rasterized PNG textures used at runtime.

## Generation
- `generate_planets.py` creates 18 SVGs (2 artistic variants for each of the 9 `PlanetType` values).
- Run it with `python3 generate_planets.py` to regenerate SVGs if editing the generator.
- PNGs are produced at 512x512 via `rsvg-convert` (librsvg) for high-quality scaling in raylib:
  ```bash
  mkdir -p textures
  for f in planet_*.svg; do
    base=$(basename "$f" .svg)
    rsvg-convert -w 512 -h 512 "$f" -o "textures/${base}.png"
  done
  ```

## Planet Types (from src/core/Enums.hpp)
- Radiated (cracked, lava-glow, craters)
- Barren (cratered rock)
- Desert / Arid / Steppe (dunes, arid patches)
- Swamp (murky vegetation)
- Ocean (islands + sea)
- Terran (continents, Earth-like)
- Gaia (lush vibrant life world)

## Integration
- Loaded at startup via `LoadPlanetTextures()` (robust search paths mirroring fonts).
- `GetPlanetTexture(planet)` selects stable variant by name+props.
- `DrawTexturedPlanet(...)` used in:
  - Star system view (small orbiting planets, slow rotation)
  - Detailed planet view (large 135px radius, with overlays for atmosphere, rotating city lights, special FX)
- Fallback to colored circle + highlight if texture missing.
- Textures unloaded on shutdown.

SVGs are committed as the source of truth for planet visuals. PNGs are the runtime assets (also committed for convenience / reproducibility).

To add more variants: extend the generator and re-rasterize.
