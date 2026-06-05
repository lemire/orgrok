#!/usr/bin/env python3
"""
Improved planet SVG generator for orion-reborn.
All 9 planet types, 2 variants each.
Focus on looking like real(istic) planets: organic shapes, proper craters with rims,
clustered continents, curved dunes, clean circular body (no baked outer glow or shade
so runtime can composite clean atmosphere + dynamic lighting).
"""

import math
import random
import os

TYPES = ["Radiated", "Barren", "Desert", "Steppe", "Arid", "Swamp", "Ocean", "Terran", "Gaia"]

def clamp(v, lo=0, hi=255):
    return max(lo, min(hi, int(v)))

def color_str(rgb, a=255):
    r, g, b = rgb
    if a >= 255:
        return f"rgb({r},{g},{b})"
    return f"rgba({r},{g},{b},{a/255:.2f})"

def make_radial_gradient(id_, stops, cx="50%", cy="50%", r="52%"):
    s = [f'    <radialGradient id="{id_}" cx="{cx}" cy="{cy}" r="{r}" fx="38%" fy="36%">']
    for off, col in stops:
        s.append(f'      <stop offset="{off}" stop-color="{col}"/>')
    s.append('    </radialGradient>')
    return '\n'.join(s)

def draw_crater(cx, cy, cr, elements, seed):
    """Draw a crater with raised rim + pit for realism."""
    random.seed(seed)
    # Pit
    pit_col = "rgba(35,30,25,0.75)"
    elements.append(f'  <circle cx="{cx}" cy="{cy}" r="{cr*0.92}" fill="{pit_col}" />')
    # Rim highlight (offset to upper-left for light)
    rim_off_x = -cr * 0.18
    rim_off_y = -cr * 0.18
    rim_col = "rgba(210,200,180,0.55)"
    elements.append(f'  <circle cx="{cx + rim_off_x}" cy="{cy + rim_off_y}" r="{cr * 0.38}" fill="{rim_col}" />')
    # Subtle inner shadow ring
    elements.append(f'  <circle cx="{cx}" cy="{cy}" r="{cr*0.78}" fill="none" stroke="rgba(20,15,10,0.5)" stroke-width="{max(1.0, cr*0.08)}" />')

def planet_svg(planet_type, variant, seed):
    random.seed(seed)
    size = 512
    cx, cy = size // 2, size // 2
    r = 235  # tighter body so texture is mostly useful pixels when scaled

    palettes = {
        "Radiated": {"base": (105, 88, 68), "dark": (48, 38, 28), "accent": (255, 95, 35), "name": "Radiated"},
        "Barren":   {"base": (142, 135, 122), "dark": (68, 62, 55), "accent": (175, 165, 145), "name": "Barren"},
        "Desert":   {"base": (208, 172, 88), "dark": (118, 88, 48), "accent": (232, 188, 102), "name": "Desert"},
        "Steppe":   {"base": (148, 162, 78), "dark": (78, 88, 42), "accent": (168, 178, 88), "name": "Steppe"},
        "Arid":     {"base": (175, 148, 92), "dark": (98, 78, 50), "accent": (190, 162, 105), "name": "Arid"},
        "Swamp":    {"base": (62, 108, 72), "dark": (35, 62, 44), "accent": (82, 132, 92), "name": "Swamp"},
        "Ocean":    {"base": (48, 98, 168), "dark": (26, 48, 98), "accent": (62, 118, 182), "name": "Ocean"},
        "Terran":   {"base": (52, 105, 172), "dark": (28, 55, 105), "accent": (78, 142, 88), "name": "Terran"},
        "Gaia":     {"base": (58, 115, 185), "dark": (30, 60, 115), "accent": (95, 185, 105), "name": "Gaia"},
    }
    pal = palettes[planet_type]

    defs = []
    elements = []

    # Body gradient (subtle 3D even before overlays)
    body_id = f"body{planet_type}{variant}"
    defs.append(make_radial_gradient(
        body_id,
        [("0%", color_str(tuple(min(255, c + 18) for c in pal["base"]))),
         ("52%", color_str(pal["base"])),
         ("100%", color_str(pal["dark"]))],
        cx="39%", cy="37%", r="58%"
    ))

    # Main planet disk (clean circle)
    elements.append(f'  <circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#{body_id})" />')

    # Very thin dark limb for strong "globe" definition (baked)
    elements.append(f'  <circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="rgba(0,0,0,0.28)" stroke-width="5.5" />')

    feature_seed = seed + variant * 23

    # === Surface features per type (improved organic look) ===
    random.seed(feature_seed)

    if planet_type in ("Radiated", "Barren"):
        # Heavy cratering with proper rims
        ncrat = 16 if planet_type == "Barren" else 11
        for i in range(ncrat):
            ang = random.uniform(0, 6.2832)
            dist = random.uniform(0.12, 0.91) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.9
            cr = random.uniform(7, 27)
            draw_crater(fx, fy, cr, elements, feature_seed + i * 7)
        # Extra small pits
        for i in range(9):
            ang = random.uniform(0, 6.2832)
            dist = random.uniform(0.08, 0.94) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.88
            elements.append(f'  <circle cx="{fx}" cy="{fy}" r="{random.uniform(2.5,6)}" fill="rgba(25,20,15,0.6)" />')

    elif planet_type in ("Desert", "Arid", "Steppe"):
        # Clustered dunes + patches (use paths for curves)
        dune_base = tuple(max(0, c - 18) for c in pal["accent"])
        for i in range(7):
            ang = random.uniform(0, 6.2832)
            dist = random.uniform(0.15, 0.82) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.83
            rot = random.randint(-40, 40)
            # Curved dune ridge using path
            len_ = random.uniform(55, 110)
            dx = math.cos(math.radians(rot)) * len_
            dy = math.sin(math.radians(rot)) * len_ * 0.28
            elements.append(
                f'  <path d="M {fx-dx*0.5},{fy-dy*0.5} Q {fx},{fy} {fx+dx*0.5},{fy+dy*0.5}" '
                f'fill="none" stroke="{color_str(dune_base, random.randint(45,75))}" stroke-width="{random.uniform(3.5,8)}" stroke-linecap="round" />'
            )
        # Darker arid patches
        for i in range(5):
            ang = random.uniform(0, 6.2832)
            dist = random.uniform(0.1, 0.9) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.85
            elements.append(f'  <ellipse cx="{fx}" cy="{fy}" rx="{random.uniform(14,32)}" ry="{random.uniform(6,14)}" fill="rgba(70,52,28,0.22)" transform="rotate({random.randint(-25,25)} {fx} {fy})" />')

    elif planet_type == "Swamp":
        # Irregular vegetation patches (overlapping for organic)
        for i in range(9):
            ang = random.uniform(0, 6.2832)
            dist = random.uniform(0.14, 0.85) * r
            fx = cx + math.cos(ang) * dist
            fy = cy + math.sin(ang) * dist * 0.81
            rx = random.uniform(26, 58)
            ry = rx * random.uniform(0.6, 0.95)
            col = color_str((random.randint(48,72), random.randint(95,125), random.randint(55,78)), random.randint(80,115))
            elements.append(f'  <ellipse cx="{fx}" cy="{fy}" rx="{rx}" ry="{ry}" fill="{col}" transform="rotate({random.randint(-55,55)} {fx} {fy})" />')
            if random.random() > 0.6:
                elements.append(f'  <ellipse cx="{fx+rx*0.25}" cy="{fy-ry*0.15}" rx="{rx*0.5}" ry="{ry*0.55}" fill="{col}" transform="rotate({random.randint(-30,30)} {fx} {fy})" />')

    elif planet_type == "Ocean":
        # Island clusters (land masses look more natural)
        land_col = color_str((random.randint(82,108), random.randint(122,148), random.randint(52,75)), 225)
        for grp in range(3):
            gcx = cx + random.uniform(-0.45, 0.45) * r
            gcy = cy + random.uniform(-0.32, 0.32) * r
            for j in range(3):
                fx = gcx + random.uniform(-38, 38)
                fy = gcy + random.uniform(-28, 28)
                rx = random.uniform(22, 48)
                ry = rx * random.uniform(0.55, 0.85)
                rot = random.randint(-48, 48)
                elements.append(f'  <ellipse cx="{fx}" cy="{fy}" rx="{rx}" ry="{ry}" fill="{land_col}" transform="rotate({rot} {fx} {fy})" />')
        # Atoll hint (ring)
        if random.random() > 0.5:
            fax = cx + random.uniform(-0.3,0.3)*r
            fay = cy + random.uniform(-0.2,0.2)*r
            elements.append(f'  <circle cx="{fax}" cy="{fay}" r="{random.uniform(18,32)}" fill="none" stroke="{color_str((95,135,105),40)}" stroke-width="2.5" />')

    elif planet_type in ("Terran", "Gaia"):
        # Proper Earth-like: blue oceans (base) + green/brown continents + ice caps.
        # Use multiple overlapping ellipses in clusters for irregular landmasses (continents + islands).
        # More land / lusher for Gaia.
        is_gaia = (planet_type == "Gaia")
        # Land colors: mix of greens, some browns/tans for terrain variety
        land_colors = [
            (78, 138, 72) if not is_gaia else (88, 175, 82),
            (68, 122, 62) if not is_gaia else (78, 158, 75),
            (95, 145, 70) if not is_gaia else (105, 170, 80),
            (115, 105, 65) if not is_gaia else (125, 115, 70),  # tan/brown highlights
            (55, 105, 55) if not is_gaia else (62, 138, 60),
        ]
        # Continent cluster centers (spread for oceans between)
        groups = [(-0.38, -0.22), (0.22, 0.18), (-0.12, 0.32), (0.35, -0.25)]
        num_per_group = 6 if is_gaia else 5
        for gi, (ox, oy) in enumerate(groups):
            gcx = cx + ox * r
            gcy = cy + oy * r
            for j in range(num_per_group):
                fx = gcx + random.uniform(-72, 72)
                fy = gcy + random.uniform(-55, 55)
                rx = random.uniform(28, 78)
                ry = rx * random.uniform(0.45, 0.88)
                rot = random.randint(-45, 45)
                col = land_colors[(gi + j) % len(land_colors)]
                alpha = 232 if not is_gaia else 245
                elements.append(f'  <ellipse cx="{fx}" cy="{fy}" rx="{rx}" ry="{ry}" fill="{color_str(col, alpha)}" transform="rotate({rot} {fx} {fy})" />')
                # Extra small overlapping for jagged coast / detail
                if random.random() > 0.5:
                    fx2 = fx + random.uniform(-18, 18)
                    fy2 = fy + random.uniform(-14, 14)
                    elements.append(f'  <ellipse cx="{fx2}" cy="{fy2}" rx="{rx*0.55}" ry="{ry*0.55}" fill="{color_str(col, alpha-20)}" transform="rotate({rot+15} {fx} {fy})" />')
        # Ice caps (baked, more prominent on Gaia for "nicer")
        ice_a = 115 if is_gaia else 85
        ice_rx = r * (0.34 if is_gaia else 0.30)
        ice_ry = r * (0.13 if is_gaia else 0.11)
        elements.append(f'  <ellipse cx="{cx}" cy="{cy - r*0.76}" rx="{ice_rx}" ry="{ice_ry}" fill="rgba(235,245,255,{ice_a/255:.2f})" />')
        elements.append(f'  <ellipse cx="{cx}" cy="{cy + r*0.76}" rx="{ice_rx * 0.85}" ry="{ice_ry * 0.8}" fill="rgba(235,245,255,{(ice_a*0.75)/255:.2f})" />')
        # Extra Gaia "lush" touches: small bright green patches (forests/jungles)
        if is_gaia:
            for i in range(5):
                ang = random.uniform(0, 6.2832)
                dist = random.uniform(0.18, 0.65) * r
                fx = cx + math.cos(ang) * dist
                fy = cy + math.sin(ang) * dist * 0.82
                elements.append(f'  <ellipse cx="{fx}" cy="{fy}" rx="{random.uniform(12,22)}" ry="{random.uniform(8,16)}" fill="rgba(70,195,85,165)" transform="rotate({random.randint(-30,30)} {fx} {fy})" />')

    # Subtle fixed haze / high cloud suggestions (very low opacity, will be overpainted by runtime anim clouds)
    if planet_type in ("Gaia", "Terran", "Ocean", "Swamp"):
        haze = "rgba(245,250,255,0.16)"
        for i in range(2):
            ca = random.uniform(0, 6.28)
            cd = r * random.uniform(0.32, 0.55)
            cxx = cx + math.cos(ca) * cd * 0.65
            cyy = cy + math.sin(ca) * cd * 0.48
            elements.append(f'  <circle cx="{cxx}" cy="{cyy}" r="{r * random.uniform(0.32,0.44)}" fill="{haze}" />')

    # No baked specular here anymore — the radial body gradient provides the 3D sphere shading.
    # (Previously the offset white circle could look like a detached "light accent" or second component at small scales.)

    # Assemble
    svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg width="{size}" height="{size}" viewBox="0 0 {size} {size}" xmlns="http://www.w3.org/2000/svg">
  <defs>
{chr(10).join(defs)}
  </defs>
  <!-- {planet_type} (v{variant}) - improved for orion-reborn -->
{chr(10).join(elements)}
</svg>
'''
    return svg

def main():
    out_dir = os.path.dirname(os.path.abspath(__file__))
    count = 0
    for ptype in TYPES:
        for var in range(1, 3):
            seed = 4242 + (hash(ptype) & 0xffff) + var * 73
            svg = planet_svg(ptype, var, seed)
            fname = f"planet_{ptype.lower()}_{var:02d}.svg"
            with open(os.path.join(out_dir, fname), "w", encoding="utf-8") as f:
                f.write(svg)
            print(f"Wrote {fname}")
            count += 1
    print(f"\nGenerated {count} improved planet SVGs.")

if __name__ == "__main__":
    main()
