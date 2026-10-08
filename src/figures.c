#include "figures.h"
#include "game.h"

#define VIEW_W 224
#define VIEW_H 160

#define AVATAR_TILES 9                                   // 24x24
#define FIGURE_BASE (TILE_SPRITE_INDEX + 4 * AVATAR_TILES)  // above the four avatar slots

static u16 nextTile;
static u8 live;

u16 figures_avatarTile(u8 slot)
{
    return TILE_SPRITE_INDEX + slot * AVATAR_TILES;
}

Sprite *figures_addPlain(const SpriteDefinition *def, s16 x, s16 y, u16 pal)
{
    if (!live) nextTile = FIGURE_BASE;
    u16 tile = nextTile;
    nextTile += def->maxNumTile;
    live++;
    return SPR_addSpriteEx(def, x, y, TILE_ATTR_FULL(pal, FALSE, FALSE, FALSE, tile),
                           SPR_FLAG_AUTO_TILE_UPLOAD);
}

void figures_release(Sprite *s)
{
    SPR_releaseSprite(s);
    if (live) live--;
}

Sprite *figures_add(const SpriteDefinition *def, s16 cx, s16 bottom)
{
    PAL_setPalette(PAL2, def->palette->data, DMA);
    return figures_addPlain(def, cx - def->w / 2, bottom - def->h, PAL2);
}

Sprite *figures_addBust(const SpriteDefinition *def)
{
    return figures_add(def, VIEW_W / 2, VIEW_H);
}

void figures_wait(u16 n)
{
    for (u16 i = 0; i < n; i++)
    {
        SPR_update();
        SYS_doVBlankProcess();
    }
}

void figures_blink(Sprite *s, u8 times)
{
    for (u8 i = 0; i < times; i++)
    {
        SPR_setVisibility(s, HIDDEN);
        figures_wait(4);
        SPR_setVisibility(s, VISIBLE);
        figures_wait(4);
    }
}

void figures_shakeView(void)
{
    static const s8 offsets[] = { 5, -5, 4, -4, 3, -3, 2, -1, 0 };
    for (u8 i = 0; i < sizeof(offsets); i++)
    {
        VDP_setHorizontalScroll(BG_B, offsets[i]);
        figures_wait(2);
    }
}
