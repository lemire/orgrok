# Orion Reborn

A modern spiritual successor / clone of **Master of Orion (1993)** — the game that defined the 4X genre.

Built with **C++23**, **raylib 6.0**, and **Dear ImGui** (via rlImGui). No external asset files are used; everything is procedurally drawn with raylib primitives.

## Current Status: Phase 1 Foundation (Complete)

You can:

- Generate a seeded 2D galaxy with 50+ star systems
- Each system has 1–5 planets with authentic MoO attributes (size, type, richness, gravity, occasional special traits)
- Click stars to inspect them
- Colonize new systems with the "Colonize" button
- Press **End Turn** to watch population grow and your treasury/research increase
- Pan with WASD/arrows, zoom with the mouse wheel
- Full Dear ImGui demo available for inspection/debug (press **D**)

The game already captures the addictive "one more turn" loop and the elegant information-dense UI feel of the original.

## Building (Zero Dependencies — Everything Fetched by CPM)

```bash
cd orion-reborn
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

Then run:

```bash
./bin/orion-reborn
```

The first configure will automatically download:
- raylib 6.0
- Dear ImGui (docking branch)
- rlImGui

No system packages or manual cloning required.

Tested on macOS (Apple Silicon + Intel). Should work on Linux and Windows with a recent C++23-capable compiler.

## Controls (Phase 1)

- **Left Mouse** — Select star system
- **WASD / Arrow keys** — Pan the galaxy map
- **Mouse Wheel** — Zoom
- **End Turn button** (or hotkey in future) — Advance the turn and process growth/economy
- **D** — Toggle full ImGui demo window
- **F1** — Toggle debug/empire panel

## Project Philosophy & Architecture

- **Faithful first**: MoO1 mechanics, balance, and "feel" before new features.
- **Immediate-mode UI**: Almost all interface is Dear ImGui — fast iteration.
- **Pure procedural rendering** (Phase 1): No textures or audio files yet.
- **Clean separation**: `src/core` and `src/entities` are pure C++ (no raylib/ImGui).
- **Modern C++23**: Concepts, ranges, `std::expected` (future), etc. will be used as systems mature.

### Directory Layout

```
src/
├── core/           # Galaxy, GameState, Enums, GalaxyGeneration, Empire
├── entities/       # Planet, StarSystem, Colony
├── ui/             # (future) dedicated screen managers
├── rendering/      # (future) camera, draw helpers, particles
├── data/           # (future) JSON tech trees, race defs, ship parts
└── utils/
```

## Roadmap (High Level)

**Phase 1 (done)**: Galaxy + basic turn loop + selection + colonization stub + ImGui chrome.

**Phase 2 (next)**:
- Proper 5-slider colony management window (the heart of MoO)
- Real resource calculations (food, net production, pollution)
- Simple ship designer + build queues
- 6–8 distinct races with bonuses
- Basic AI empires that also colonize and research

**Phase 3**:
- Technology tree with "choose one" picks
- Tactical or auto-resolve combat
- Diplomacy
- Save/load (simdjson)
- Sound (raylib procedural or placeholder)

## Contributing / Playing

This is currently a solo ambitious hobby project. The goal is a complete, polished, playable love letter to Master of Orion that runs everywhere and feels authentic.

Pull requests that improve the fidelity of the simulation or the elegance of the UI are very welcome once the core loops are solid.

## License

To be determined (likely MIT or similar once more complete). All original Master of Orion IP remains with its owners.

---

"**The galaxy is a big place...**" — let's explore it again, the right way.
