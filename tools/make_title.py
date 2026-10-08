#!/usr/bin/env python3
"""Generates the title screen (res/gfx/title.png, 320x224, 16 colours on PAL0): the Nautiloid --
a huge nautilus shell trailing tentacles -- before the burning sky of Avernus, floating rocks,
and the NAUTILOID logo in bone-coloured block letters. The text lines ("EIN D&D-PROLOG",
"START DRÜCKEN") are drawn by src/title.c on the text layer.

    python3 tools/make_title.py      # also writes out/title_preview.png (3x)
"""
import math
import os
import random

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
W, H = 320, 224

# Mega Drive levels 0-7 per channel
PAL = [
    (0, 0, 0),   # 0 black
    (1, 0, 0),   # 1 sky, darkest red
    (2, 0, 0),   # 2
    (3, 1, 0),   # 3
    (5, 1, 0),   # 4 red
    (7, 3, 0),   # 5 orange
    (7, 6, 2),   # 6 yellow glow
    (1, 1, 1),   # 7 ship, dark
    (2, 2, 3),   # 8 ship
    (3, 3, 4),   # 9 ship, lit edge
    (3, 1, 3),   # 10 flesh, dark violet
    (5, 3, 5),   # 11 flesh, light
    (5, 4, 3),   # 12 bone (logo)
    (7, 7, 6),   # 13 bone highlight
    (6, 1, 1),   # 14 glowing membrane
    (7, 7, 7),   # 15 white
]
RGB = [tuple(round(c * 255 / 7) for c in p) for p in PAL]

FONT = {   # 5x7 block letters for the logo
    "N": ["X...X", "XX..X", "X.X.X", "X.X.X", "X..XX", "X...X", "X...X"],
    "A": [".XXX.", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X"],
    "U": ["X...X", "X...X", "X...X", "X...X", "X...X", "X...X", ".XXX."],
    "T": ["XXXXX", "..X..", "..X..", "..X..", "..X..", "..X..", "..X.."],
    "I": ["XXXXX", "..X..", "..X..", "..X..", "..X..", "..X..", "XXXXX"],
    "L": ["X....", "X....", "X....", "X....", "X....", "X....", "XXXXX"],
    "O": [".XXX.", "X...X", "X...X", "X...X", "X...X", "X...X", ".XXX."],
    "D": ["XXXX.", "X...X", "X...X", "X...X", "X...X", "X...X", "XXXX."],
}


def main():
    rng = random.Random(5)
    img = [[0] * W for _ in range(H)]

    def put(x, y, c):
        if 0 <= x < W and 0 <= y < H:
            img[int(y)][int(x)] = c

    # sky: black above, glowing towards the horizon, dithered bands
    bands = [(0, 0), (60, 1), (100, 2), (140, 3), (175, 4), (200, 5), (216, 6)]
    for y in range(H):
        for x in range(W):
            v = y + ((x * 7 + y * 3) % 9) - 4 + rng.randint(-3, 3)
            c = 0
            for y0, col in bands:
                if v >= y0:
                    c = col
            img[y][x] = c
    # floating rocks, black against the glow
    for _ in range(9):
        cx, cy, r = rng.randint(10, 310), rng.randint(110, 215), rng.randint(3, 9)
        for y in range(cy - r, cy + r):
            for x in range(cx - 2 * r, cx + 2 * r):
                if ((x - cx) / (1.6 * r)) ** 2 + ((y - cy) / r) ** 2 <= 1 + rng.random() * 0.2:
                    put(x, y, 0)
                    if y == cy - r + 1 and rng.random() < 0.5:
                        put(x, y, 4)   # lit rim

    # the Nautiloid: a nautilus shell (logarithmic spiral), ribbed, with glowing membranes
    sx, sy = 206, 128
    for y in range(40, 224):
        for x in range(90, 320):
            dx, dy = x - sx, (y - sy) * 1.15
            r = math.hypot(dx, dy)
            if r < 4:
                continue
            a = math.atan2(dy, dx)
            # distance along the spiral r = a0 * e^(k*theta): which whorl this pixel is in
            t = (math.log(r / 6.0) / 0.18 - a) / (2 * math.pi)
            if r > 92:
                continue
            if dx < -52 and abs(dy) < 48 - (dx + 52) * 0.3:
                put(x, y, 7 if abs(dy) > 38 else 10)   # the shell's mouth, flesh inside
                continue
            whorl = t % 1.0
            rib = (a * 7 + r * 0.25) % 1.0
            c = 8
            if whorl < 0.08:
                c = 7                                # the seam between whorls
            elif whorl > 0.85:
                c = 9                                # lit edge of each whorl
            if rib < 0.12 and c == 8:
                c = 7                                # ribs across the shell
            if c == 8 and 30 < r < 88 and (int(a * 5 + t * 3) % 4 == 0) and 0.3 < whorl < 0.6:
                c = 14                               # glowing membranes between the ribs
            put(x, y, c)
    # tentacles trailing from the shell's mouth, curling down and to the left: outlined, lit on
    # top, suckers underneath
    tentacles = []
    for k in range(6):
        pts = []
        x0, y0 = 128 + (k % 2) * 6, 104 + k * 9
        for i in range(80):
            t = i / 79
            x = x0 - 100 * t - 8 * math.sin(t * 5 + k)
            y = y0 + (30 + k * 6) * t * t + 10 * math.sin(t * 7 + k * 1.3)
            pts.append((x, y, 6.5 * (1 - t) + 1.2))
        tentacles.append(pts)
    for pts in tentacles:                            # outlines first, so neighbours stay apart
        for x, y, r in pts:
            for yy in range(int(y - r - 1), int(y + r) + 2):
                for xx in range(int(x - r - 1), int(x + r) + 2):
                    if (xx - x) ** 2 + (yy - y) ** 2 <= (r + 1) ** 2:
                        put(xx, yy, 7)
    for pts in tentacles:
        for i, (x, y, r) in enumerate(pts):
            for yy in range(int(y - r), int(y + r) + 1):
                for xx in range(int(x - r), int(x + r) + 1):
                    if (xx - x) ** 2 + (yy - y) ** 2 <= r * r:
                        put(xx, yy, 11 if yy < y - r * 0.35 else 10)
            if i % 6 == 3 and r > 2.2:
                put(x, y + r - 1, 14)                # suckers
                put(x + 1, y + r - 1, 14)

    # the logo: NAUTILOID in bone block letters with an outline and a drop shadow
    word, scale, gap = "NAUTILOID", 5, 5
    width = len(word) * (5 * scale + gap) - gap
    x0, y0 = (W - width) // 2, 18
    for pass_, col in ((0, 0), (1, 12)):
        for i, ch in enumerate(word):
            for ry, row in enumerate(FONT[ch]):
                for rx, bit in enumerate(row):
                    if bit != "X":
                        continue
                    for yy in range(scale):
                        for xx in range(scale):
                            px = x0 + i * (5 * scale + gap) + rx * scale + xx
                            py = y0 + ry * scale + yy
                            if pass_ == 0:
                                for ox, oy in ((-1, 0), (1, 0), (0, -1), (0, 1), (3, 3), (2, 3), (3, 2)):
                                    put(px + ox, py + oy, 0)
                            else:
                                c = 13 if yy == 0 or (xx == 0 and rx == 0) else 12
                                put(px, py, c)
    # a letterbox band at the bottom for the text lines (src/title.c), edged in glowing red
    for y in range(196, H):
        for x in range(W):
            put(x, y, 14 if y == 196 else 4 if y == 197 else 0)

    # a thin red glow line under the logo
    for x in range(x0, x0 + width):
        put(x, y0 + 7 * scale + 6, 14 if (x // 3) % 2 else 4)

    out = Image.new("P", (W, H))
    out.putpalette([v for c in RGB for v in c] + [0] * (768 - 48))
    out.putdata([c for row in img for c in row])
    path = os.path.join(ROOT, "res", "gfx", "title.png")
    out.save(path)
    prev = os.path.join(ROOT, "out", "title_preview.png")
    os.makedirs(os.path.dirname(prev), exist_ok=True)
    out.convert("RGB").resize((W * 3, H * 3), Image.NEAREST).save(prev)
    tiles = set()
    for ty in range(H // 8):
        for tx in range(W // 8):
            tiles.add(tuple(img[ty * 8 + r][tx * 8 + k] for r in range(8) for k in range(8)))
    print(f"wrote {path}: {len(tiles)} unique tiles (the view's 1120 must not be exceeded)")


if __name__ == "__main__":
    main()
