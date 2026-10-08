#!/usr/bin/env python3
"""Generates res/gfx/font_de.png: SGDK's default 8x8 font (res/gfx/font_base.png, copied from
SGDK's res/image/font_default.png, MIT licensed) with German umlauts in place of seven ASCII
characters the game's texts never use. src/text.c maps UTF-8 umlauts in string literals onto
these slots, so source strings can simply be written in German:

    Ä -> '\\'   Ö -> '^'   Ü -> '`'   ä -> '{'   ö -> '|'   ü -> '}'   ß -> '~'

    python3 tools/make_font.py
"""
import os

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
BASE = os.path.join(ROOT, "res", "gfx", "font_base.png")
OUT = os.path.join(ROOT, "res", "gfx", "font_de.png")

DOTS = ".##..##."
ESZETT = [
    "........",
    "..####..",
    ".##..##.",
    ".##.##..",
    ".##..##.",
    ".##..##.",
    ".##.##..",
    "........",
]


def main():
    img = Image.open(BASE)
    ink = max(img.getdata())                     # the font's foreground colour index

    def glyph(ch):
        i = ord(ch) - 32
        gx, gy = (i % 16) * 8, (i // 16) * 8
        return ["".join("#" if img.getpixel((gx + x, gy + y)) else "." for x in range(8))
                for y in range(8)]

    def put(ch, rows):
        i = ord(ch) - 32
        gx, gy = (i % 16) * 8, (i // 16) * 8
        for y, row in enumerate(rows):
            for x, c in enumerate(row):
                img.putpixel((gx + x, gy + y), ink if c == "#" else 0)

    def upper_umlaut(ch):
        rows = glyph(ch)[1:7]                    # capitals use rows 1-6
        del rows[len(rows) // 2]                 # squeeze to 5 rows to make room for the dots
        return [DOTS, "........"] + rows + ["........"]

    def lower_umlaut(ch):
        rows = glyph(ch)
        return [DOTS] + rows[1:]                 # lower case starts at row 2, dots fit on row 0

    put("\\", upper_umlaut("A"))
    put("^", upper_umlaut("O"))
    put("`", upper_umlaut("U"))
    put("{", lower_umlaut("a"))
    put("|", lower_umlaut("o"))
    put("}", lower_umlaut("u"))
    put("~", ESZETT)
    img.save(OUT)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
