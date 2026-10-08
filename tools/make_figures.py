#!/usr/bin/env python3
"""Generates the figure sprites: characters and monsters shown in front of the first-person view
during dialogue scenes and combat (hardware sprites on PAL2, not part of the view's bitmap).

Every figure has its own 16-colour palette, loaded into PAL2 when it's shown (src/figures.c) --
two different figures are never on screen at the same time. Conventions shared by all palettes:
index 0 transparent, 1 outline, 15 a bright accent (the combat target marker is drawn in it).

    res/gfx/fig_imp.png              Niederer Kobold (imp), 32x48, 3 frames of wing beat
    res/gfx/fig_hound.png            Höllenhund, 48x40
    res/gfx/fig_cambion.png          Cambion, 48x96
    res/gfx/fig_zhalk.png            Kommandant Zhalk, 64x112: the Everburn Blade in his hand; disarmed
    res/gfx/fig_mindflayer_bust.png  the mind flayer on the bridge, 80x96 close-up
    res/gfx/fig_laezel.png           Lae'zel, full figure 48x96 (she lands in front of you)
    res/gfx/fig_laezel_bust.png      Lae'zel, 80x96 close-up while she speaks
    res/gfx/fig_shadowheart.png      Schattenherz, full figure 48x96
    res/gfx/fig_shadowheart_bust.png Schattenherz, 80x96 close-up while she speaks
    res/gfx/fig_arrow.png            8x8 target marker for combat menus

The looks follow the companions' BG3 designs as described in BeschreibungInhaltVerticalSlice.md
and the reference artworks in screenso/ (drawn from scratch here, nothing traced): Lae'zel with
yellow-green skin, dark spots and a black war-paint band across amber eyes, reddish-brown hair in
a high ponytail with metal rings, ornate silver armour with red stones, the greatsword over her
shoulder; Schattenherz pale-skinned (the violet in her artworks is light and clothing, not
her skin), black hair with a straight fringe and a long
braid in gold rings, a gold circlet, silver armour with swept, wing-like pauldrons, violet cloth.

Sizes are what's shown on screen: a figure one cell ahead is about as tall as the wall there
(112 px), so a human is ~96 px, an imp (small, hovering) 48 px.
"""
import math
import os
import random

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "res", "gfx")
TR, OUT_C = 0, 1

# ---------------------------------------------------------------- palettes (index 0: transparent)
# All creatures of Avernus share one palette, so mixed fights work (PAL2 holds one palette at a
# time): imps, hellhounds, cambions, Zhalk. Colours 8-10 are the fire ramp; combat.c cycles them
# during Zhalk's fight, so his sword burns (flames anywhere else flicker along).
HELL_PAL = [
    (0xFF, 0x00, 0xFF), (0x10, 0x08, 0x08),
    (0xE8, 0x60, 0x48), (0xB8, 0x30, 0x24), (0x70, 0x14, 0x14),   # blood-red skin
    (0xE4, 0xE4, 0xEC), (0xA0, 0xA0, 0xB0), (0x58, 0x58, 0x68),   # silver armour (Zhalk, cambions)
    (0xF8, 0xE8, 0x80), (0xF0, 0x98, 0x30), (0xD0, 0x40, 0x20),   # fire (cycled)
    (0xD0, 0xB8, 0x98),                                           # horn, bone, teeth
    (0x2C, 0x24, 0x2C),                                           # black: hide, hair, boots
    (0x48, 0x24, 0x4C),                                           # dark violet cloth
    (0xD0, 0x58, 0x34),                                           # wing membrane, orange-red
    (0xF8, 0xF0, 0x80),                                           # glowing eyes (accent)
]
(H_RED_H, H_RED, H_RED_D, H_SILVER_H, H_SILVER, H_SILVER_D, H_FIRE_Y, H_FIRE_O, H_FIRE_R, H_HORN,
 H_BLACK, H_CLOTH, H_WING, H_EYE) = range(2, 16)

MF_PAL = [
    (0xFF, 0x00, 0xFF), (0x14, 0x0C, 0x1C),
    (0xC8, 0x98, 0xD0), (0x98, 0x68, 0xA8), (0x60, 0x3C, 0x70),   # mauve skin
    (0x50, 0x40, 0x88), (0x30, 0x24, 0x58), (0x18, 0x10, 0x30),   # indigo robe
    (0xF8, 0xD8, 0x70), (0xB8, 0x84, 0x34),                       # gold
    (0xF0, 0xF0, 0xF8),                                           # pupil-less eyes
    (0x40, 0x18, 0x30),                                           # maw
    (0x90, 0x70, 0xF0), (0x00, 0x00, 0x00), (0x00, 0x00, 0x00),
    (0xC8, 0xB8, 0xFF),                                           # psionic glow (accent)
]
(M_SKIN_H, M_SKIN, M_SKIN_L, M_ROBE_H, M_ROBE, M_ROBE_D, M_GOLD_H, M_GOLD, M_EYE, M_MAW,
 M_GLOW) = range(2, 13)

LZ_PAL = [
    (0xFF, 0x00, 0xFF), (0x18, 0x10, 0x10),
    (0xD8, 0xD4, 0x80), (0xA8, 0xA8, 0x50), (0x70, 0x70, 0x30),   # skin: light, mid, shade
    (0x30, 0x24, 0x18),                                           # war paint, spots, brows
    (0xF8, 0xB0, 0x28),                                           # amber eyes
    (0xB8, 0x68, 0x40), (0x84, 0x40, 0x28), (0x50, 0x22, 0x18),   # hair
    (0xF4, 0xF4, 0xFC), (0xA8, 0xB0, 0xC4), (0x5C, 0x64, 0x78),   # silver armour
    (0xD8, 0x20, 0x28),                                           # red stones
    (0x5C, 0x34, 0x24),                                           # leather
    (0xF8, 0xE8, 0x90),                                           # bright accent
]
(L_SKIN_H, L_SKIN, L_SKIN_L, L_PAINT, L_AMBER, L_HAIR_H, L_HAIR, L_HAIR_L, L_STEEL_H, L_STEEL,
 L_STEEL_L, L_GEM, L_LEATHER, L_ACCENT) = range(2, 16)

SH_PAL = [
    (0xFF, 0x00, 0xFF), (0x14, 0x0C, 0x1C),
    (0xF8, 0xE4, 0xD4), (0xE4, 0xC4, 0xAC), (0xB4, 0x8C, 0x78),   # pale skin (natural, warm)
    (0x48, 0x48, 0x70), (0x10, 0x10, 0x28),                       # hair: blue sheen, black (the
                                                                  # Mega Drive's 3 bits per channel
                                                                  # turn near-black violet magenta)
    (0xF8, 0xDC, 0x78), (0xB8, 0x84, 0x34),                       # gold
    (0xEC, 0xF0, 0xF8), (0xA4, 0xAC, 0xBC), (0x58, 0x60, 0x74),   # silver armour
    (0x78, 0x4C, 0xA8), (0x40, 0x28, 0x64),                       # violet cloth
    (0xB8, 0x58, 0x78),                                           # lips
    (0x7C, 0xA0, 0x88),                                           # eyes (bright accent)
]
(S_SKIN_H, S_SKIN, S_SKIN_L, S_HAIR_H, S_HAIR, S_GOLD_H, S_GOLD, S_STEEL_H, S_STEEL, S_STEEL_L,
 S_PURPLE, S_PURPLE_D, S_LIP, S_EYE) = range(2, 16)


# ---------------------------------------------------------------- drawing
class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [[TR] * w for _ in range(h)]

    def set(self, x, y, c):
        x, y = int(x), int(y)
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = c

    def get(self, x, y):
        return self.px[y][x] if 0 <= x < self.w and 0 <= y < self.h else TR

    def rect(self, x0, y0, x1, y1, c):
        for y in range(int(y0), int(y1)):
            for x in range(int(x0), int(x1)):
                self.set(x, y, c)

    def ellipse(self, cx, cy, rx, ry, c):
        for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
            for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
                if ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1:
                    self.set(x, y, c)

    def poly(self, pts, c):
        """Filled polygon (even-odd scanline)."""
        for y in range(self.h):
            xs = []
            for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]):
                if (y0 <= y + 0.5 < y1) or (y1 <= y + 0.5 < y0):
                    xs.append(x0 + (y + 0.5 - y0) * (x1 - x0) / (y1 - y0))
            xs.sort()
            for a, b in zip(xs[::2], xs[1::2]):
                for x in range(int(round(a)), int(round(b))):
                    self.set(x, y, c)

    def line(self, x0, y0, x1, y1, c, w=1):
        n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for i in range(n + 1):
            t = i / n
            x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
            for d in range(w):
                self.set(x + d, y, c)

    def shade_region(self, mask_c, hi, mid, lo, cx, cy, rx, ry, light=(-0.7, -0.5)):
        """Recolours every pixel of colour `mask_c` by lighting from the upper left: a round-ish
        body centred at (cx, cy) gets a lit side, a mid tone and a shadow side, with a one-pixel
        dithered seam between them."""
        for y in range(self.h):
            for x in range(self.w):
                if self.px[y][x] != mask_c:
                    continue
                v = light[0] * (x + 0.5 - cx) / rx + light[1] * (y + 0.5 - cy) / ry
                d = 0.08 if (x + y) % 2 else -0.08
                self.px[y][x] = hi if v + d > 0.45 else lo if v + d < -0.4 else mid

    def outline(self):
        """Dark outline around every opaque shape, so figures read against the busy view."""
        src = [row[:] for row in self.px]
        for y in range(self.h):
            for x in range(self.w):
                if src[y][x] != TR:
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < self.w and 0 <= yy < self.h and src[yy][xx] not in (TR, OUT_C):
                        self.px[y][x] = OUT_C
                        break


TMP = 99   # scratch colour for shade_region


def save(frames, name, palette):
    w, h = frames[0].w, frames[0].h
    img = Image.new("P", (w * len(frames), h), 0)
    img.putpalette([v for c in palette for v in c] + [0] * (768 - 3 * len(palette)))
    for i, f in enumerate(frames):
        for y in range(h):
            for x in range(w):
                img.putpixel((i * w + x, y), f.px[y][x])
    path = os.path.join(OUT, name)
    img.save(path)
    print("wrote", os.path.normpath(path), img.size)


def to_rgba(canvas, palette):
    """For tools/make_avatar.py: a canvas as an RGBA image (transparent where index 0)."""
    img = Image.new("RGBA", (canvas.w, canvas.h))
    img.putdata([(0, 0, 0, 0) if c == TR else palette[c] + (255,) for row in canvas.px for c in row])
    return img


# ---------------------------------------------------------------- creatures of Avernus
def bat_wing(c, root, tip, low, bones=3):
    """A bat wing: orange-red membrane, darker finger bones, a clawed tip."""
    c.poly([root, tip, low], H_WING)
    for k in range(bones):                                # the wing's finger bones
        t = (k + 1) / (bones + 1)
        c.line(root[0], root[1], tip[0] + (low[0] - tip[0]) * t, tip[1] + (low[1] - tip[1]) * t, H_RED_D)
    c.line(root[0], root[1], tip[0], tip[1], H_RED)
    c.set(tip[0], tip[1] - 1, H_HORN)                     # claw at the top


def horn(c, x, y, dx, length, thick, col=H_HORN, dark=H_RED_D):
    """A horn curving up and outwards from (x, y); dx = -1 / +1 for the side."""
    for i in range(length):
        t = i / max(1, length - 1)
        r = thick * (1 - t) + 0.6
        c.ellipse(x + dx * (length * 0.35 * t + length * 0.25 * t * t), y - length * 0.9 * t + length * 0.25 * t * t,
                  r, r, col if i % 3 else dark)


def imp(frame):
    """Tiny crimson devil: goat horns, bat wings (3-frame beat), thin arrow tail, iron trident."""
    c = Canvas(32, 48)
    wing_y = (0, 6, 12)[frame]                            # wing tips: up, level, down
    for side in (-1, 1):
        bat_wing(c, (16 + side * 4, 20), (16 + side * 15, 8 + wing_y), (16 + side * 12, 24 + wing_y // 2), 2)
    for i in range(16):                                   # tail, curling behind
        c.set(16 + int(6 * math.sin(i / 3.0)), 30 + i, H_RED_D)
    c.poly([(14, 45), (19, 45), (16, 41)], H_RED_D)       # arrow tip of the tail
    c.ellipse(16, 27, 6, 7, TMP)                          # body
    c.rect(12, 32, 15, 38, H_RED_D)                       # legs
    c.rect(18, 32, 21, 38, H_RED_D)
    c.ellipse(16, 15, 6, 6, TMP)                          # head
    c.shade_region(TMP, H_RED_H, H_RED, H_RED_D, 16, 20, 7, 12)
    c.poly([(11, 11), (9, 4), (13, 9)], H_HORN)           # goat horns
    c.poly([(21, 11), (23, 4), (19, 9)], H_HORN)
    c.set(13, 15, H_EYE)                                  # eyes
    c.set(18, 15, H_EYE)
    c.rect(14, 18, 19, 19, H_RED_D)                       # grin
    c.set(15, 18, H_HORN)
    c.set(17, 18, H_HORN)
    c.rect(26, 6, 27, 40, H_SILVER_D)                     # trident shaft
    for x in (24, 26, 28):
        c.rect(x, 3, x + 1, 8, H_SILVER)
    c.rect(24, 7, 29, 8, H_SILVER_H)
    c.rect(21, 24, 26, 26, H_RED)                         # arm holding it
    c.outline()
    return c


def hound():
    """Hellhound: a black hound with glowing magma cracks, fire in its jaws, a flame for a tail."""
    rng = random.Random(31)
    c = Canvas(48, 40)
    for i in range(10):                                   # tail of flame
        c.ellipse(42 + i * 0.3, 18 - i * 1.2, 2.5 - i * 0.2, 2, (H_FIRE_R, H_FIRE_O, H_FIRE_Y)[i % 3])
    c.ellipse(28, 22, 15, 9, TMP)                         # body
    c.shade_region(TMP, H_SILVER_D, H_BLACK, OUT_C, 26, 18, 16, 9)
    for x, bend in ((18, -2), (24, 1), (34, -1), (40, 2)):    # legs, claws
        c.rect(x, 26, x + 4, 37, H_BLACK)
        c.rect(x + bend, 37, x + bend + 5, 39, H_SILVER_D)
    for _ in range(6):                                    # magma cracks
        x, y = rng.randint(18, 40), rng.randint(16, 26)
        for k in range(rng.randint(4, 8)):
            c.set(x, y, H_FIRE_O if k % 3 else H_FIRE_Y)
            x += rng.choice((1, 1, 0))
            y += rng.choice((-1, 0, 1))
    for x in range(16, 40, 3):                            # smouldering ridge on the back
        c.poly([(x, 15), (x + 2, 9 - (x % 2) * 2), (x + 3, 15)], H_FIRE_R)
    c.ellipse(11, 17, 9, 8, TMP)                          # head
    c.shade_region(TMP, H_SILVER_D, H_BLACK, OUT_C, 9, 14, 9, 8)
    c.poly([(5, 12), (7, 2), (10, 11)], H_BLACK)          # ears
    c.poly([(13, 11), (16, 3), (17, 12)], H_BLACK)
    c.rect(1, 18, 12, 23, H_BLACK)                        # snout
    c.rect(1, 22, 12, 26, H_FIRE_O)                       # open jaws full of fire
    c.rect(2, 23, 10, 25, H_FIRE_Y)
    for x in range(2, 12, 2):
        c.set(x, 22, H_HORN)                              # teeth
        c.set(x + 1, 26, H_HORN)
    c.set(7, 15, H_EYE)
    c.set(12, 15, H_EYE)
    c.outline()
    return c


def silver_plate(c, pts, cx, cy, rx, ry):
    c.poly(pts, TMP)
    c.shade_region(TMP, H_SILVER_H, H_SILVER, H_SILVER_D, cx, cy, rx, ry)


def cambion():
    """Cambion (after the Monster Manual): red skin, black hair tied back, small horns, red bat
    wings, a tail; silver scale armour over a dark violet tunic; a spear with a silver head."""
    c = Canvas(48, 96)
    for side in (-1, 1):
        bat_wing(c, (24 + side * 6, 32), (24 + side * 23, 4), (24 + side * 20, 58))
    for i in range(30):                                   # tail, curling out to the side
        t = i / 29
        c.ellipse(30 + 14 * t, 64 + 20 * t - 12 * t * t, 1.8 - t, 1.5, H_RED)
    c.poly([(16, 56), (32, 56), (35, 80), (13, 80)], H_CLOTH)   # tunic, ragged hem
    for x in range(14, 35, 4):
        c.poly([(x, 80), (x + 2, 86), (x + 4, 80)], H_CLOTH)
    c.rect(18, 76, 23, 90, H_BLACK)                       # legs, dark cloth
    c.rect(25, 76, 30, 90, H_BLACK)
    silver_plate(c, [(16, 84), (24, 84), (24, 93), (16, 93)], 18, 86, 6, 6)   # greaves, boots
    silver_plate(c, [(24, 84), (32, 84), (32, 93), (24, 93)], 26, 86, 6, 6)
    c.rect(16, 93, 24, 96, H_BLACK)
    c.rect(24, 93, 32, 96, H_BLACK)
    silver_plate(c, [(15, 32), (33, 32), (32, 56), (16, 56)], 20, 40, 12, 14)   # scale armour
    for y in range(36, 56, 3):                            # scales
        for x in range(17 + (y // 3) % 2 * 2, 32, 4):
            c.set(x, y, H_SILVER_D)
    silver_plate(c, [(14, 52), (34, 52), (32, 58), (16, 58)], 20, 54, 12, 4)   # belt plate
    for side in (-1, 1):                                  # pointed pauldrons
        silver_plate(c, [(24 + side * 6, 31), (24 + side * 15, 30), (24 + side * 17, 25), (24 + side * 12, 37)],
                     24 + side * 10, 30, 7, 5)
    c.rect(10, 36, 14, 58, H_RED)                         # red arms with dark tattoos
    c.rect(34, 36, 38, 58, H_RED_D)
    for y in range(40, 56, 4):
        c.set(11, y, H_BLACK); c.set(12, y + 1, H_BLACK); c.set(35, y + 1, H_BLACK)
    silver_plate(c, [(9, 46), (15, 46), (15, 52), (9, 52)], 11, 48, 3, 3)      # bracers
    silver_plate(c, [(33, 46), (39, 46), (39, 52), (33, 52)], 35, 48, 3, 3)
    c.line(44, 2, 34, 94, H_BLACK, 2)                     # spear, held diagonally...
    c.poly([(43, 0), (47, 0), (46, 9), (43, 9)], H_SILVER)    # ...with a silver head
    c.line(47, 0, 46, 9, H_SILVER_H)
    c.rect(35, 54, 40, 58, H_RED)                         # hand on the shaft
    c.rect(21, 25, 27, 32, H_RED_D)                       # neck
    c.ellipse(24, 18, 6, 8, TMP)                          # head
    c.shade_region(TMP, H_RED_H, H_RED, H_RED_D, 22, 16, 7, 9)
    c.ellipse(24, 12, 7, 5, H_BLACK)                      # black hair, slicked back
    c.rect(27, 12, 32, 30, H_BLACK)                       # ponytail
    c.poly([(17, 17), (13, 13), (18, 21)], H_RED)         # pointed ears
    c.poly([(31, 17), (35, 13), (30, 21)], H_RED_D)
    horn(c, 20, 11, -1, 5, 1.0)                           # small horns
    horn(c, 28, 11, 1, 5, 1.0)
    c.rect(20, 17, 23, 18, H_EYE)                         # glowing eyes
    c.rect(25, 17, 28, 18, H_EYE)
    c.rect(21, 22, 27, 23, H_RED_D)                       # snarl
    c.outline()
    return c


def zhalk(armed=True):
    """Kommandant Zhalk (after BG3): a hulking cambion with red skin, great horns sweeping up,
    huge orange-red wings, spiked silver armour with an open chest and riveted pauldrons, and the
    Everburn Blade -- a long glowing golden blade, fire licking round the hilt (the fire ramp is
    cycled). Frame 2 (armed=False): Schattenherz's "Befehl: Fallenlassen!" -- an empty fist."""
    c = Canvas(64, 112)
    for side in (-1, 1):
        bat_wing(c, (32 + side * 9, 38), (32 + side * 31, 2), (32 + side * 27, 88), 4)
    c.rect(22, 78, 29, 102, TMP)                          # legs in silver greaves
    c.rect(35, 78, 42, 102, TMP)
    c.shade_region(TMP, H_SILVER_H, H_SILVER, H_SILVER_D, 28, 86, 10, 14)
    c.rect(20, 102, 30, 108, H_SILVER_D)
    c.rect(34, 102, 44, 108, H_SILVER_D)
    c.rect(21, 108, 29, 111, H_RED)                       # bare red feet
    c.rect(35, 108, 43, 111, H_RED)
    c.poly([(20, 62), (44, 62), (46, 84), (18, 84)], TMP) # plated skirt
    c.shade_region(TMP, H_SILVER_H, H_SILVER, H_SILVER_D, 28, 70, 14, 12)
    for y in range(66, 84, 4):
        c.line(19, y, 45, y, H_SILVER_D)
    c.poly([(21, 40), (43, 40), (44, 62), (20, 62)], H_RED)   # bare chest...
    c.shade_region(H_RED, H_RED_H, H_RED, H_RED_D, 28, 48, 12, 12)
    c.line(32, 44, 32, 60, H_RED_D)
    c.rect(22, 56, 42, 62, H_SILVER_D)                    # ...a spiked belt
    for x in range(22, 42, 4):
        c.poly([(x, 56), (x + 2, 52), (x + 3, 56)], H_SILVER)
    for side in (-1, 1):                                  # huge riveted pauldrons with spikes
        cx = 32 + side * 14
        c.ellipse(cx, 40, 11, 8, TMP)
        c.ellipse(cx + side * 2, 46, 8, 7, TMP)
        c.shade_region(TMP, H_SILVER_H, H_SILVER, H_SILVER_D, cx - 3, 38, 11, 8)
        for k in range(4):
            c.poly([(cx - 7 + k * 4, 35), (cx - 6 + k * 4 + side * 2, 26 - k), (cx - 4 + k * 4, 35)], H_SILVER_H)
        for rx, ry in ((-4, 40), (0, 43), (4, 40), (side * 3, 47)):
            c.set(cx + rx, ry, H_SILVER_D)                # rivet holes
    c.rect(6, 48, 12, 72, H_RED)                          # arms, bracers
    c.rect(52, 48, 58, 72, H_RED_D)
    c.rect(6, 60, 12, 68, H_SILVER)
    c.rect(52, 60, 58, 68, H_SILVER)
    if armed:
        # the Everburn Blade, held across the body: glowing gold, flames round the hilt
        for i in range(62):
            x, y = 10 + i * 0.86, 76 - i * 0.62
            c.rect(x, y, x + 3, y + 2, H_FIRE_Y if i % 5 else H_SILVER_H)
            if i < 14 and i % 2 == 0:
                c.poly([(x, y), (x - 2 + (i % 4), y - 6), (x + 3, y)], (H_FIRE_O, H_FIRE_R)[(i // 2) % 2])
        c.rect(7, 73, 15, 79, H_SILVER_D)                 # crossguard and fist
        c.rect(4, 76, 11, 81, H_RED)
    else:
        c.rect(5, 72, 13, 79, H_RED)                      # the empty fist, clenched in rage
        c.line(6, 75, 12, 75, H_RED_D)
        c.line(6, 77, 12, 77, H_RED_D)
        c.rect(52, 72, 59, 78, H_RED_D)                   # and the other one
    c.rect(26, 28, 38, 40, H_RED_D)                       # neck
    c.ellipse(32, 21, 9, 11, TMP)                         # head
    c.shade_region(TMP, H_RED_H, H_RED, H_RED_D, 30, 19, 10, 12)
    c.poly([(23, 20), (18, 15), (24, 24)], H_RED)         # pointed ears
    c.poly([(41, 20), (46, 15), (40, 24)], H_RED_D)
    horn(c, 26, 12, -1, 18, 2.6, H_RED_D, H_BLACK)        # great horns sweeping up
    horn(c, 38, 12, 1, 18, 2.6, H_RED_D, H_BLACK)
    c.rect(26, 20, 30, 22, H_EYE)                         # glowing eyes
    c.rect(34, 20, 38, 22, H_EYE)
    c.rect(28, 27, 37, 28, H_RED_D)                       # a cold smile
    c.outline()
    return c


def mindflayer_bust():
    """The mind flayer on the bridge: bulging mauve head, pupil-less white eyes, four tentacles
    over the chest, a towering indigo collar with gold trim."""
    c = Canvas(80, 96)
    c.poly([(8, 16), (24, 4), (40, 8), (56, 4), (72, 16), (66, 70), (14, 70)], TMP)   # collar
    c.shade_region(TMP, M_ROBE_H, M_ROBE, M_ROBE_D, 36, 30, 30, 30)
    for i in range(7):                                    # gold trim along its edge
        c.line(8 + i * 0, 16, 24, 4, M_GOLD)
    c.line(8, 16, 24, 4, M_GOLD_H); c.line(24, 4, 40, 8, M_GOLD); c.line(40, 8, 56, 4, M_GOLD)
    c.line(56, 4, 72, 16, M_GOLD_H)
    c.poly([(14, 70), (66, 70), (78, 96), (2, 96)], TMP)   # robe over the shoulders
    c.shade_region(TMP, M_ROBE_H, M_ROBE, M_ROBE_D, 34, 80, 30, 14)
    c.line(40, 72, 40, 96, M_GOLD)
    c.line(20, 72, 12, 96, M_GOLD)
    c.line(60, 72, 68, 96, M_GOLD)
    c.ellipse(40, 32, 18, 21, TMP)                        # the bulging head
    c.shade_region(TMP, M_SKIN_H, M_SKIN, M_SKIN_L, 36, 26, 20, 22)
    for side in (-1, 1):                                  # pupil-less eyes, slanted
        for k in range(7):
            c.set(40 + side * (5 + k), 33 - k // 3, M_EYE)
            c.set(40 + side * (5 + k), 34 - k // 3, M_EYE)
        c.line(40 + side * 4, 31, 40 + side * 12, 28, M_SKIN_L)
    c.ellipse(40, 45, 7, 4, M_MAW)                        # the maw...
    for k, dx in enumerate((-9, -3, 3, 9)):               # ...and four tentacles over the chest
        for i in range(34):
            t = i / 33
            x = 40 + dx * (1 + 0.4 * t) + 3 * math.sin(t * 5 + k)
            y = 44 + i
            c.ellipse(x, y, 3.2 - 1.6 * t, 1.6, M_SKIN if (i // 4) % 2 else M_SKIN_L)
            if i % 5 == 2:
                c.set(x, y, M_SKIN_H)
    for gx, gy in ((10, 40), (70, 36), (64, 58), (14, 60)):   # psionic glow
        c.set(gx, gy, 15)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c.set(gx + dx, gy + dy, M_GLOW)
    c.outline()
    return c


# ---------------------------------------------------------------- shared bust geometry
def face(c, cx, cy, rx, ry, jaw=0.45):
    """An oval face narrowing towards the chin, filled with TMP (shade it afterwards)."""
    for y in range(int(cy - ry), int(cy + ry) + 1):
        k = (y + 0.5 - cy) / ry
        w = rx * math.sqrt(max(0.0, 1 - k * k))
        if k > 0.1:
            w -= (k - 0.1) * rx * jaw
        for x in range(int(cx - w), int(cx + w) + 1):
            c.set(x, y, TMP)


def ear(c, base_top, base_bot, tip, col, edge):
    c.poly([base_top, tip, base_bot], col)
    c.line(base_top[0], base_top[1], tip[0], tip[1], edge)


def almond_eye(c, x, y, iris, lid, white=None, slit=None, glint=None):
    """A 6x3 almond eye with its left end at x: upper lid, iris, optional slit pupil / glint."""
    c.rect(x + 1, y - 1, x + 6, y, lid)                   # upper lid
    c.set(x, y, lid)
    c.rect(x + 1, y, x + 6, y + 2, white if white is not None else iris)
    c.rect(x + 2, y, x + 5, y + 2, iris)
    if slit is not None:
        c.rect(x + 3, y, x + 4, y + 2, slit)
    if glint is not None:
        c.set(x + 2, y, glint)


# ---------------------------------------------------------------- Lae'zel
def laezel_bust():
    rng = random.Random(3)
    c = Canvas(80, 96)
    # greatsword behind her right shoulder (the viewer's left): grip, crossguard, red stone
    c.line(19, 2, 13, 24, L_LEATHER, 3)
    for y in range(4, 24, 3):
        c.line(19 - (y - 2) * 0.27, y, 16 - (y - 2) * 0.27, y, L_HAIR_L)   # grip wrapping
    c.poly([(26, 24), (4, 20), (3, 24), (25, 28)], L_STEEL)
    c.line(26, 24, 4, 20, L_STEEL_H)
    c.ellipse(14, 23, 2, 2, L_GEM)
    c.ellipse(19, 2, 3, 3, L_STEEL)
    # high ponytail: gathered at the crown, falling down behind her on the viewer's right
    for i in range(28):
        t = i / 27
        x = 46 + 20 * t - 3 * math.sin(t * 3)
        y = 14 + 60 * t
        c.ellipse(x, y, 4.5 - 2 * t, 4, L_HAIR if i % 6 else L_STEEL)   # metal rings
    # ears: long and pointed, swept back and up along the head
    ear(c, (28, 35), (28, 45), (13, 25), L_SKIN, L_SKIN_H)
    ear(c, (52, 35), (52, 45), (67, 25), L_SKIN_L, L_SKIN)
    # neck and face
    c.rect(33, 52, 47, 70, TMP)
    face(c, 40, 40, 14, 18)
    c.shade_region(TMP, L_SKIN_H, L_SKIN, L_SKIN_L, 38, 40, 16, 22)
    c.rect(34, 58, 46, 61, L_SKIN_L)                      # shadow under the chin
    # hair pulled back tight, hairline arching down to the temples
    for y in range(10, 40):
        for x in range(20, 60):
            inside = ((x + 0.5 - 40) / 16) ** 2 + ((y + 0.5 - 30) / 16) ** 2 <= 1
            if inside and y < 24 + abs(x - 40) * 0.55:
                c.set(x, y, TMP)
    c.shade_region(TMP, L_HAIR_H, L_HAIR, L_HAIR_L, 36, 20, 16, 12)
    for x0 in range(25, 57, 4):                           # strands combed back to the crown
        x1, y0 = 40 + (x0 - 40) * 0.35, 23 + abs(x0 - 40) * 0.5
        for i in range(12):
            t = i / 11
            x, y = x0 + (x1 - x0) * t, y0 + (13 - y0) * t
            if c.get(int(x), int(y)) in (L_HAIR, L_HAIR_H):
                c.set(x, y, L_HAIR_L)
    # spots on forehead, temples and cheeks (mirrored, a little irregular)
    for _ in range(26):
        x, y = rng.randint(2, 13), rng.choice((rng.randint(27, 34), rng.randint(45, 53)))
        for sx in (40 - x, 40 + x - 1):
            if c.get(sx, y) in (L_SKIN_H, L_SKIN, L_SKIN_L) and rng.random() < 0.8:
                c.set(sx, y, L_PAINT)
                if rng.random() < 0.4:
                    c.set(sx + 1, y, L_PAINT)
    # black war paint: a band from the temples under the eyes, thinning towards the nose and
    # breaking up into spots
    for x in range(26, 55):
        k = abs(x - 40)
        if k < 4:
            continue
        top = 41 if k > 9 else 42
        bot = top + (4 if k > 10 else 3 if k > 6 else 2) + rng.randint(0, 1)
        for y in range(top, bot):
            if c.get(x, y) != TR and rng.random() < (0.95 if k > 6 else 0.6):
                c.set(x, y, L_PAINT)
    # fierce brows, angled down towards the nose
    c.line(29, 35, 37, 37, L_PAINT, 1)
    c.line(29, 36, 37, 38, L_PAINT, 1)
    c.line(43, 37, 51, 35, L_PAINT, 1)
    c.line(43, 38, 51, 36, L_PAINT, 1)
    # amber eyes with slit pupils
    almond_eye(c, 31, 40, L_AMBER, OUT_C, slit=OUT_C, glint=L_ACCENT)
    almond_eye(c, 43, 40, L_AMBER, OUT_C, slit=OUT_C, glint=L_ACCENT)
    # flat nose: two nostrils, a little shade under the tip
    c.rect(38, 48, 43, 49, L_SKIN_L)
    c.set(38, 49, OUT_C)
    c.set(42, 49, OUT_C)
    # mouth, and the scar from forehead across nose to lip
    c.rect(36, 54, 45, 55, L_SKIN_L)
    c.rect(36, 55, 45, 56, OUT_C)
    c.rect(37, 56, 44, 57, L_HAIR_H)
    c.line(44, 28, 37, 58, L_HAIR_H)
    # high leather collar, stitched
    c.poly([(30, 62), (50, 62), (53, 72), (27, 72)], L_LEATHER)
    for x in range(31, 50, 3):
        c.set(x, 66, L_STEEL)
    # ornate silver armour: breastplate and big curved pauldrons, red stones
    c.poly([(26, 70), (54, 70), (60, 96), (20, 96)], TMP)
    c.shade_region(TMP, L_STEEL_H, L_STEEL, L_STEEL_L, 36, 80, 18, 14)
    c.line(40, 72, 40, 95, L_STEEL_H)
    for side in (-1, 1):
        cx = 40 + side * 24
        c.ellipse(cx, 80, 17, 12, TMP)
        c.ellipse(cx, 82, 17, 12, TMP)
        c.shade_region(TMP, L_STEEL_H, L_STEEL, L_STEEL_L, cx - 6, 76, 16, 12)
        for i in range(12):                                # scrollwork
            a = i / 11 * math.pi
            c.set(cx - side * 2 + side * 8 * math.cos(a), 80 - 6 * math.sin(a) + 2, L_STEEL_H)
        c.line(cx - 15, 91, cx + 15, 91, L_STEEL_L)
        c.ellipse(cx + side * 2, 79, 2, 2, L_GEM)
    c.ellipse(40, 84, 3, 3, L_GEM)
    c.set(39, 83, L_ACCENT)
    c.outline()
    return c


def laezel():
    """Full figure: the same design at 48x96 (she lands in front of the party)."""
    c = Canvas(48, 96)
    c.line(4, 90, 40, 6, L_STEEL, 2)                      # greatsword behind her, diagonal
    c.line(5, 90, 41, 6, L_STEEL_H)
    for i in range(22):                                   # ponytail
        t = i / 21
        c.ellipse(28 + 6 * t, 8 + 30 * t, 2.5 - t, 2, L_HAIR if i % 5 else L_STEEL)
    c.rect(19, 62, 23, 88, L_LEATHER)                     # legs
    c.rect(26, 62, 30, 88, L_LEATHER)
    c.rect(18, 86, 24, 95, L_STEEL_L)                     # iron-shod boots
    c.rect(25, 86, 31, 95, L_STEEL_L)
    c.poly([(15, 32), (33, 32), (32, 60), (16, 60)], TMP)   # breastplate
    c.poly([(16, 58), (32, 58), (35, 72), (13, 72)], TMP)   # tassets
    c.shade_region(TMP, L_STEEL_H, L_STEEL, L_STEEL_L, 22, 46, 12, 20)
    c.rect(16, 50, 32, 53, L_LEATHER)                     # belt
    c.ellipse(24, 42, 2, 2, L_GEM)
    for x in (18, 24, 30):
        c.rect(x, 60, x + 1, 72, L_STEEL_L)
    for side in (-1, 1):                                  # pauldrons
        c.ellipse(24 + side * 12, 34, 7, 5, TMP)
        c.shade_region(TMP, L_STEEL_H, L_STEEL, L_STEEL_L, 20 + side * 12, 32, 7, 5)
        c.set(24 + side * 12, 34, L_GEM)
    c.rect(10, 38, 14, 58, L_SKIN)                        # bare arms, bracers
    c.rect(34, 38, 38, 58, L_SKIN_L)
    c.rect(10, 50, 14, 55, L_LEATHER)
    c.rect(34, 50, 38, 55, L_LEATHER)
    c.rect(21, 25, 27, 32, L_SKIN_L)                      # neck
    face(c, 24, 18, 6, 8)
    c.shade_region(TMP, L_SKIN_H, L_SKIN, L_SKIN_L, 23, 18, 7, 9)
    ear(c, (18, 15), (18, 19), (13, 11), L_SKIN, L_SKIN_H)
    ear(c, (30, 15), (30, 19), (35, 11), L_SKIN_L, L_SKIN)
    c.ellipse(24, 11, 7, 5, L_HAIR)                       # hair pulled back
    c.rect(17, 11, 31, 13, L_HAIR)
    c.rect(18, 18, 22, 19, L_PAINT)                       # war paint under the eyes
    c.rect(26, 18, 30, 19, L_PAINT)
    c.set(20, 17, L_AMBER); c.set(21, 17, OUT_C)          # amber eyes
    c.set(26, 17, OUT_C); c.set(27, 17, L_AMBER)
    c.set(21, 13, L_PAINT); c.set(27, 14, L_PAINT); c.set(19, 21, L_PAINT); c.set(29, 21, L_PAINT)
    c.set(23, 22, OUT_C); c.set(25, 22, OUT_C)            # nostrils
    c.rect(22, 24, 27, 25, OUT_C)                         # stern mouth
    c.outline()
    return c


# ---------------------------------------------------------------- Schattenherz
def shadowheart_bust():
    c = Canvas(80, 96)
    # long braid down her left side, bound in gold rings
    for i in range(30):
        t = i / 29
        x = 26 - 12 * t + 3 * math.sin(t * 5)
        y = 34 + 58 * t
        c.ellipse(x, y, 3.5 - t, 3, S_HAIR if i % 6 else S_GOLD)
        if i % 6:
            c.set(x - 1, y - 1, S_HAIR_H)
    # ears, subtly pointed, peeking out of the hair
    ear(c, (27, 37), (27, 44), (17, 30), S_SKIN, S_SKIN_H)
    ear(c, (53, 37), (53, 44), (63, 30), S_SKIN_L, S_SKIN)
    # neck and face
    c.rect(34, 52, 46, 68, TMP)
    face(c, 40, 40, 13, 18, 0.5)
    c.shade_region(TMP, S_SKIN_H, S_SKIN, S_SKIN_L, 38, 40, 15, 22)
    c.rect(35, 57, 45, 60, S_SKIN_L)
    # black hair: a cap, locks framing the face, a straight fringe
    for y in range(12, 60):
        for x in range(20, 61):
            cap = ((x + 0.5 - 40) / 17) ** 2 + ((y + 0.5 - 30) / 17) ** 2 <= 1 and y < 34
            locks = (22 <= x < 29 or 52 <= x < 58) and 28 <= y < 56 - abs(x - 40) // 4
            if cap or locks:
                c.set(x, y, S_HAIR)
    for x in range(26, 55):                               # fringe, a few longer strands
        c.rect(x, 30, x + 1, 34 + (1 if x % 4 == 0 else 0), S_HAIR)
    for x in range(24, 56, 2):                            # sheen
        y = 17 + int(((x - 40) / 16) ** 2 * 8)
        c.set(x, y, S_HAIR_H)
        c.set(x + 1, y + 1, S_HAIR_H)
    # gold circlet across the hair, a dark stone in the middle
    for x in range(26, 55):
        y = 25 + int(((x - 40) / 14) ** 2 * 3)
        c.set(x, y, S_GOLD_H)
        c.set(x, y + 1, S_GOLD)
    c.ellipse(40, 26, 3, 3, S_GOLD)
    c.ellipse(40, 26, 2, 2, S_PURPLE_D)
    c.set(39, 25, S_PURPLE)
    # fine brows, calm eyes with dark lids, lips
    c.line(30, 37, 37, 36, S_HAIR)
    c.line(43, 36, 50, 37, S_HAIR)
    almond_eye(c, 31, 40, S_EYE, S_HAIR, white=S_SKIN_H, glint=S_SKIN_H)
    almond_eye(c, 43, 40, S_EYE, S_HAIR, white=S_SKIN_H, glint=S_SKIN_H)
    c.line(41, 42, 42, 48, S_SKIN_L)                      # nose
    c.rect(39, 49, 43, 50, S_SKIN_L)
    c.rect(37, 53, 44, 54, S_LIP)
    c.rect(38, 54, 43, 55, S_SKIN_L)
    # high violet collar with a gold trim
    c.poly([(31, 60), (49, 60), (52, 70), (28, 70)], S_PURPLE_D)
    c.line(29, 69, 51, 69, S_GOLD)
    # armour: violet cloth down the middle, silver plates, gold trim in a V, Shar's amulet
    c.poly([(26, 70), (54, 70), (60, 96), (20, 96)], TMP)
    c.shade_region(TMP, S_STEEL_H, S_STEEL, S_STEEL_L, 36, 80, 18, 14)
    c.poly([(34, 70), (46, 70), (44, 96), (36, 96)], S_PURPLE)
    c.line(26, 71, 40, 90, S_GOLD)
    c.line(54, 71, 40, 90, S_GOLD)
    c.ellipse(40, 80, 3, 3, S_GOLD_H)
    c.ellipse(40, 80, 2, 2, S_PURPLE_D)
    # swept, wing-like pauldrons: three layered blades per side, rising outwards
    for side in (-1, 1):
        for k, (dy, reach) in enumerate(((0, 21), (7, 23), (14, 21))):
            base = 40 + side * 12
            tip = 40 + side * (12 + reach)
            pts = [(base, 70 + dy), (tip, 60 + dy + k * 2), (tip - side * 3, 66 + dy + k * 2), (base, 78 + dy)]
            c.poly(pts, TMP)
            c.shade_region(TMP, S_STEEL_H, S_STEEL, S_STEEL_L, 40 + side * 20, 68 + dy, 20, 8)
            c.line(base, 70 + dy, tip, 60 + dy + k * 2, S_STEEL_H)
        for y in range(88, 96):                            # chain mail on the upper arm
            for x in range(40 + side * 30 - 6, 40 + side * 30 + 6):
                c.set(x, y, S_STEEL if (x + y) % 2 else S_STEEL_L)
    c.outline()
    return c


def shadowheart():
    """Full figure: the same design at 48x96."""
    c = Canvas(48, 96)
    for y in range(12, 66):                               # braid over her left shoulder
        c.rect(14 - (y - 12) // 18, y, 17 - (y - 12) // 18, y + 1, S_GOLD if y % 7 == 0 else S_HAIR)
    c.rect(19, 62, 23, 88, S_PURPLE_D)                    # legs
    c.rect(26, 62, 30, 88, S_PURPLE_D)
    c.rect(18, 86, 24, 95, S_STEEL_L)
    c.rect(25, 86, 31, 95, S_STEEL_L)
    c.poly([(16, 32), (32, 32), (34, 74), (14, 74)], TMP)   # armour and skirt of plates
    c.shade_region(TMP, S_STEEL_H, S_STEEL, S_STEEL_L, 22, 50, 12, 24)
    c.poly([(21, 32), (27, 32), (26, 74), (22, 74)], S_PURPLE)
    c.line(16, 33, 24, 46, S_GOLD)
    c.line(32, 33, 24, 46, S_GOLD)
    c.set(24, 40, S_GOLD_H)
    c.rect(16, 52, 32, 54, S_PURPLE_D)                    # belt
    for side in (-1, 1):                                  # swept pauldrons
        for k in range(3):
            c.poly([(24 + side * 7, 31 + k * 3), (24 + side * 17, 27 + k * 3), (24 + side * 15, 30 + k * 3),
                    (24 + side * 7, 35 + k * 3)], S_STEEL_H if k == 0 else S_STEEL)
    c.rect(10, 38, 14, 58, S_STEEL_L)                     # chain sleeves
    c.rect(34, 38, 38, 58, S_STEEL_L)
    for y in range(38, 58, 2):
        c.set(11, y, S_STEEL); c.set(13, y + 1, S_STEEL); c.set(35, y, S_STEEL); c.set(37, y + 1, S_STEEL)
    c.rect(10, 57, 14, 61, S_SKIN)                        # hands
    c.rect(34, 57, 38, 61, S_SKIN_L)
    c.rect(38, 36, 40, 62, S_STEEL_L)                     # mace
    c.ellipse(39, 35, 3, 3, S_STEEL_H)
    c.rect(21, 25, 27, 32, S_SKIN_L)                      # neck
    face(c, 24, 18, 6, 8, 0.5)
    c.shade_region(TMP, S_SKIN_H, S_SKIN, S_SKIN_L, 23, 18, 7, 9)
    c.ellipse(24, 12, 8, 6, S_HAIR)                       # hair, fringe, locks
    c.rect(17, 12, 32, 15, S_HAIR)
    c.rect(16, 12, 19, 25, S_HAIR)
    c.rect(29, 12, 32, 24, S_HAIR)
    c.rect(18, 10, 31, 11, S_GOLD_H)                      # circlet
    c.set(24, 10, S_PURPLE_D)
    c.set(20, 17, OUT_C); c.set(21, 17, S_EYE)            # eyes
    c.set(26, 17, S_EYE); c.set(27, 17, OUT_C)
    c.rect(22, 23, 26, 24, S_LIP)
    c.outline()
    return c


def arrow():
    """Target marker, drawn in the accent colour (index 15) of whatever figure palette is loaded."""
    c = Canvas(8, 8)
    c.poly([(0, 1), (8, 1), (4, 7)], 15)
    c.rect(0, 0, 8, 1, OUT_C)
    return c


if __name__ == "__main__":
    save([imp(0), imp(1), imp(2)], "fig_imp.png", HELL_PAL)
    save([hound()], "fig_hound.png", HELL_PAL)
    save([cambion()], "fig_cambion.png", HELL_PAL)
    save([zhalk(), zhalk(False)], "fig_zhalk.png", HELL_PAL)
    save([mindflayer_bust()], "fig_mindflayer_bust.png", MF_PAL)
    save([laezel()], "fig_laezel.png", LZ_PAL)
    save([laezel_bust()], "fig_laezel_bust.png", LZ_PAL)
    save([shadowheart()], "fig_shadowheart.png", SH_PAL)
    save([shadowheart_bust()], "fig_shadowheart_bust.png", SH_PAL)
    save([arrow()], "fig_arrow.png", HELL_PAL)
