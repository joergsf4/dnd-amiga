#include "text.h"
#include "game.h"

static const u16 textPalette[16] = {
    0x0000, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE,
    0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE,
};

void text_init(void)
{
    VDP_loadFont(&font_de, DMA);
    text_resetPalette();
    VDP_setTextPalette(PAL3);
}

void text_resetPalette(void)
{
    PAL_setPalette(PAL3, textPalette, DMA);
}

// UTF-8 is 0xC3 followed by one of these for the German letters; mapped to the font slots
// tools/make_font.py puts the umlauts in.
static char umlaut(u8 second)
{
    switch (second)
    {
        case 0x84: return '\\';  // Ä
        case 0x96: return '^';   // Ö
        case 0x9C: return '`';   // Ü
        case 0xA4: return '{';   // ä
        case 0xB6: return '|';   // ö
        case 0xBC: return '}';   // ü
        case 0x9F: return '~';   // ß
        default:   return '?';
    }
}

void text_draw(const char *utf8, u16 x, u16 y)
{
    char buf[41];
    u16 n = 0;
    const u8 *s = (const u8 *) utf8;
    while (*s && n < 40)
    {
        if (*s < 0x80) buf[n++] = *s++;
        else if (*s == 0xC3 && s[1]) { buf[n++] = umlaut(s[1]); s += 2; }
        else
        {
            buf[n++] = '?';
            s++;
            while ((*s & 0xC0) == 0x80) s++;    // skip the rest of an unsupported sequence
        }
    }
    buf[n] = 0;
    VDP_drawText(buf, x, y);
}

u16 text_len(const char *utf8)
{
    u16 n = 0;
    for (const u8 *s = (const u8 *) utf8; *s; s++)
        if ((*s & 0xC0) != 0x80) n++;
    return n;
}
