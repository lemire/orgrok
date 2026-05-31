# Fonts for Orion Reborn

Place any TrueType (.ttf) or OpenType (.otf) font file in this directory and the game will automatically use it for the UI (race selection, and later all ImGui panels).

## How it works
- The game looks for several common names on startup (see main.cpp around the font loading block).
- It loads the same file twice: once at ~16pt for normal text, once at ~24pt for titles and race names.
- If no suitable .ttf is found, it silently falls back to ImGui's built-in default font.

## Recommended fonts (all free / high quality)

**Best everyday choice**
- **Inter** — https://rsms.me/inter/
  - Extremely readable at small sizes, excellent hinting, feels modern and clean.
  - Download the "Inter Desktop" or "Inter Variable" and use `Inter-Regular.ttf` (or `Inter.ttf`).

**Nice subtle sci-fi / space feel**
- **Exo 2** — https://fonts.google.com/specimen/Exo+2
  - Geometric, slightly futuristic, still very legible. Great match for a spiritual MoO successor.
  - Use the Regular weight.

**Safe & excellent everywhere**
- **Roboto** (or Roboto Flex) — https://fonts.google.com/specimen/Roboto
  - The classic. Never looks bad. Roboto Flex gives you more weight options if you want.

**More "space age" header fonts** (use for titles only or pair with a body font)
- Rajdhani
- Orbitron
- Exo (the original, more condensed)

## Tips
- For best results pick a font that has good numerals and is hinted well at 14–20 px.
- Having a matching Bold weight is nice for future UI work but not required.
- The game only needs one .ttf file right now (it re-uses it at two sizes).
- Keep file names simple (e.g. `Inter-Regular.ttf`) so the auto-loader finds them.

## Font quality improvements (2026)
The UI now uses an optimized `ImFontConfig`:
- Oversampling (stb_truetype path) + `PixelSnapH` + `RasterizerMultiply` for crisper text.
- When CMake detects FreeType (highly recommended), we build with `imgui_freetype` + `ForceAutoHint` for **much** sharper results, especially at small sizes. This is the single biggest visual upgrade for fonts in Dear ImGui.

**FreeType is optional** — the game builds and runs without it (falls back to improved stb_truetype). On macOS with Homebrew it's usually auto-detected (`brew install freetype`). On Linux: `sudo apt install libfreetype6-dev` (or equivalent). Windows: vcpkg or manual.

After installing FreeType, just reconfigure + rebuild (`cmake -B build -S . && cmake --build build`).

## Example layout after adding a font
assets/fonts/
    Inter-Regular.ttf
    README.md

Then just run the game — it will log which font it picked.

If you want multiple fonts (body + a fancy title font) or more sizes in the future, let me know and we can extend the loader.
