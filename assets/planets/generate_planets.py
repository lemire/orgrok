#!/usr/bin/env python3
"""
Generate a range of SVG planet representations for all PlanetType in orion-reborn.
Covers: Radiated, Barren, Desert, Steppe, Arid, Swamp, Ocean, Terran, Gaia.
Creates 2 variants per type for visual variety (18 total).
SVGs are self-contained, use gradients, paths, circles for nice vector planets ~512x512 viewBox.
"""

import math
import random
import os

TYPES = [
    "Radiated",
    "Barren",
    "Desert",
    "Steppe",
    "Arid",
    "Swamp",
    "Ocean",
    "Terran",
    "Gaia",
]

def clamp(v, lo=0, hi=255):
    return max(lo, min(hi, int(v)))

def hsv_to_rgb(h, s, v):
    """Simple HSV to RGB (h in 0-360, s,v 0-1)"""
    c = v * s
    x = c * (1 - abs(((h / 60) % 2) - 1))
    m = v - c
    if 0 <= h < 60:   r,g,b = c, x, 0
    elif h < 120:     r,g,b = x, c, 0
    elif h < 180:     r,g,b = 0, c, x
    elif h < 240:     r,g,b = 0, x, c
    elif h < 300:     r,g,b = x, 0, c
    else:             r,g,b = c, 0, x
    return (clamp((r+m)*255), clamp((g+m)*255), clamp((b+m)*255))

def color_str(rgb, a=255):
    r,g,b = rgb
    if a >= 255:
        return f"rgb({r},{g},{b})"
    return f"rgba({r},{g},{b},{a/255:.2f})"

def make_gradient(id_, stops, x1="0%", y1="0%", x2="100%", y2="100%"):
    """Linear gradient string"""
    s = [f'    <linearGradient id="{id_}" x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}">']
    for off, col in stops:
        s.append(f'      <stop offset="{off}" stop-color="{col}"/>')
    s.append('    </linearGradient>')
    return '\n'.join(s)

def make_radial_gradient(id_, stops, cx="50%", cy="50%", r="50%"):
    s = [f'    <radialGradient id="{id_}" cx="{cx}" cy="{cy}" r="{r}" fx="40%" fy="35%">']
    for off, col in stops:
        s.append(f'      <stop offset="{off}" stop-color="{col}"/>')
    s.append('    </radialGradient>')
    return '\n'.join(s)

def planet_svg(planet_type, variant, seed):
    """Generate a full SVG string for a planet of given type/variant."""
    random.seed(seed)
    size = 512
    cx, cy = size//2, size//2
    r = 220  # planet radius in viewBox units

    # Base palette per type (inspired by classic 4X / MoO)
    palettes = {
        "Radiated": {
            "base": (110, 95, 75),
            "dark": (55, 45, 35),
            "accent": (255, 110, 40),
            "atm": (160, 120, 70, 55),
            "name": "Radiated",
        },
        "Barren": {
            "base": (145, 138, 125),
            "dark": (75, 70, 62),
            "accent": (180, 170, 150),
            "atm": (150, 140, 120, 45),
            "name": "Barren",
        },
        "Desert": {
            "base": (210, 175, 95),
            "dark": (130, 100, 55),
            "accent": (235, 195, 110),
            "atm": (200, 160, 90, 50),
            "name": "Desert",
        },
        "Steppe": {
            "base": (155, 170, 85),
            "dark": (85, 95, 45),
            "accent": (175, 185, 95),
            "atm": (160, 170, 100, 48),
            "name": "Steppe",
        },
        "Arid": {
            "base": (180, 155, 100),
            "dark": (105, 85, 55),
            "accent": (195, 170, 115),
            "atm": (170, 145, 95, 52),
            "name": "Arid",
        },
        "Swamp": {
            "base": (70, 115, 80),
            "dark": (40, 70, 50),
            "accent": (90, 145, 100),
            "atm": (85, 130, 95, 60),
            "name": "Swamp",
        },
        "Ocean": {
            "base": (55, 105, 175),
            "dark": (30, 55, 105),
            "accent": (70, 130, 195),
            "atm": (80, 140, 210, 55),
            "name": "Ocean",
        },
        "Terran": {
            "base": (60, 125, 75),
            "dark": (35, 70, 45),
            "accent": (85, 155, 95),
            "atm": (95, 155, 210, 50),
            "name": "Terran",
        },
        "Gaia": {
            "base": (75, 175, 105),
            "dark": (40, 95, 60),
            "accent": (120, 220, 145),
            "atm": (110, 230, 160, 65),
            "name": "Gaia",
        },
    }
    pal = palettes[planet_type]

    defs = []
    elements = []

    # Background transparent, we draw planet at center
    # Outer soft atmosphere glow (larger)
    atm_r = r * 1.18 if planet_type in ("Ocean", "Terran", "Gaia", "Swamp") else r * 1.10
    atm_col = color_str(pal["atm"][:3], pal["atm"][3])
    elements.append(f'  <circle cx="{cx}" cy="{cy}" r="{atm_r}" fill="{atm_col}" />')

    # Main planet body - use radial gradient for subtle 3D
    base_rgb = pal["base"]
    dark_rgb = pal["dark"]
    grad_id = f"bodyGrad{variant}"
    defs.append(make_radial_gradient(
        grad_id,
        [("0%", color_str(tuple(min(255, c+25) for c in base_rgb))),
         ("55%", color_str(base_rgb)),
         ("100%", color_str(dark_rgb))],
        cx="42%", cy="38%", r="62%"
    ))
    elements.append(f'  <circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#{grad_id})" />')

    # Terminator / night side shading (offset dark ellipse)
    shade_id = f"shade{variant}"
    defs.append(make_radial_gradient(
        shade_id,
        [("0%", "rgba(0,0,0,0.0)"), ("70%", "rgba(0,0,0,0.55)"), ("100%", "rgba(0,0,0,0.78)")],
    ))
    shade_cx = cx + r * 0.22
    shade_cy = cy + r * 0.18
    elements.append(f'  <ellipse cx="{shade_cx}" cy="{shade_cy}" rx="{r*0.96}" ry="{r*0.96}" fill="url(#{shade_id})" />')

    # === Type-specific surface features ===
    feat_seed = seed + (variant * 17)
    random.seed(feat_seed)

    if planet_type == "Radiated":
        # Cracks / lava fissures + craters + glow
        for i in range(7):
            ang = random.uniform(0, 6.28)
            dist = random.uniform(0.25, 0.82) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.88
            # fissure
            length = random.uniform(18, 52)
            dx = math.cos(ang + random.uniform(-0.6,0.6)) * length
            dy = math.sin(ang + random.uniform(-0.6,0.6)) * length * 0.6
            lw = random.uniform(2.5, 5.5)
            lava = color_str((255, random.randint(70,120), 20), random.randint(120,190))
            elements.append(f'  <line x1="{fx}" y1="{fy}" x2="{fx+dx}" y2="{fy+dy}" stroke="{lava}" stroke-width="{lw}" stroke-linecap="round" />')
        # craters
        for i in range(9):
            ang = random.uniform(0, 6.28)
            dist = random.uniform(0.15, 0.88) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.9
            cr = random.uniform(8, 26)
            elements.append(f'  <circle cx="{fx}" cy="{fy}" r="{cr}" fill="rgba(40,32,25,0.6)" stroke="rgba(70,55,40,0.7)" stroke-width="1.5"/>')
            elements.append(f'  <circle cx="{fx-cr*0.2}" cy="{fy-cr*0.25}" r="{cr*0.35}" fill="rgba(255,90,30,0.25)"/>')

    elif planet_type == "Barren":
        # Lots of craters, dusty texture suggestion
        for i in range(14):
            ang = random.uniform(0, 6.28)
            dist = random.uniform(0.1, 0.92) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.88
            cr = random.uniform(6, 29)
            gray = random.randint(55, 95)
            elements.append(f'  <circle cx="{fx}" cy="{fy}" r="{cr}" fill="rgb({gray},{gray-5},{gray-10})" stroke="rgba(30,25,20,0.6)" stroke-width="1.2"/>')
            if random.random() < 0.6:
                elements.append(f'  <circle cx="{fx-cr*0.25}" cy="{fy-cr*0.3}" r="{cr*0.28}" fill="rgba(255,255,255,0.12)"/>')

    elif planet_type in ("Desert", "Arid", "Steppe"):
        # Dunes / patches / sparse features
        dune_col = color_str( (pal["accent"][0]-20, pal["accent"][1]-15, pal["accent"][2]-25), 70 )
        for i in range(5 + (2 if planet_type=="Steppe" else 0)):
            ang = random.uniform(0, 6.28)
            dist = random.uniform(0.2, 0.78) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.82
            rx = random.uniform(45, 95)
            ry = rx * random.uniform(0.22, 0.45)
            rot = random.randint(-35, 35)
            elements.append(f'  <ellipse cx="{fx}" cy="{fy}" rx="{rx}" ry="{ry}" fill="{dune_col}" transform="rotate({rot} {fx} {fy})" />')
        # small darker patches
        for i in range(6):
            ang = random.uniform(0, 6.28)
            dist = random.uniform(0.12, 0.9) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.85
            pr = random.uniform(12, 32)
            elements.append(f'  <circle cx="{fx}" cy="{fy}" r="{pr}" fill="rgba(80,60,35,0.25)"/>')

    elif planet_type == "Swamp":
        # Murky patches, vegetation blobs
        veg = color_str( (55, random.randint(95,125), 65), 95 )
        for i in range(8):
            ang = random.uniform(0, 6.28)
            dist = random.uniform(0.18, 0.82) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.8
            rx = random.uniform(28, 68)
            ry = rx * random.uniform(0.55, 0.9)
            elements.append(f'  <ellipse cx="{fx}" cy="{fy}" rx="{rx}" ry="{ry}" fill="{veg}" />')
        # darker water-ish
        for i in range(3):
            ang = random.uniform(0, 6.28)
            dist = random.uniform(0.35, 0.7) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.75
            elements.append(f'  <ellipse cx="{fx}" cy="{fy}" rx="{45}" ry="{22}" fill="rgba(25,55,40,0.35)" />')

    elif planet_type == "Ocean":
        # Islands / landmasses
        land = color_str( (random.randint(85,115), random.randint(130,155), random.randint(55,80)), 220 )
        for i in range(5):
            ang = random.uniform(0, 6.28)
            dist = random.uniform(0.18, 0.72) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.78
            rx = random.uniform(32, 72)
            ry = rx * random.uniform(0.5, 0.85)
            rot = random.randint(-50, 50)
            elements.append(f'  <ellipse cx="{fx}" cy="{fy}" rx="{rx}" ry="{ry}" fill="{land}" transform="rotate({rot} {fx} {fy})" />')
            # tiny extra bits
            if random.random() > 0.5:
                elements.append(f'  <ellipse cx="{fx+rx*0.4}" cy="{fy-ry*0.2}" rx="{rx*0.45}" ry="{ry*0.4}" fill="{land}" transform="rotate({rot+20} {fx} {fy})" />')
        # wave lines suggestion
        for i in range(4):
            yoff = cy - r*0.35 + i * (r * 0.22)
            elements.append(f'  <path d="M {cx-r*0.75},{yoff} Q {cx-r*0.25},{yoff-8} {cx+r*0.75},{yoff+4}" fill="none" stroke="rgba(255,255,255,0.18)" stroke-width="3.5" />')

    elif planet_type in ("Terran", "Gaia"):
        # Continents + clouds
        cont_col = color_str( pal["accent"], 235 ) if planet_type == "Gaia" else color_str( (random.randint(70,100), random.randint(115,145), random.randint(55,75)), 225 )
        for i in range(4 if planet_type=="Terran" else 5):
            ang = random.uniform(0, 6.28)
            dist = random.uniform(0.15, 0.68) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.76
            rx = random.uniform(40, 88)
            ry = rx * random.uniform(0.48, 0.78)
            rot = random.randint(-42, 42)
            elements.append(f'  <ellipse cx="{fx}" cy="{fy}" rx="{rx}" ry="{ry}" fill="{cont_col}" transform="rotate({rot} {fx} {fy})" />')
            if random.random() > 0.4:
                elements.append(f'  <ellipse cx="{fx + rx*0.35}" cy="{fy + ry*0.15}" rx="{rx*0.55}" ry="{ry*0.55}" fill="{cont_col}" transform="rotate({rot-15} {fx} {fy})" />')
        # Ice caps for Terran/Gaia (subtle)
        ice_a = 105 if planet_type == "Gaia" else 85
        elements.append(f'  <ellipse cx="{cx}" cy="{cy - r*0.82}" rx="{r*0.38}" ry="{r*0.14}" fill="rgba(235,245,255,{ice_a/255:.2f})" />')
        elements.append(f'  <ellipse cx="{cx}" cy="{cy + r*0.82}" rx="{r*0.32}" ry="{r*0.12}" fill="rgba(235,245,255,{(ice_a*0.75)/255:.2f})" />')

    # Clouds / haze layer (most types except barren/radiated extremes)
    if planet_type not in ("Radiated", "Barren"):
        cloud_col = "rgba(255,255,255,0.38)" if planet_type in ("Gaia", "Terran", "Ocean") else "rgba(235,230,215,0.32)"
        cloud_count = 3 if planet_type in ("Gaia", "Terran") else 2
        for i in range(cloud_count):
            cang = random.uniform(0, 6.28)
            cdist = random.uniform(0.38, 0.72) * r
            cx_ = cx + math.cos(cang) * cdist * 0.75
            cy_ = cy + math.sin(cang * 0.8) * cdist * 0.55
            cr_ = random.uniform(r*0.38, r*0.62)
            elements.append(f'  <circle cx="{cx_}" cy="{cy_}" r="{cr_}" fill="{cloud_col}" />')

    # Extra Gaia shimmer / life glow rim
    if planet_type == "Gaia":
        elements.append(f'  <circle cx="{cx}" cy="{cy}" r="{r*0.995}" fill="none" stroke="rgba(140,255,170,0.35)" stroke-width="6" />')

    # Specular highlight (always)
    hl_x = cx - r * 0.30
    hl_y = cy - r * 0.32
    hl_r = r * 0.26
    elements.append(f'  <circle cx="{hl_x}" cy="{hl_y}" r="{hl_r}" fill="rgba(255,255,255,0.32)" />')
    elements.append(f'  <circle cx="{hl_x - 6}" cy="{hl_y - 8}" r="{hl_r*0.42}" fill="rgba(255,255,255,0.18)" />')

    # Assemble SVG
    svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg width="{size}" height="{size}" viewBox="0 0 {size} {size}" xmlns="http://www.w3.org/2000/svg">
  <defs>
{chr(10).join(defs)}
  </defs>
  <!-- {planet_type} planet (variant {variant}) - generated for orion-reborn -->
{chr(10).join(elements)}
</svg>
'''
    return svg

def main():
    out_dir = os.path.dirname(os.path.abspath(__file__))
    count = 0
    for ptype in TYPES:
        for var in range(1, 3):  # two variants each
            seed = 1337 + hash(ptype) % 10000 + var * 91
            svg = planet_svg(ptype, var, seed)
            fname = f"planet_{ptype.lower()}_{var:02d}.svg"
            fpath = os.path.join(out_dir, fname)
            with open(fpath, "w", encoding="utf-8") as f:
                f.write(svg)
            print(f"Wrote {fname}")
            count += 1
    print(f"\nGenerated {count} planet SVG files in {out_dir}")

if __name__ == "__main__":
    main()
