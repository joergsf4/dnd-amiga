#!/usr/bin/env python3
"""Converts the game's assets for the Amiga -- the counterpart of SGDK's rescomp.

Writes
    out/dnd.pak                 the data file on the disk: graphics, sounds, view data, ZX0-packed
    out/gen/res/resources.h     the resource declarations the game includes (as with rescomp)
    out/gen/resources.c         the resource structures; res_bind() points them into the pak
    out/gen/view_gen.h          the view's constants and table declarations (as make_view.py's on
                                the Mega Drive)
    out/gen/view_data.c         the view's tables

The Amiga reads dnd.pak once at start-up (src/amiga/hw.c) into memory blocks of their own, in
chip RAM where the blitter or Paula needs them. See AMIGA_PORT.md for the formats' reasoning.

Formats:
    tile        40 bytes: bitplanes 0-3 (8 row bytes each), then the opaque mask
    sprite      per frame: bitplanes 0-3 then the mask, each h rows of wWords words (chip)
    sample      signed 8-bit, even length (chip)
    textures    64x64 texels, 4 bits each, left texel in the high nibble, row by row
    props       per prop and texel row: the runs of opaque texels (see view())
    backdrop    the view's floor and ceiling: 112 bytes per row (2 pixels per byte), 160 rows
"""
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)
import make_view as V   # noqa: E402
import generate_music as M   # noqa: E402

GFX = os.path.join(ROOT, "res", "gfx")
SFX = os.path.join(ROOT, "res", "sfx")
OUT = os.path.join(ROOT, "out")
GEN = os.path.join(OUT, "gen")


# ---------------------------------------------------------------- the pak

class Pak:
    def __init__(self):
        self.entries = []          # (bytes, chip)

    def add(self, data, chip=False):
        data = bytes(data)
        if len(data) % 2:
            data += b"\0"
        self.entries.append((data, chip))
        return len(self.entries) - 1

    def write(self, path):
        """"DNDP", the entry count, per entry (offset, stored size, unpacked size, flags: bit 0 chip
        RAM, bit 1 ZX0-packed), then the entries. Each is packed with zx0 (the Docker image has
        it) where that makes it smaller."""
        zx0 = shutil.which("zx0")
        stored = []
        with tempfile.TemporaryDirectory() as tmp:
            for i, (data, chip) in enumerate(self.entries):
                flags = 1 if chip else 0
                blob = data
                if zx0 and len(data) > 64:
                    src, dst = os.path.join(tmp, f"{i}.bin"), os.path.join(tmp, f"{i}.zx0")
                    with open(src, "wb") as f:
                        f.write(data)
                    subprocess.run([zx0, "-f", src, dst], check=True, stdout=subprocess.DEVNULL)
                    packed = open(dst, "rb").read()
                    if len(packed) < len(data):
                        blob, flags = packed, flags | 2
                stored.append((blob, len(data), flags))
        head = 8 + 16 * len(stored)
        table, out = [], bytearray()
        for blob, size, flags in stored:
            while (head + len(out)) % 4:
                out.append(0)
            table.append(struct.pack(">IIII", head + len(out), len(blob), size, flags))
            out += blob
        with open(path, "wb") as f:
            f.write(b"DNDP" + struct.pack(">I", len(stored)) + b"".join(table) + out)
        return head + len(out)


PAK = Pak()
PROP_OFFSETS = []   # per prop: its start in viewPropSources
H = []          # resources.h body
C = []          # resources.c definitions
BIND = []       # res_bind() statements
FREE = []       # res_free_*() functions


# ---------------------------------------------------------------- colours and pixels

def md_color(r, g, b):
    """8-bit RGB -> the Mega Drive's 0x0BGR, 3 bits per component (rescomp's rounding down)."""
    return ((b >> 5) << 9) | ((g >> 5) << 5) | ((r >> 5) << 1)


def png_palette(img):
    p = img.getpalette() or []
    p += [0] * (48 - len(p))
    return [md_color(*p[i * 3:i * 3 + 3]) for i in range(16)]


def c_u16(values, per_line=12):
    lines = []
    for i in range(0, len(values), per_line):
        lines.append("    " + ", ".join("0x%04X" % v for v in values[i:i + per_line]) + ",")
    return "\n".join(lines)


def indexed(path):
    img = Image.open(path)
    if img.mode != "P":
        raise SystemExit(f"{path}: not an indexed PNG")
    return img


def tile_bytes(px, x0, y0):
    """8x8 pixel indices at (x0, y0) -> 40 bytes: bitplanes 0-3, then the opaque mask."""
    planes = [bytearray(8) for _ in range(5)]
    for r in range(8):
        for c in range(8):
            v = px[x0 + c, y0 + r] & 15
            bit = 0x80 >> c
            for k in range(4):
                if v & (1 << k):
                    planes[k][r] |= bit
            if v:
                planes[4][r] |= bit
    return b"".join(planes)


def palette_decl(name, pal):
    C.append(f"static const u16 {name}_data[16] = {{\n{c_u16(pal)}\n}};")
    C.append(f"static const Palette {name} = {{ 16, {name}_data }};")


# ---------------------------------------------------------------- resource kinds

def image(name, file):
    img = indexed(os.path.join(GFX, file))
    w, h = img.size
    px = img.load()
    tiles, index, tmap = [], {}, []
    for ty in range(h // 8):
        for tx in range(w // 8):
            t = tile_bytes(px, tx * 8, ty * 8)
            if t not in index:
                index[t] = len(tiles)
                tiles.append(t)
            tmap.append(index[t])
    e = PAK.add(b"".join(tiles))
    palette_decl(f"{name}_pal", png_palette(img))
    C.append(f"static TileSet {name}_tileset = {{ {len(tiles)}, 0 }};")
    C.append(f"static const u16 {name}_map_data[{len(tmap)}] = {{\n{c_u16(tmap)}\n}};")
    C.append(f"static const TileMap {name}_map = {{ {w // 8}, {h // 8}, {name}_map_data }};")
    C.append(f"Image {name} = {{ &{name}_pal, &{name}_tileset, &{name}_map }};")
    BIND.append(f"    {name}_tileset.tiles = e[{e}];")
    H.append(f"extern Image {name};")
    FREE.append(f"void res_free_{name}(void) {{ hw_free((void *) {name}_tileset.tiles); {name}_tileset.tiles = 0; }}")
    H.append(f"void res_free_{name}(void);   // its tiles, once it's no longer shown")


def tileset(name, file):
    """A font: the glyphs in order, 16 per row of the PNG."""
    img = indexed(os.path.join(GFX, file))
    w, h = img.size
    px = img.load()
    tiles = [tile_bytes(px, tx * 8, ty * 8) for ty in range(h // 8) for tx in range(w // 8)]
    e = PAK.add(b"".join(tiles))
    C.append(f"TileSet {name} = {{ {len(tiles)}, 0 }};")
    BIND.append(f"    {name}.tiles = e[{e}];")
    H.append(f"extern TileSet {name};")


def view_index(rgb):
    """Nearest colour of the view palette (make_view.py), compared on the Mega Drive's levels."""
    lv = tuple(c >> 5 for c in rgb)
    return min(range(16), key=lambda i: sum((a - b) ** 2 for a, b in zip(lv, V.PAL[i])))


def sprite(name, file, wt, ht, timer=0, view_palette=False):
    """A sprite sheet: frames of wt x ht tiles side by side (one animation, like the game's).
    view_palette: converted to the view palette (the panel avatars, colours 0-15)."""
    img = indexed(os.path.join(GFX, file))
    pal = img.getpalette()
    px = img.load()
    fw, fh = wt * 8, ht * 8
    nframes = img.size[0] // fw
    words = (fw + 15) // 16
    remap = {0: 0}
    if view_palette:
        for i in range(1, len(pal) // 3):
            remap[i] = view_index(pal[i * 3:i * 3 + 3])
    frames = []
    for f in range(nframes):
        planes = [bytearray(words * 2 * fh) for _ in range(5)]
        for y in range(fh):
            for x in range(fw):
                v = px[f * fw + x, y]
                if v == 0:
                    continue
                c = remap[v] if view_palette else v & 15
                o = y * words * 2 + x // 8
                bit = 0x80 >> (x & 7)
                for k in range(4):
                    if c & (1 << k):
                        planes[k][o] |= bit
                planes[4][o] |= bit
        e = PAK.add(b"".join(planes), chip=True)
        frames.append(e)
    palette_decl(f"{name}_pal", [md_color(*V.RGB[i]) for i in range(16)] if view_palette else png_palette(img))
    C.append(f"static BobFrame {name}_frames[{nframes}] = {{ "
             + ", ".join(f"{{ {words}, {fh}, 0 }}" for _ in frames) + " };")
    C.append(f"static const Animation {name}_anims[1] = {{ {{ {nframes}, {name}_frames, {timer} }} }};")
    C.append(f"SpriteDefinition {name} = {{ {fw}, {fh}, &{name}_pal, 1, {name}_anims, {wt * ht} }};")
    for i, e in enumerate(frames):
        BIND.append(f"    {name}_frames[{i}].data = e[{e}];")
    H.append(f"extern SpriteDefinition {name};")
    FREE.append(f"void res_free_{name}(void) {{ for (int i = 0; i < {nframes}; i++) {{ hw_free((void *) {name}_frames[i].data); {name}_frames[i].data = 0; }} }}")
    H.append(f"void res_free_{name}(void);")


SFX_RATE = 8400        # Hz; the Mega Drive played them at 13.3 kHz. Saves ~45 KB of chip RAM.


def sample(name, file):
    """A sound effect, resampled to SFX_RATE (linear interpolation), signed 8-bit."""
    with wave.open(os.path.join(SFX, file)) as w:
        assert w.getsampwidth() == 1 and w.getnchannels() == 1, file
        rate = w.getframerate()
        src = [b - 128 for b in w.readframes(w.getnframes())]
    n = len(src) * SFX_RATE // rate
    data = bytearray()
    for i in range(n):
        x = i * rate / SFX_RATE
        j = int(x)
        a, b = src[j], src[min(j + 1, len(src) - 1)]
        data.append(int(round(a + (b - a) * (x - j))) & 0xFF)
    e = PAK.add(data, chip=True)
    C.append(f"Sample {name} = {{ 0, {len(data) + (len(data) & 1)}, {SFX_RATE} }};")
    BIND.append(f"    {name}.data = e[{e}];")
    H.append(f"extern Sample {name};")


# ---------------------------------------------------------------- music

PAULA_CLOCK = 3546895
SQUARE_LEN = 16         # bytes of the looped square wave (src/amiga/audio.c)


def song(name, fn):
    """A song from tools/generate_music.py as a stream of per-tick changes for the player in
    src/amiga/audio.c, ticks at 60 Hz as on the Mega Drive:
        0x10+v, period (2 bytes)   voice v (0-2): Paula period for the square wave
        0x20+v, volume             voice v: volume 0-64
        0x30, period (2 bytes)     noise: Paula period for the noise loop
        0x31, volume               noise: volume
        0x40, n                    wait n ticks
        0xFF                       end: loop to the start
    PSG dividers become periods of the 16-byte square (the same pitch), attenuations (2 dB steps)
    volumes."""
    t = fn()
    n = t.length()
    vol_of = [round(64 * 10 ** (-a * 2 / 20)) if a < 15 else 0 for a in range(16)]
    noise_period = {0: 124, 1: 248, 2: 496, 3: 496}      # the PSG's 3 noise rates, relatively
    out = bytearray()
    cur = [None] * 4
    curv = [None] * 4
    wait = 0

    def flush_wait():
        nonlocal wait
        while wait:
            k = min(wait, 255)
            out.extend([0x40, k])
            wait -= k

    for tick in range(n):
        cmds = bytearray()
        for v in range(3):
            st = t.voice[v][tick] if tick < len(t.voice[v]) else None
            vol = 0 if st is None else vol_of[st[1]]
            if st is not None:
                period = round(PAULA_CLOCK * st[0] * 32 / (M.PSG_CLOCK * SQUARE_LEN))
                if period != cur[v]:
                    cmds += bytes([0x10 + v, period >> 8, period & 0xFF])
                    cur[v] = period
            if vol != curv[v]:
                cmds += bytes([0x20 + v, vol])
                curv[v] = vol
        st = t.drum[tick] if tick < len(t.drum) else None
        vol = 0 if st is None else vol_of[st[1]]
        if st is not None:
            period = noise_period[st[0] & 3]
            if period != cur[3]:
                cmds += bytes([0x30, period >> 8, period & 0xFF])
                cur[3] = period
        if vol != curv[3]:
            cmds += bytes([0x31, vol])
            curv[3] = vol
        if cmds:
            flush_wait()
            out += cmds
        wait += 1
    flush_wait()
    out += bytes([0x20, 0, 0x21, 0, 0x22, 0, 0x31, 0, 0xFF])
    e = PAK.add(out)
    C.append(f"const u8 *{name};")
    BIND.append(f"    {name} = e[{e}];")
    H.append(f"extern const u8 *{name};   // a song for audio_playMusic (src/amiga/audio.c)")


# ---------------------------------------------------------------- the view

def view():
    """Source data for the renderer (src/dungeon_view.c): the Amiga bakes the wall columns and the
    scaled props itself (src/amiga/view_bake.c), from the textures and these sampling tables."""
    bd, bd_mirror = V.backdrop()
    cols = [V.column_events(pc) for pc in range(V.NPAIR)]
    start, flat, bake_off = [], [], []
    total = 0
    for ev in cols:
        start.append(len(flat))
        for e in ev:
            flat.append(e)
            bake_off.append(total)
            total += e[3]
    start.append(len(flat))

    def nibbles(t, transparent=None):
        out = bytearray()
        for row in t:
            for x in range(0, V.TEX, 2):
                a, b = row[x], row[x + 1]
                a = 0 if a == transparent else a
                b = 0 if b == transparent else b
                out.append((a << 4) | b)
        return out

    textures = b"".join(nibbles(t) for t in V.TEXTURES)
    # Props: per texel row, the runs of opaque texels: [run count], then per run [first column,
    # length, the texels as nibbles (left one high)]. Mostly transparent, so a third of 64x64x4.
    props, prop_offsets = bytearray(), []
    for sprite_ in V.PROPS:
        prop_offsets.append(len(props))
        for row in sprite_:
            runs, x = [], 0
            while x < V.TEX:
                if row[x] == V.T:
                    x += 1
                    continue
                x0 = x
                while x < V.TEX and row[x] != V.T:
                    x += 1
                runs.append((x0, row[x0:x]))
            props.append(len(runs))
            for x0, texels in runs:
                props += bytes([x0, len(texels)])
                padded = list(texels) + [0] * (len(texels) & 1)
                props += bytes((padded[i] << 4) | padded[i + 1] for i in range(0, len(padded), 2))
    backdrop = bytearray()
    for row in bd:
        for x in range(0, V.VW, 2):
            backdrop.append((row[x] << 4) | row[x + 1])

    e_tex, e_props, e_bd = PAK.add(textures), PAK.add(props), PAK.add(backdrop)
    C.append("const u8 *viewTextures, *viewPropSources, *viewBackdrop;")
    BIND.append(f"    viewTextures = e[{e_tex}];")
    BIND.append(f"    viewPropSources = e[{e_props}];")
    BIND.append(f"    viewBackdrop = e[{e_bd}];")
    H.append("extern const u8 *viewTextures, *viewPropSources, *viewBackdrop;")
    PROP_OFFSETS.extend(prop_offsets)

    # Props: per distance the column pairs' texel columns and each row's texel row (bake_prop in
    # make_view.py, as tables; 255 = outside the sprite).
    geo = []
    for d in range(1, V.DMAX + 1):
        half = V.HALF_K / d
        y0 = V.CY - half
        scale = V.TEX / (2 * half)
        k0 = math.floor(-half / 2) - 1
        us = []
        for k in range(k0, -k0 + 1):
            for i in (0, 1):
                u = int(V.TEX / 2 + (2 * k + i + 0.5) * scale)
                us.append(u if 0 <= u < V.TEX else 255)
        vs = []
        for y in range(V.VH):
            v = int((y + 0.5 - y0) * scale)
            vs.append(v if 0 <= v < V.TEX else 255)
        geo.append((k0, us, vs, V.band_of(d)))

    vdp_pal = [(b << 9) | (g << 5) | (r << 1) for r, g, b in V.PAL]
    o = ["// Generated by tools/make_res.py from tools/make_view.py -- do not edit by hand.",
         "#ifndef _VIEW_GEN_H_\n#define _VIEW_GEN_H_\n",
         f"#define VIEW_W {V.VW}\n#define VIEW_H {V.VH}\n#define VIEW_TW {V.TW}\n#define VIEW_TH {V.TH}",
         f"#define VIEW_BPR {V.VW // 2}   // bytes per row of the chunky buffer (2 pixels per byte)",
         f"#define VIEW_BYTES {V.VW // 2 * V.VH}",
         f"#define VIEW_PAIRS {V.NPAIR}\n#define VIEW_DMAX {V.DMAX}\n#define VIEW_LMAX {V.LMAX}",
         f"#define VIEW_TEX {V.TEX}",
         f"#define VIEW_TEX_ROWS {total}   // bytes of one baked texture"]
    for i, n in enumerate(V.TEX_NAMES):
        o.append(f"#define {n} {i}")
    o.append(f"#define TEX_COUNT {len(V.TEX_NAMES)}")
    for i, n in enumerate(V.PROP_NAMES):
        o.append(f"#define {n} {i}")
    o.append(f"#define PROP_COUNT {len(V.PROP_NAMES)}")
    o.append(f"#define VIEW_HALF_K {int(V.HALF_K)}\n#define VIEW_CY {int(V.CY)}\n")
    o.append("// One wall crossing of a column pair's ray: the cell (d ahead, l to the right), the rows it\n"
             "// covers, its offset in a baked texture, and how to sample it: the texel columns of the\n"
             "// pair's two pixels, the distance band (shading) and the texel row in 8.8 fixed point.")
    o.append("// cell: d * (2 * VIEW_LMAX + 1) + l + VIEW_LMAX, the index into the renderer's cell tables.\n"
             "// 16 bytes, so that an event's index is a shift away from its address.")
    o.append("typedef struct { s8 d, l; u8 top, rows; u16 bake; u8 u0, u1, band, cell; u16 vstart, vstep, pad; } ViewEvent;\n")
    # The tables themselves go into view_data.c (once); view_gen.h declares them.
    o.append("typedef struct { s8 k0; u8 band; const u8 *u; const u8 *v; } ViewPropGeo;")
    o.append("extern const u16 viewPalette[16];")
    o.append("extern const u8 viewShade[4][16];")
    o.append(f"extern const u16 viewColStart[{len(start)}];")
    o.append(f"extern const ViewEvent viewEvents[{len(flat)}];")
    o.append(f"extern const ViewPropGeo viewPropGeo[{V.DMAX}];")
    o.append(f"extern const s16 viewPropCenter[{V.DMAX + 1}][{2 * V.LMAX + 1}];")
    o.append(f"extern const u16 viewPropSourceOffset[{len(V.PROPS)}];   // start of each prop in viewPropSources")

    t = ["// Generated by tools/make_res.py from tools/make_view.py -- do not edit by hand.",
         "#include <genesis.h>", "#include \"view_gen.h\"\n"]
    t.append(f"const u16 viewPalette[16] = {{\n{c_u16(vdp_pal)}\n}};")
    t.append("const u8 viewShade[4][16] = {")
    for b in range(4):
        t.append("    { " + ", ".join(str(V.SHADE[b][i]) for i in range(16)) + " },")
    t.append("};")
    t.append(f"const u16 viewColStart[{len(start)}] = {{\n" + ",".join(str(x) for x in start) + "\n};")
    t.append(f"const ViewEvent viewEvents[{len(flat)}] = {{")
    for e, off in zip(flat, bake_off):
        (d, l, top, rows, u0, u1, band, vstart, vstep) = e
        cell = d * (2 * V.LMAX + 1) + l + V.LMAX
        t.append("    {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, 0}," % (d, l, top, rows, off, u0, u1, band, cell, vstart, vstep))
    t.append("};")
    for i, (k0, us, vs, band) in enumerate(geo):
        t.append(f"static const u8 viewPropU{i}[{len(us)}] = {{" + ",".join(map(str, us)) + "};")
        t.append(f"static const u8 viewPropV{i}[{len(vs)}] = {{" + ",".join(map(str, vs)) + "};")
    t.append(f"const ViewPropGeo viewPropGeo[{V.DMAX}] = {{")
    for i, (k0, us, vs, band) in enumerate(geo):
        t.append(f"    {{ {k0}, {band}, viewPropU{i}, viewPropV{i} }},")
    t.append("};")
    t.append(f"const s16 viewPropCenter[{V.DMAX + 1}][{2 * V.LMAX + 1}] = {{")
    for d in range(V.DMAX + 1):
        t.append("    { " + ", ".join(str(V.prop_center(d, l) if d else 0) for l in range(-V.LMAX, V.LMAX + 1)) + " },")
    t.append("};")
    t.append(f"const u16 viewPropSourceOffset[{len(PROP_OFFSETS)}] = {{" + ",".join(map(str, PROP_OFFSETS)) + "};")
    with open(os.path.join(GEN, "view_data.c"), "w") as f:
        f.write("\n".join(t) + "\n")
    o.append("\n#endif")
    with open(os.path.join(GEN, "view_gen.h"), "w") as f:
        f.write("\n".join(o) + "\n")


# ---------------------------------------------------------------- the game's resources (as res/resources.res)

def main():
    os.makedirs(os.path.join(GEN, "res"), exist_ok=True)

    image("rcd_logo", "rcd_logo.png")
    image("title_image", "title.png")

    for n in ("wir", "laezel", "shadowheart"):
        sprite(f"avatar_{n}_sprite", f"avatar_{n}.png", 3, 3, view_palette=True)

    sprite("fig_imp_sprite", "fig_imp.png", 4, 6, timer=6)
    sprite("fig_laezel_sprite", "fig_laezel.png", 6, 12)
    sprite("fig_shadowheart_sprite", "fig_shadowheart.png", 6, 12)
    sprite("fig_laezel_bust_sprite", "fig_laezel_bust.png", 10, 12)
    sprite("fig_shadowheart_bust_sprite", "fig_shadowheart_bust.png", 10, 12)
    sprite("fig_arrow_sprite", "fig_arrow.png", 1, 1)
    sprite("fig_hound_sprite", "fig_hound.png", 6, 5)
    sprite("fig_cambion_sprite", "fig_cambion.png", 6, 12)
    sprite("fig_zhalk_sprite", "fig_zhalk.png", 8, 14)
    sprite("fig_mindflayer_bust_sprite", "fig_mindflayer_bust.png", 10, 12)

    tileset("font_de", "font_de.png")

    view()

    for cls in ("fighter", "rogue", "mage"):
        for i in (1, 2, 3):
            sprite(f"portrait_{cls}_{i}", f"portrait_{cls}_{i}.png", 10, 12)
            sprite(f"hero_{cls}_{i}", f"hero_{cls}_{i}.png", 3, 3, view_palette=True)

    for n in ("hit", "miss", "hurt", "spell", "fire", "heal", "explosion", "door", "land", "dice",
              "victory", "gameover", "menu", "quake", "item", "intro"):
        sample(f"sfx_{n}", f"sfx_{n}.wav")

    for n in ("title", "dungeon", "combat", "bridge", "ending"):
        song(f"music_{n}", getattr(M, f"song_{n}"))

    size = PAK.write(os.path.join(OUT, "dnd.pak"))

    with open(os.path.join(GEN, "res", "resources.h"), "w") as f:
        f.write("// Generated by tools/make_res.py -- do not edit by hand.\n"
                "#ifndef _RES_RESOURCES_H_\n#define _RES_RESOURCES_H_\n\n#include <genesis.h>\n\n"
                "typedef struct { const u8 *data; u32 size; u16 rate; } Sample;   // signed 8-bit, chip RAM\n\n"
                + "\n".join(H) + "\n\n#endif\n")
    with open(os.path.join(GEN, "resources.c"), "w") as f:
        f.write("// Generated by tools/make_res.py -- do not edit by hand.\n"
                "#include \"res/resources.h\"\n\nvoid hw_free(void *p);\n\n" + "\n".join(C)
                + "\n\nvoid res_bind(u8 *const *e)\n{\n" + "\n".join(BIND) + "\n}\n\n" + "\n".join(FREE) + "\n")
    chip = sum(len(d) for d, c in PAK.entries if c)
    raw = sum(len(d) for d, c in PAK.entries)
    print(f"out/dnd.pak: {size} bytes ({raw} unpacked), {len(PAK.entries)} entries ({chip} bytes for chip RAM)")


if __name__ == "__main__":
    main()
