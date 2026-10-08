/*
 * The subset of SGDK's API the game uses, implemented for the Amiga (src/amiga/), so the game
 * sources copied from the Mega Drive version compile unchanged where they only talk to SGDK.
 *
 * How the Mega Drive's VDP maps onto the Amiga (see src/amiga/vdp.c):
 *   - BG_A and BG_B are 40x28 tile maps, composited on the CPU into a 320x224, 5-bitplane
 *     screen (double buffered) wherever a cell changed. The view area of BG_B (28x20 tiles) is
 *     the first-person view instead, when one is active (dungeon_view.c).
 *   - Sprites are blitter objects drawn on top.
 *   - Colours: 32 registers in two banks. PAL0 -> colours 0-15 (view, images, text), PAL2 and
 *     PAL3 -> colours 16-31 (figures, busts). PAL1 has no bank of its own: the companions'
 *     avatars are converted to the view palette at build time (tools/make_res.py), so loading
 *     PAL1 does nothing. Text always uses colour 15.
 *   - Colours stay in the Mega Drive's 9-bit format (0x0BGR, 3 bits each) everywhere in the
 *     game and are converted when they reach a colour register.
 *   - SYS_doVBlankProcess paces the game at 60 calls per second on the 50 Hz display, so every
 *     frame count in the game keeps its Mega Drive timing.
 */
#ifndef _GENESIS_H_
#define _GENESIS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned long u32;
typedef signed long s32;
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef u8 bool;

#define TRUE  1
#define FALSE 0

// ---------------------------------------------------------------- resources (tools/make_res.py)

typedef struct
{
    u16 length;
    const u16 *data;          // Mega Drive colour format
} Palette;

// Tiles in Amiga form: 40 bytes each, bitplanes 0-3 (8 row bytes each), then the opaque mask
// (pixel index != 0).
#define TILE_BYTES 40

typedef struct
{
    u16 numTile;
    const u8 *tiles;
} TileSet;

typedef struct
{
    u16 w, h;
    const u16 *tilemap;       // tile index per cell, relative to the tile set
} TileMap;

typedef struct
{
    const Palette *palette;
    const TileSet *tileset;
    const TileMap *tilemap;
} Image;

// One sprite frame as a blitter object: wWords words per row, bitplanes 0-3 then the mask, each
// h rows. In chip RAM.
typedef struct
{
    u16 wWords, h;
    const u8 *data;
} BobFrame;

typedef struct
{
    u16 numFrame;
    const BobFrame *frames;
    u16 timer;                // frames per animation step, 0 = no automatic animation
} Animation;

typedef struct
{
    s16 w, h;
    const Palette *palette;
    u16 numAnimation;
    const Animation *animations;
    u16 maxNumTile;           // kept for the game's VRAM bookkeeping (figures.c); unused here
} SpriteDefinition;

// ---------------------------------------------------------------- VDP

#define BG_A 0
#define BG_B 1

#define PAL0 0
#define PAL1 1
#define PAL2 2
#define PAL3 3

#define CPU       0
#define DMA       1
#define DMA_QUEUE 2

#define TILE_ATTR_FULL(pal, prio, flipV, flipH, index) \
    ((((u16) (prio)) << 15) | (((u16) (pal)) << 13) | (((u16) (flipV)) << 12) | (((u16) (flipH)) << 11) | ((u16) (index)))
#define TILE_INDEX_MASK 0x07FF

#define TILE_MAX          2048
#define TILE_USER_INDEX   16
#define TILE_FONT_INDEX   (TILE_MAX - 96)
#define TILE_SPRITE_INDEX (TILE_FONT_INDEX - 320)

void VDP_setScreenWidth320(void);
void VDP_setScreenHeight224(void);
void VDP_loadFont(const TileSet *font, u16 mode);
void VDP_setTextPalette(u16 pal);
void VDP_drawText(const char *str, u16 x, u16 y);
void VDP_clearPlane(u16 plane, bool wait);
void VDP_clearTileMapRect(u16 plane, u16 x, u16 y, u16 w, u16 h);
void VDP_fillTileMapRect(u16 plane, u16 attr, u16 x, u16 y, u16 w, u16 h);
void VDP_fillTileMapRectInc(u16 plane, u16 attr, u16 x, u16 y, u16 w, u16 h);
bool VDP_drawImageEx(u16 plane, const Image *image, u16 attr, u16 x, u16 y, bool loadpal, u16 mode);
void VDP_loadTileData(const u32 *data, u16 index, u16 num, u16 mode);
void VDP_setBackgroundColor(u8 index);
void VDP_setHorizontalScroll(u16 plane, s16 value);

void PAL_setPalette(u16 numPal, const u16 *pal, u16 mode);
void PAL_setColor(u16 index, u16 value);
void PAL_setColors(u16 index, const u16 *pal, u16 count, u16 mode);
void PAL_getColors(u16 index, u16 *dest, u16 count);

// ---------------------------------------------------------------- sprites (src/amiga/spr.c)

typedef struct Sprite Sprite;

#define HIDDEN  0
#define VISIBLE 1
#define SPR_FLAG_AUTO_TILE_UPLOAD 0x0001

void SPR_initEx(u16 vramSize);
Sprite *SPR_addSpriteEx(const SpriteDefinition *def, s16 x, s16 y, u16 attribut, u16 flag);
void SPR_releaseSprite(Sprite *sprite);
void SPR_setPosition(Sprite *sprite, s16 x, s16 y);
void SPR_setVisibility(Sprite *sprite, u16 value);
void SPR_setFrame(Sprite *sprite, s16 frame);
void SPR_update(void);

// ---------------------------------------------------------------- input (src/amiga/joy.c)

#define JOY_1 0

#define BUTTON_UP    0x0001
#define BUTTON_DOWN  0x0002
#define BUTTON_LEFT  0x0004
#define BUTTON_RIGHT 0x0008
#define BUTTON_B     0x0010
#define BUTTON_C     0x0020
#define BUTTON_A     0x0040
#define BUTTON_START 0x0080

void JOY_update(void);
u16 JOY_readJoypad(u16 joy);

// ---------------------------------------------------------------- system (src/amiga/hw.c)

void SYS_doVBlankProcess(void);
void SYS_setVBlankCallback(void (*callback)(void));
void SYS_hardReset(void) __attribute__((noreturn));

// SGDK's random() is a 16-bit generator; libnix has its own random(), so it gets renamed here.
#define random sgdk_random
u16 sgdk_random(void);

// Debug output on the serial port (FS-UAE writes it to a file, see tools/emutest.sh).
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// The game's entry point (src/main.c); the Amiga's main() in src/amiga/hw.c calls it.
int game_main(bool hardReset);

#endif
