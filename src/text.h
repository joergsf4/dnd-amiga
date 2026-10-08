#ifndef _TEXT_H_
#define _TEXT_H_

#include <genesis.h>

// All on-screen text goes through here instead of VDP_drawText directly: string literals are
// written in plain German (UTF-8 source), and text_draw maps the umlauts onto the slots of the
// custom font (res/gfx/font_de.png, tools/make_font.py). Other non-ASCII characters show as '?',
// so stick to ASCII punctuation ("...", "-").

// Loads the umlaut font and the text palette (PAL3, so text never depends on the view's
// colours). Call once at boot, before any text is drawn.
void text_init(void);

// Puts the text palette back into PAL3 after something borrowed it (the intro's logo).
void text_resetPalette(void);

void text_draw(const char *utf8, u16 x, u16 y);

// Number of on-screen characters (an umlaut is 2 bytes of UTF-8 but 1 character).
u16 text_len(const char *utf8);

#endif
