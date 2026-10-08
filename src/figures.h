#ifndef _FIGURES_H_
#define _FIGURES_H_

#include <genesis.h>

// Figures: characters and monsters shown in front of the first-person view during dialogue
// scenes and combat, as hardware sprites on PAL2 (tools/make_figures.py). Unlike props they're
// never occluded or scaled -- they only ever stand right in front of the player. Every figure has
// its own palette, loaded into PAL2 when it's added, so only one kind of figure can be on screen
// at a time (several of the same kind are fine: the imps in a fight).

// Adds a figure centred at screen x `cx` with its feet at screen y `bottom` (view coordinates).
Sprite *figures_add(const SpriteDefinition *def, s16 cx, s16 bottom);

// A companion's close-up while they speak: the bust sits on the bottom edge of the view.
Sprite *figures_addBust(const SpriteDefinition *def);

// A sprite at (x, y) on palette `pal` whose palette the caller loads itself (the combat marker,
// the portrait on the creation screen), in the figures' VRAM.
Sprite *figures_addPlain(const SpriteDefinition *def, s16 x, s16 y, u16 pal);

// Releases a figure. Once none is left, the figures' VRAM starts over from the bottom.
void figures_release(Sprite *s);

// Sprite VRAM is laid out by hand (SGDK's allocator fragments: after a few scenes a 120-tile bust
// found no room): the party avatars' fixed slots first, then the figures stacked on top of each
// other. The VRAM tile of avatar slot i:
u16 figures_avatarTile(u8 slot);

// Blinks a figure `times` times (a hit), blocking.
void figures_blink(Sprite *s, u8 times);

// Shakes the view horizontally for a moment (the party takes a hit, something lands), blocking.
void figures_shakeView(void);

// Waits n frames, keeping sprites (animations) updated.
void figures_wait(u16 n);

#endif
