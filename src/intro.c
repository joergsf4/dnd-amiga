#include "intro.h"
#include "text.h"
#include "sfx.h"
#include "game.h"

// The logo (tools/make_logo.py, 33 x 13 tiles) lies on BG_B, a mask of solid dark blue tiles on
// BG_A covers it, and a white shine column runs across, uncovering it -- as in Wanderburg.
#define INTRO_COL           3               // tile position of the logo, centred
#define INTRO_ROW           7
#define LOGO_TILES_W        33
#define LOGO_TILES_H        13
#define INTRO_STEP_FRAMES   2               // frames per uncovered column
#define INTRO_HOLD_FRAMES   110             // the finished logo stays this long
#define INTRO_SKIP_AFTER    12              // a button skips the intro after this many frames

static u16 shineTile;

static void solidTile(u16 tileIndex, u8 colour)
{
    u32 tile[8];
    for (u16 i = 0; i < 8; i++)
        tile[i] = colour * 0x11111111UL;
    VDP_loadTileData(tile, tileIndex, 1, CPU);
}

static void shineAt(u16 column)
{
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL3, TRUE, FALSE, FALSE, shineTile),
                        INTRO_COL + column, INTRO_ROW, 1, LOGO_TILES_H);
}

void intro_run(void)
{
    // palette 3 holds the logo colours; colour 6 is the backdrop's dark blue (the mask tiles use
    // it, so they are invisible)
    PAL_setPalette(PAL3, rcd_logo.palette->data, DMA);
    PAL_setColor(0, rcd_logo.palette->data[6]);
    VDP_setBackgroundColor(0);

    VDP_drawImageEx(BG_B, &rcd_logo, TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                    INTRO_COL, INTRO_ROW, FALSE, TRUE);
    u16 maskTile = TILE_USER_INDEX + rcd_logo.tileset->numTile;
    shineTile = maskTile + 1;
    solidTile(maskTile, 6);
    solidTile(shineTile, 5);
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL3, TRUE, FALSE, FALSE, maskTile),
                        INTRO_COL, INTRO_ROW, LOGO_TILES_W, LOGO_TILES_H);
    shineAt(0);
    sfx_play(SFX_INTRO);

    u16 frame = 0, revealed = 0;
    u16 prevJoy = JOY_readJoypad(JOY_1);
    while (TRUE)
    {
        SYS_doVBlankProcess();
        u16 joy = JOY_readJoypad(JOY_1);
        u16 pressed = joy & ~prevJoy;
        prevJoy = joy;
        frame++;

        if (revealed < LOGO_TILES_W && (frame % INTRO_STEP_FRAMES) == 0)
        {
            // the shine column moves on: what it covered is the logo now
            VDP_clearTileMapRect(BG_A, INTRO_COL + revealed, INTRO_ROW, 1, LOGO_TILES_H);
            revealed++;
            if (revealed < LOGO_TILES_W)
                shineAt(revealed);
        }
        if (frame > INTRO_SKIP_AFTER && (pressed & (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C)))
            break;
        if (frame > LOGO_TILES_W * INTRO_STEP_FRAMES + INTRO_HOLD_FRAMES)
            break;
    }

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    PAL_setColor(0, 0);
    text_resetPalette();
}
