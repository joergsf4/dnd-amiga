#!/usr/bin/env python3
"""Makes the intro logo (res/gfx/rcd_logo.png) from the club's pixel-art logo, tools/rcd_logo.svg
(Retro Computer Dresden e.V., https://www.retrocomputer-dresden.de/img/LogoQuer.svg).

The SVG is pixel art on a 36-unit grid (65 x 26 pixels, four colours). It is drawn 4 times as large (260 x 104) in a
264 x 104 indexed PNG (33 x 13 tiles) for rescomp's IMAGE. Palette: 0 backdrop (transparent), 1 black, 2 teal,
3 orange, 4 red, and two colours the intro code uses for its own tiles: 5 white (the shine), 6 dark blue (the
mask, the same colour as the intro's backdrop).

Usage: make_logo.py        writes res/gfx/rcd_logo.png
"""
import os
import re

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SVG = os.path.join(HERE, "rcd_logo.svg")
OUT = os.path.join(HERE, "..", "res", "gfx", "rcd_logo.png")
GRID = 36                   # SVG units per logo pixel
SCALE = 4                   # screen pixels per logo pixel
NAVY = (28, 28, 60)
PALETTE = [(255, 0, 255), (1, 2, 2), (35, 178, 160), (242, 146, 79), (233, 76, 78), (255, 255, 255), NAVY]


def main():
    svg = open(SVG).read()
    w, h = 2340 // GRID, 936 // GRID
    logo = Image.new("P", (w, h), 0)
    logo.putpalette([c for rgb in PALETTE + [(0, 0, 0)] * (256 - len(PALETTE)) for c in rgb])
    d = ImageDraw.Draw(logo)
    for tag in re.findall(r"<rect\b[^>]*>", svg):
        a = dict(re.findall(r'(\w[\w-]*)="([^"]*)"', tag))
        if "fill" not in a:
            continue
        x, y, rw, rh = (float(a[k]) for k in ("x", "y", "width", "height"))
        hexc = a["fill"].lstrip("#")
        rgb = tuple(int(hexc[i:i + 2], 16) for i in (0, 2, 4))
        idx = min(range(1, 5), key=lambda i: sum((PALETTE[i][k] - rgb[k]) ** 2 for k in range(3)))
        x0, y0 = int(x // GRID), int(y // GRID)
        d.rectangle((x0, y0, max(x0, int((x + rw) // GRID) - 1), max(y0, int((y + rh) // GRID) - 1)), fill=idx)
    big = logo.resize((w * SCALE, h * SCALE), Image.NEAREST)
    out = Image.new("P", (264, 104), 0)
    out.putpalette(logo.getpalette())
    out.paste(big, (2, 0))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    out.save(OUT)
    print(f"wrote {os.path.normpath(OUT)}: {out.size}, colours used {sorted(set(out.getdata()))}")


if __name__ == "__main__":
    main()
