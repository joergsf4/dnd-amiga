#!/usr/bin/env python3
"""Checks the Amiga's first-person view against tools/make_view.py's reference renderer.

Build with the view dump (./build.sh clean && ./build.sh DEFS=-DVIEW_DUMP), play to a view with
tools/emutest.sh, then run this: for every view in out/emutest/serial.txt -- the cells the game drew
and its chunky buffer -- it renders the same cells with make_view.render() and compares them pixel
for pixel; out/emutest/viewcheck.png shows the last one (Amiga | reference | differences in red).
"""
import os
import re
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)
import make_view as V   # noqa: E402

text = open(os.path.join(ROOT, "out", "emutest", "serial.txt"), "rb").read().decode("latin-1")
text = text.replace("\r", "")
blocks = text.split("VIEWCELLS ")[1:]
if not blocks:
    raise SystemExit("no VIEWCELLS in serial.txt -- built with DEFS=-DVIEW_DUMP?")
bd, bd_mirror = V.backdrop()
cols = [V.column_events(pc) for pc in range(V.NPAIR)]
failed, last_image = 0, None

for n, block in enumerate(blocks):
    head, rest = block.split("\n", 1) if "\n" in block else (block, "")
    fields = head.split()
    data = bytes.fromhex("".join(re.findall(r"^VIEWDATA ([0-9a-f]+)$", rest, re.M)))
    if len(data) != V.VW // 2 * V.VH:
        print(f"view {n}: incomplete ({len(data)} bytes), skipped")
        continue
    mirror = int(fields[0])
    cells = [tuple(map(int, f.split("/"))) for f in fields[1:]]
    grid, props = {}, {}
    for i, (t, p) in enumerate(cells):
        d, l = i // (2 * V.LMAX + 1), i % (2 * V.LMAX + 1) - V.LMAX
        if t:
            grid[(d, l)] = t - 1
        if p:
            props[(d, l)] = p - 1
    ref = V.render(bd_mirror if mirror else bd, cols, grid, props)
    amiga = [[(data[y * V.VW // 2 + x // 2] >> (0 if x & 1 else 4)) & 15 for x in range(V.VW)] for y in range(V.VH)]
    diff = sum(1 for y in range(V.VH) for x in range(V.VW) if amiga[y][x] != ref[y][x])
    print(f"view {n}: {len(grid)} walls, {len(props)} props, mirror {mirror}: {diff} of {V.VW * V.VH} pixels differ")
    failed += diff > 0
    last_image = (amiga, ref)

if last_image:
    amiga, ref = last_image
    out = Image.new("RGB", (V.VW * 3, V.VH))
    for y in range(V.VH):
        for x in range(V.VW):
            out.putpixel((x, y), V.RGB[amiga[y][x]])
            out.putpixel((V.VW + x, y), V.RGB[ref[y][x]])
            out.putpixel((2 * V.VW + x, y), (255, 0, 0) if amiga[y][x] != ref[y][x] else (0, 0, 0))
    out.resize((V.VW * 6, V.VH * 2), Image.NEAREST).save(os.path.join(ROOT, "out", "emutest", "viewcheck.png"))
sys.exit(1 if failed else 0)
