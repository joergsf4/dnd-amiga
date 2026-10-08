#!/usr/bin/env python3
"""Generates the hero portraits: three per starting class, picked on the character creation
screen. Each portrait is an 80x96 bust in the style of the companions' busts
(tools/make_figures.py) plus a 24x24 panel avatar scaled down from it:

    res/gfx/portrait_<class>_<n>.png   bust, shown on the creation screen
    res/gfx/hero_<class>_<n>.png       panel avatar

<class> is fighter, rogue or mage, <n> 1-3. Bust and avatar share one palette of their own. It
goes into PAL3 once the hero is chosen: PAL3 is the text palette, but the font only uses colour 15,
so every palette here keeps 15 white and has 1-14 to itself (1 is the outline).

    Kämpfer: a bald veteran with a grey beard and an earring; a blonde warrior with a braid, chain mail and furs;
             a dwarf with a braided red beard and a helmet
    Schurke: a hooded rogue with a scarf over his face; a dark-skinned rogue with gold earrings
             and leather straps; an elf with silver hair and a green cloak
    Magier:  an old wizard with a white beard and a starred hat; a young sorceress with fiery
             hair and a circlet; a tiefling with red skin, horns and gold eyes
"""
import math
import os
import random
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_figures as F   # noqa: E402  (Canvas, face, ear, almond_eye, TMP, OUT_C)

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "gfx")
WHITE = 15
OUTLINE_RGB = (0x14, 0x10, 0x14)


class Pal:
    """A portrait palette: index 0 transparent, 1 outline, 2-14 assigned on first use, 15 white."""

    def __init__(self):
        self.cols = [(0xFF, 0x00, 0xFF), OUTLINE_RGB]

    def __call__(self, rgb):
        if rgb in self.cols:
            return self.cols.index(rgb)
        assert len(self.cols) < 15, "portrait palette full"
        self.cols.append(rgb)
        return len(self.cols) - 1

    def ramp(self, *rgbs):
        return tuple(self(c) for c in rgbs)

    def final(self):
        return self.cols + [(0, 0, 0)] * (15 - len(self.cols)) + [(0xFF, 0xFF, 0xFF)]


# ---------------------------------------------------------------- body parts
def head(c, skin, cx=40, cy=40, rx=14, ry=18, jaw=0.45, ears=None):
    """Neck, face and optionally ears ('human' / 'elf'). The face is lit from the front-left: the
    shadow lies as a band along the right edge and under the chin -- a diagonal split through the
    face read like a mask."""
    hi, mid, lo = skin
    if ears == "human":
        c.ellipse(cx - rx, cy + 3, 2.5, 4.5, mid)
        c.ellipse(cx + rx, cy + 3, 2.5, 4.5, lo)
    elif ears == "elf":
        F.ear(c, (cx - rx + 1, cy - 5), (cx - rx + 1, cy + 5), (cx - rx - 13, cy - 16), mid, hi)
        F.ear(c, (cx + rx - 1, cy - 5), (cx + rx - 1, cy + 5), (cx + rx + 13, cy - 16), lo, mid)
    c.rect(cx - 7, cy + 12, cx + 7, cy + 26, mid)       # neck, shaded on the right
    c.rect(cx + 3, cy + 12, cx + 7, cy + 26, lo)
    F.face(c, cx, cy, rx, ry, jaw)
    for y in range(c.h):
        for x in range(c.w):
            if c.px[y][x] != F.TMP:
                continue
            nx = (x + 0.5 - cx) / rx
            ny = (y + 0.5 - cy) / ry
            if nx > 0.62 or ny > 0.86:
                col = lo
            elif nx < -0.2 and -0.6 < ny < 0.45:
                col = hi
            else:
                col = mid
            if 0.55 < nx <= 0.62 and (x + y) % 2:          # one dithered pixel into the shadow
                col = lo
            c.px[y][x] = col
    c.rect(cx - 6, cy + 18, cx + 6, cy + 20, lo)         # shadow under the chin


def eyes(c, iris, lid, brow, cy=40, glint=WHITE, brow_angle=0, white=None):
    c.line(29, cy - 4 + brow_angle, 37, cy - 4 - brow_angle, brow)
    c.line(43, cy - 4 - brow_angle, 51, cy - 4 + brow_angle, brow)
    F.almond_eye(c, 31, cy, iris, lid, white=white, glint=glint)
    F.almond_eye(c, 43, cy, iris, lid, white=white, glint=glint)


def nose_mouth(c, shade, lip, cy=40, smirk=0):
    c.line(41, cy + 2, 42, cy + 8, shade)
    c.rect(39, cy + 9, 43, cy + 10, shade)
    c.rect(36, cy + 14, 45, cy + 15, lip)
    if smirk:
        c.set(45, cy + 13, lip)
    c.rect(37, cy + 15, 44, cy + 16, shade)


def shaded_poly(c, pts, ramp, cx, cy, rx, ry):
    c.poly(pts, F.TMP)
    c.shade_region(F.TMP, ramp[0], ramp[1], ramp[2], cx, cy, rx, ry)


def torso(c, ramp):
    """Shoulders and chest, the base every outfit is drawn onto."""
    shaded_poly(c, [(24, 68), (56, 68), (74, 80), (78, 96), (2, 96), (6, 80)], ramp, 34, 80, 30, 16)


def pauldrons(c, ramp, gem=None):
    for side in (-1, 1):
        cx = 40 + side * 24
        c.ellipse(cx, 80, 17, 12, F.TMP)
        c.ellipse(cx, 82, 17, 12, F.TMP)
        c.shade_region(F.TMP, ramp[0], ramp[1], ramp[2], cx - 6, 76, 16, 12)
        c.line(cx - 15, 91, cx + 15, 91, ramp[2])
        for i in range(10):                                # a rim along the edge
            a = math.pi * (0.15 + 0.7 * i / 9)
            c.set(cx - 15 * math.cos(a), 80 - 11 * math.sin(a), ramp[0])
        if gem is not None:
            c.ellipse(cx + side * 2, 80, 2, 2, gem)


def chain(c, ramp, x0=6, x1=74, y0=70):
    for y in range(y0, 96):
        for x in range(x0, x1):
            if c.get(x, y) not in (F.TR,):
                c.set(x, y, ramp[1] if (x + y) % 2 else ramp[2])
                if (x + 2 * y) % 5 == 0:
                    c.set(x, y, ramp[0])


def collar(c, col, trim=None, y=62):
    c.poly([(30, y), (50, y), (53, y + 10), (27, y + 10)], col)
    if trim is not None:
        c.line(28, y + 9, 52, y + 9, trim)


def hair_cap(c, ramp, hairline=24, cy=30, r=16, part=False):
    """Hair covering the top of the head down to a hairline that arches towards the temples."""
    for y in range(8, 44):
        for x in range(18, 62):
            if ((x + 0.5 - 40) / r) ** 2 + ((y + 0.5 - cy) / r) ** 2 <= 1 and y < hairline + abs(x - 40) * 0.5:
                c.set(x, y, F.TMP)
    c.shade_region(F.TMP, ramp[0], ramp[1], ramp[2], 34, 20, 16, 12)
    for x0 in range(25, 57, 4):                           # strands
        x1, y0 = 40 + (x0 - 40) * 0.35, hairline - 1 + abs(x0 - 40) * 0.5
        for i in range(10):
            t = i / 9
            x, y = x0 + (x1 - x0) * t, y0 + (cy - r + 3 - y0) * t
            if c.get(int(x), int(y)) == ramp[1]:
                c.set(x, y, ramp[2])
    if part:
        c.line(36, cy - r + 2, 33, hairline + 2, ramp[2])


def locks(c, ramp, x0, x1, y0, y1):
    """A fall of hair beside the face (x0..x1), shaded, with strands."""
    for y in range(y0, y1):
        taper = max(0, (y - (y1 - 8)))
        for x in range(x0 + (taper if x0 < 40 else 0), x1 - (taper if x0 > 40 else 0)):
            c.set(x, y, F.TMP)
    c.shade_region(F.TMP, ramp[0], ramp[1], ramp[2], (x0 + x1) / 2 - 3, y0 + 6, (x1 - x0), (y1 - y0))
    for x in range(x0 + 1, x1, 2):
        for y in range(y0 + (x % 3), y1, 5):
            if c.get(x, y) == ramp[1]:
                c.set(x, y, ramp[2])


def braid(c, ramp, band, x0, y0, x1, y1, width=3.5):
    for i in range(24):
        t = i / 23
        x = x0 + (x1 - x0) * t + 2 * math.sin(t * 6)
        y = y0 + (y1 - y0) * t
        c.ellipse(x, y, width - t, 3, ramp[1] if i % 6 else band)
        if i % 6:
            c.set(x - 1, y - 1, ramp[0])
            c.set(x + 1, y + 1, ramp[2])


def beard(c, ramp, full=True, braided_band=None):
    """A beard from the cheeks down (full) or stubble on the jaw."""
    if not full:
        for y in range(46, 60):
            for x in range(26, 55):
                if c.get(x, y) not in (F.TR,) and (x * 7 + y * 3) % 4 == 0 and y > 47 + abs(x - 40) // 5:
                    c.set(x, y, ramp[2])
        return
    c.poly([(25, 42), (55, 42), (56, 60), (48, 76), (40, 82), (32, 76), (24, 60)], F.TMP)
    c.shade_region(F.TMP, ramp[0], ramp[1], ramp[2], 36, 56, 16, 20)
    for x in range(27, 54, 3):                            # flowing strands
        for y in range(46, 80, 2):
            if c.get(x, y) == ramp[1] and (x + y) % 3 == 0:
                c.set(x, y, ramp[2])
    if braided_band is not None:
        for bx in (34, 46):
            braid(c, ramp, braided_band, bx, 74, bx + (bx - 40) // 3, 94, 3)
    c.rect(33, 51, 48, 55, ramp[1])                       # moustache over the mouth
    c.line(33, 51, 47, 51, ramp[0])
    c.rect(37, 55, 44, 56, F.OUT_C)                       # the mouth, just a line


def face_details(c, skin, iris, brow, lip, cy=40, frown=1):
    hi, mid, lo = skin
    c.line(29, cy - 4 + frown, 37, cy - 4 - frown, brow)  # brows
    c.line(29, cy - 5 + frown, 37, cy - 5 - frown, brow)
    c.line(43, cy - 4 - frown, 51, cy - 4 + frown, brow)
    c.line(43, cy - 5 - frown, 51, cy - 5 + frown, brow)
    c.rect(30, cy - 2, 38, cy - 1, lo)                    # shadow under the brow
    c.rect(42, cy - 2, 50, cy - 1, lo)
    F.almond_eye(c, 31, cy, iris, F.OUT_C, white=hi, glint=WHITE)
    F.almond_eye(c, 43, cy, iris, F.OUT_C, white=hi, glint=WHITE)
    c.line(40, cy + 2, 40, cy + 8, mid)                   # nose: ridge lit, side shaded
    c.line(41, cy + 3, 42, cy + 8, lo)
    c.rect(38, cy + 9, 43, cy + 10, lo)
    c.rect(36, cy + 14, 45, cy + 15, F.OUT_C)             # mouth line
    c.rect(37, cy + 15, 44, cy + 16, lip)                 # lower lip


def short_beard(c, ramp, cy=40):
    """A trimmed full beard: solid, following the jaw, lighter where the light falls."""
    for y in range(cy + 6, cy + 22):
        for x in range(22, 59):
            if c.get(x, y) in (F.TR,):
                continue
            k = (y - cy) / 18.0
            half = 14 - max(0, (y - cy - 8)) * 0.55
            if abs(x + 0.5 - 40) < half and (y > cy + 11 or abs(x + 0.5 - 40) > 9):
                c.set(x, y, ramp[1] if x < 44 else ramp[2])
    for x in range(33, 48):                                # moustache
        c.set(x, cy + 12, ramp[1])
        c.set(x, cy + 13, ramp[1] if x < 44 else ramp[2])
    c.rect(36, cy + 14, 45, cy + 15, F.OUT_C)             # the mouth in it
    for x in range(26, 40, 3):                             # a little texture on the lit side
        y = cy + 16 + (x % 2)
        if c.get(x, y) == ramp[1]:
            c.set(x, y, ramp[0])


# ---------------------------------------------------------------- the portraits
def fighter_1():
    """A bald veteran: a shaven head, a grey stubble beard drawn as a solid
    shape, dark skin, a gold earring, a scar across the cheek, a leather gorget over mail."""
    p, c = Pal(), F.Canvas(80, 96)
    skin = p.ramp((0xB0, 0x7C, 0x5C), (0x88, 0x58, 0x3C), (0x58, 0x38, 0x28))
    grey = p.ramp((0xC8, 0xC8, 0xC0), (0x90, 0x90, 0x8C), (0x5C, 0x5C, 0x5C))
    steel = p.ramp((0xE8, 0xEC, 0xF4), (0x98, 0xA0, 0xB4), (0x54, 0x5C, 0x70))
    leather = p.ramp((0x9C, 0x68, 0x40), (0x6C, 0x44, 0x28), (0x40, 0x28, 0x18))
    gold = p((0xE8, 0xB8, 0x48))
    iris = leather[2]
    torso(c, steel)
    chain(c, steel, y0=74)
    pauldrons(c, leather)
    c.poly([(26, 62), (54, 62), (58, 74), (22, 74)], leather[1])   # leather gorget
    c.line(24, 66, 56, 66, leather[0])
    head(c, skin, rx=15, ry=19, jaw=0.15, ears="human")
    for y in range(16, 30):                                # shaven head: a sheen on the scalp
        for x in range(30, 40):
            if ((x - 34) / 5) ** 2 + ((y - 22) / 4) ** 2 <= 1 and c.get(x, y) != F.TR:
                c.set(x, y, skin[0])
    face_details(c, skin, iris, grey[2], skin[2])
    short_beard(c, grey)
    c.line(31, 45, 36, 50, skin[0])                        # scar across the cheek
    c.ellipse(55, 47, 1.6, 2.2, gold)                      # earring
    c.set(55, 47, skin[2])
    c.outline()
    return c, p


def fighter_2():
    """A blonde warrior: thick braid over the shoulder, freckles, chain mail, fur on the shoulders."""
    rng = random.Random(21)
    p, c = Pal(), F.Canvas(80, 96)
    skin = p.ramp((0xF4, 0xD4, 0xC0), (0xDC, 0xAC, 0x94), (0xA4, 0x74, 0x60))
    hair = p.ramp((0xF8, 0xDC, 0x80), (0xC8, 0x9C, 0x44), (0x84, 0x60, 0x28))
    steel = p.ramp((0xE0, 0xE4, 0xEC), (0x98, 0xA0, 0xB0), (0x54, 0x5C, 0x6C))
    fur = p.ramp((0xB0, 0xA0, 0x84), (0x80, 0x70, 0x58), (0x50, 0x44, 0x34))
    iris = p((0x48, 0x84, 0x60))
    lip = skin[2]
    torso(c, steel)
    chain(c, steel, y0=74)
    for side in (-1, 1):                                  # fur over the shoulders
        for i in range(40):
            x = 40 + side * rng.randint(12, 38)
            y = rng.randint(66, 84)
            c.ellipse(x, y, rng.randint(3, 5), 3, fur[rng.randint(0, 2)])
    head(c, skin, ears="human", rx=13, jaw=0.5)
    hair_cap(c, hair, hairline=25)
    locks(c, hair, 23, 28, 28, 52)
    braid(c, hair, steel[1], 54, 30, 62, 92, 4.5)         # the braid over her left shoulder
    eyes(c, iris, hair[2], hair[2], white=skin[0])
    nose_mouth(c, skin[2], lip)
    for _ in range(14):                                   # freckles
        x, y = rng.randint(30, 50), rng.randint(43, 49)
        if abs(x - 40) > 3:
            c.set(x, y, skin[2])
    c.outline()
    return c, p


def fighter_3():
    """A dwarf: helmet with a gold rim and a nasal, a braided red beard, plate armour."""
    p, c = Pal(), F.Canvas(80, 96)
    skin = p.ramp((0xF0, 0xB8, 0x98), (0xD0, 0x88, 0x6C), (0x94, 0x58, 0x44))
    beardc = p.ramp((0xE8, 0x7C, 0x38), (0xB0, 0x4C, 0x20), (0x70, 0x2C, 0x14))
    steel = p.ramp((0xF0, 0xF0, 0xF8), (0xA0, 0xA8, 0xBC), (0x58, 0x60, 0x74))
    gold = p.ramp((0xF8, 0xD8, 0x70), (0xC0, 0x8C, 0x30), (0x7C, 0x54, 0x1C))
    iris = p((0x3C, 0x54, 0x84))
    torso(c, steel)
    pauldrons(c, steel, gem=gold[1])
    head(c, skin, cx=40, cy=42, rx=16, ry=17, jaw=0.2, ears="human")
    eyes(c, iris, F.OUT_C, beardc[2], cy=42, brow_angle=1, white=skin[0])
    c.rect(38, 44, 44, 52, skin[1])                       # broad nose
    c.rect(38, 51, 44, 53, skin[2])
    beard(c, beardc, full=True, braided_band=gold[1])
    c.rect(29, 37, 38, 39, beardc[1])                     # bushy brows
    c.rect(43, 37, 52, 39, beardc[1])
    for y in range(12, 36):                               # helmet dome
        for x in range(20, 61):
            if ((x + 0.5 - 40) / 21) ** 2 + ((y + 0.5 - 35) / 22) ** 2 <= 1:
                c.set(x, y, F.TMP)
    c.shade_region(F.TMP, steel[0], steel[1], steel[2], 34, 22, 18, 14)
    c.rect(19, 33, 62, 36, gold[1])                       # gold rim
    c.line(19, 33, 61, 33, gold[0])
    c.rect(38, 33, 43, 48, steel[1])                      # nasal
    c.line(38, 33, 38, 47, steel[0])
    c.line(40, 12, 40, 32, steel[0])                      # ridge
    c.outline()
    return c, p


def rogue_1():
    """A hooded rogue: dark hood and cloak, a scarf over the lower face, sharp green eyes."""
    p, c = Pal(), F.Canvas(80, 96)
    skin = p.ramp((0xE8, 0xBC, 0xA0), (0xC0, 0x90, 0x74), (0x7C, 0x58, 0x44))
    hood = p.ramp((0x5C, 0x68, 0x58), (0x38, 0x42, 0x38), (0x1C, 0x22, 0x1C))
    leather = p.ramp((0x8C, 0x5C, 0x38), (0x5C, 0x3C, 0x24), (0x34, 0x20, 0x14))
    iris = p((0x7C, 0xC8, 0x60))
    steel = p((0xC8, 0xCC, 0xD8))
    torso(c, hood)
    c.line(26, 70, 56, 96, leather[1], 3)                 # baldric
    c.ellipse(46, 84, 3, 3, steel)                        # buckle
    c.line(62, 60, 70, 78, leather[2], 3)                 # dagger hilt over the shoulder
    c.rect(58, 58, 68, 61, steel)
    head(c, skin, jaw=0.45)
    c.poly([(22, 50), (58, 50), (60, 70), (20, 70)], F.TMP)   # scarf over nose and mouth
    c.shade_region(F.TMP, hood[0], hood[1], hood[2], 34, 56, 18, 10)
    for x in range(24, 58, 5):
        c.line(x, 51, x + 3, 69, hood[2])
    for y in range(4, 60):                                # hood, deep over the forehead
        for x in range(12, 69):
            outer = ((x + 0.5 - 40) / 26) ** 2 + ((y + 0.5 - 36) / 32) ** 2 <= 1
            opening = ((x + 0.5 - 40) / 15) ** 2 + ((y + 0.5 - 44) / 18) ** 2 <= 1 and y > 32
            if outer and not opening:
                c.set(x, y, F.TMP)
    c.shade_region(F.TMP, hood[0], hood[1], hood[2], 30, 24, 24, 26)
    c.rect(26, 33, 55, 37, hood[2])                       # shadow under the hood's rim
    eyes(c, iris, F.OUT_C, hood[2], cy=41, brow_angle=1, white=skin[1])
    c.outline()
    return c, p


def rogue_2():
    """A dark-skinned rogue: short black hair shaved at the side, gold earrings, a smirk,
    leather armour with straps and buckles."""
    p, c = Pal(), F.Canvas(80, 96)
    skin = p.ramp((0xB0, 0x7C, 0x58), (0x84, 0x54, 0x38), (0x54, 0x34, 0x24))
    hair = p.ramp((0x48, 0x40, 0x48), (0x24, 0x1C, 0x24), (0x10, 0x0C, 0x10))
    leather = p.ramp((0xA8, 0x70, 0x44), (0x74, 0x48, 0x2C), (0x44, 0x28, 0x18))
    gold = p.ramp((0xF8, 0xD8, 0x70), (0xC0, 0x8C, 0x30), (0x7C, 0x54, 0x1C))
    iris = hair[0]
    lip = skin[2]
    torso(c, leather)
    for x0 in (20, 44):                                   # straps and buckles
        c.line(x0, 70, x0 + 16, 96, leather[2], 2)
    c.ellipse(28, 82, 2, 2, gold[1])
    c.ellipse(52, 82, 2, 2, gold[1])
    collar(c, leather[2], gold[1])
    head(c, skin, ears="human", rx=13, jaw=0.5)
    hair_cap(c, hair, hairline=23, cy=29, r=15)
    for y in range(24, 38):                               # shaved side
        for x in range(24, 28):
            if c.get(x, y) in hair:
                c.set(x, y, skin[2] if (x + y) % 2 else hair[2])
    locks(c, hair, 44, 55, 20, 34)                        # longer on top, falling to one side
    eyes(c, iris, F.OUT_C, hair[1], white=skin[0])
    nose_mouth(c, skin[2], lip, smirk=1)
    for ex in (26, 54):                                   # gold hoops
        c.ellipse(ex, 49, 2, 3, gold[1])
        c.set(ex, 49, skin[1])
    c.outline()
    return c, p


def rogue_3():
    """An elf: long silver hair, fine features, a green cloak with the hood down, leather."""
    p, c = Pal(), F.Canvas(80, 96)
    skin = p.ramp((0xF4, 0xDC, 0xC8), (0xD8, 0xB4, 0x9C), (0x9C, 0x78, 0x68))
    hair = p.ramp((0xF4, 0xF4, 0xFC), (0xB8, 0xBC, 0xCC), (0x78, 0x7C, 0x94))
    cloak = p.ramp((0x68, 0x98, 0x58), (0x40, 0x68, 0x38), (0x24, 0x40, 0x20))
    leather = p.ramp((0x8C, 0x5C, 0x38), (0x5C, 0x3C, 0x24), (0x34, 0x20, 0x14))
    iris = p((0x4C, 0x9C, 0xB8))
    torso(c, leather)
    for side in (-1, 1):                                  # cloak over the shoulders, hood behind
        c.poly([(40 + side * 10, 66), (40 + side * 40, 80), (40 + side * 40, 96), (40 + side * 20, 96),
                (40 + side * 14, 74)], cloak[1] if side < 0 else cloak[2])
    c.ellipse(40, 66, 20, 6, cloak[1])
    c.ellipse(40, 90, 3, 3, hair[1])                      # leaf clasp
    locks(c, hair, 21, 29, 26, 74)                        # long hair to the shoulders
    locks(c, hair, 51, 59, 26, 74)
    head(c, skin, ears="elf", rx=13, ry=18, jaw=0.55)
    hair_cap(c, hair, hairline=26, part=True)
    eyes(c, iris, hair[2], hair[2], white=skin[0])
    nose_mouth(c, skin[2], skin[2])
    c.outline()
    return c, p


def mage_1():
    """An old wizard: a blue pointed hat with gold stars, bushy white brows, a long white beard."""
    p, c = Pal(), F.Canvas(80, 96)
    skin = p.ramp((0xF0, 0xC8, 0xAC), (0xC8, 0x98, 0x80), (0x8C, 0x64, 0x54))
    white = p.ramp((0xF8, 0xF8, 0xF8), (0xC8, 0xC8, 0xD0), (0x88, 0x88, 0x98))
    robe = p.ramp((0x50, 0x6C, 0xC8), (0x30, 0x44, 0x90), (0x1C, 0x24, 0x54))
    gold = p.ramp((0xF8, 0xD8, 0x70), (0xC0, 0x8C, 0x30), (0x7C, 0x54, 0x1C))
    iris = p((0x40, 0x60, 0xA0))
    torso(c, robe)
    c.line(40, 70, 40, 96, gold[1])                       # trim down the robe
    head(c, skin, ears="human", jaw=0.3)
    for y in (35, 45, 47):                                # wrinkles
        c.line(31, y, 36, y, skin[1])
        c.line(45, y, 50, y, skin[1])
    eyes(c, iris, F.OUT_C, white[1], white=skin[0])
    c.rect(28, 35, 38, 38, white[0])                      # bushy brows
    c.rect(43, 35, 53, 38, white[1])
    c.rect(38, 42, 44, 50, skin[1])                       # big nose
    c.rect(38, 49, 44, 51, skin[2])
    beard(c, white, full=True)
    # the hat: a wide brim and a tall cone bending back
    c.ellipse(40, 27, 30, 6, F.TMP)
    c.poly([(22, 26), (58, 26), (50, 8), (44, 0), (36, 0)], F.TMP)
    c.shade_region(F.TMP, robe[0], robe[1], robe[2], 34, 16, 20, 16)
    c.line(12, 29, 68, 29, robe[2])
    for sx, sy in ((32, 18), (46, 12), (41, 22), (38, 7)):
        c.set(sx, sy, gold[0])
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c.set(sx + dx, sy + dy, gold[1])
    c.outline()
    return c, p


def mage_2():
    """A young sorceress: long fiery red hair, a circlet with a green stone, a violet robe with a
    high collar and gold trim, a spark of magic in the air."""
    rng = random.Random(23)
    p, c = Pal(), F.Canvas(80, 96)
    skin = p.ramp((0xF8, 0xDC, 0xC8), (0xE0, 0xB0, 0x98), (0xA4, 0x78, 0x68))
    hair = p.ramp((0xF0, 0x80, 0x40), (0xC0, 0x44, 0x20), (0x78, 0x24, 0x14))
    robe = p.ramp((0x98, 0x60, 0xC8), (0x68, 0x3C, 0x94), (0x3C, 0x20, 0x5C))
    gold = p.ramp((0xF8, 0xD8, 0x70), (0xC0, 0x8C, 0x30), (0x7C, 0x54, 0x1C))
    gem = p((0x40, 0xC8, 0x80))
    lip = hair[1]
    locks(c, hair, 18, 30, 26, 88)                        # long hair falling down both sides
    locks(c, hair, 50, 62, 26, 88)
    torso(c, robe)
    c.line(24, 70, 40, 88, gold[1])
    c.line(56, 70, 40, 88, gold[1])
    collar(c, robe[2], gold[0], y=60)
    head(c, skin, rx=13, jaw=0.55)
    hair_cap(c, hair, hairline=25, part=True)
    for x in range(25, 56):                               # circlet
        y = 26 + int(((x - 40) / 14) ** 2 * 3)
        c.set(x, y, gold[0])
        c.set(x, y + 1, gold[1])
    c.ellipse(40, 27, 2, 2, gem)
    eyes(c, gem, hair[2], hair[2], white=skin[0])
    nose_mouth(c, skin[2], lip)
    for _ in range(8):                                    # freckles
        x = rng.randint(31, 49)
        if abs(x - 40) > 3:
            c.set(x, rng.randint(44, 48), skin[2])
    for sx, sy in ((10, 30), (68, 44), (64, 20)):         # sparks of magic
        c.set(sx, sy, WHITE)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c.set(sx + dx, sy + dy, gem)
    c.outline()
    return c, p


def mage_3():
    """A tiefling: red skin, horns curving back, black hair, glowing gold eyes, a dark teal robe."""
    p, c = Pal(), F.Canvas(80, 96)
    skin = p.ramp((0xE8, 0x70, 0x5C), (0xB4, 0x44, 0x3C), (0x74, 0x24, 0x24))
    hair = p.ramp((0x40, 0x38, 0x48), (0x20, 0x18, 0x24), (0x0C, 0x08, 0x10))
    horn = p.ramp((0x9C, 0x88, 0x84), (0x60, 0x50, 0x54)) + (hair[2],)
    robe = p.ramp((0x3C, 0x78, 0x80), (0x24, 0x50, 0x58), (0x14, 0x2C, 0x34))
    gold = p.ramp((0xF8, 0xD8, 0x70), (0xC0, 0x8C, 0x30))
    torso(c, robe)
    c.line(28, 70, 28, 96, gold[1])
    c.line(52, 70, 52, 96, gold[1])
    collar(c, robe[2], gold[1])
    locks(c, hair, 22, 28, 26, 58)
    locks(c, hair, 52, 58, 26, 58)
    head(c, skin, ears="elf", rx=13, jaw=0.55)
    hair_cap(c, hair, hairline=24)
    for side in (-1, 1):                                  # horns: up from the temples, then back
        x0, y0, cx_, cy_, x1, y1 = 40 + side * 10, 22, 40 + side * 16, -2, 40 + side * 28, 10
        for i in range(24):
            t = i / 23
            x = (1 - t) ** 2 * x0 + 2 * (1 - t) * t * cx_ + t * t * x1
            y = (1 - t) ** 2 * y0 + 2 * (1 - t) * t * cy_ + t * t * y1
            r = 4.2 - 3.0 * t
            c.ellipse(x, y, r, r, horn[1])
            c.set(x - side * r * 0.5, y - r * 0.6, horn[0])
            if i % 4 == 0:
                c.set(x + side * r * 0.4, y + r * 0.4, horn[2])   # ridges
    eyes(c, gold[0], F.OUT_C, hair[1], brow_angle=1, glint=WHITE)
    nose_mouth(c, skin[2], skin[2], smirk=1)
    c.outline()
    return c, p


PORTRAITS = {
    "fighter": (fighter_1, fighter_2, fighter_3),
    "rogue": (rogue_1, rogue_2, rogue_3),
    "mage": (mage_1, mage_2, mage_3),
}


def save_indexed(canvas, palette, path):
    img = Image.new("P", (canvas.w, canvas.h), 0)
    img.putpalette([v for c in palette for v in c] + [0] * (768 - 3 * len(palette)))
    img.putdata([c for row in canvas.px for c in row])
    img.save(path)


def avatar(canvas, palette, path, crop=(10, 6, 70, 66)):
    """The bust scaled down to 24x24 and mapped back onto its own palette."""
    src = F.to_rgba(canvas, palette).crop(crop).resize((24, 24), Image.BOX)
    out = F.Canvas(24, 24)
    for y in range(24):
        for x in range(24):
            r, g, b, a = src.getpixel((x, y))
            if a >= 140:
                out.px[y][x] = min(range(1, 16), key=lambda i: sum((u - v) ** 2 for u, v in zip((r, g, b), palette[i])))
    save_indexed(out, palette, path)


def main():
    sheet = Image.new("RGB", (3 * 88 + 3 * 32, 3 * 100), (40, 30, 40))
    for row, (cls, funcs) in enumerate(PORTRAITS.items()):
        for n, fn in enumerate(funcs, 1):
            canvas, pal = fn()
            palette = pal.final()
            bust = os.path.join(OUT, f"portrait_{cls}_{n}.png")
            av = os.path.join(OUT, f"hero_{cls}_{n}.png")
            save_indexed(canvas, palette, bust)
            avatar(canvas, palette, av)
            sheet.paste(Image.open(bust).convert("RGB"), ((n - 1) * 88, row * 100))
            sheet.paste(Image.open(av).convert("RGB"), (3 * 88 + (n - 1) * 32, row * 100))
            print("wrote", os.path.normpath(bust), "+ avatar,", len(pal.cols) - 1, "colours")
    preview = os.path.join(os.path.dirname(OUT), "..", "out", "portraits.png")
    os.makedirs(os.path.dirname(preview), exist_ok=True)
    sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).save(preview)
    print("preview", os.path.normpath(preview))


if __name__ == "__main__":
    main()
