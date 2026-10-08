#include "ui_panel.h"
#include "inventory.h"
#include "text.h"
#include "portraits.h"
#include "countdown.h"
#include "figures.h"
#include "game.h"

static const char *const facingLetter[4] = { "N", "O", "S", "W" };

#define SLOT_ROWS 3                    // avatar height in tiles == rows per party slot
#define TEXT_COL  (UI_PANEL_COL + 3)   // right of the 3-tile-wide avatar

static void drawSlotText(u8 i)
{
    u16 row = i * SLOT_ROWS;
    const Character *c = &party.members[i];

    if (!c->active)
    {
        text_draw("---LEER---  ", UI_PANEL_COL, row);
        return;
    }

    for (u8 r = 0; r < SLOT_ROWS; r++)   // the slot may still show "---LEER---" from before
        text_draw("         ", TEXT_COL, row + r);

    char text[16];
    text_draw(c->name, TEXT_COL, row);
    sprintf(text, "KP %2d/%2d", c->hp, c->hpMax);
    text_draw(text, TEXT_COL, row + 1);
    if (c->mpMax > 0)   // class resource: spell slots (Magier), Heilende-Hände pool (paladin)
    {
        sprintf(text, "%s %2d/%2d", c->cls == CLASS_SHADOWHEART ? "HH" : "ZP", c->mp, c->mpMax);
        text_draw(text, TEXT_COL, row + 2);
    }
}

static Sprite *avatars[PARTY_MAX];

void uiPanel_initSprites(void)
{
    for (u8 i = 0; i < PARTY_MAX; i++)
    {
        if (!party.members[i].active || avatars[i]) continue;
        u16 row = i * SLOT_ROWS;
        const SpriteDefinition *def;
        u16 pal = PAL1;   // companions share PAL1's palette
        switch (party.members[i].cls)
        {
            case CLASS_WIR:    def = &avatar_wir_sprite; break;
            case CLASS_LAEZEL: def = &avatar_laezel_sprite; break;
            case CLASS_SHADOWHEART: def = &avatar_shadowheart_sprite; break;
            default:           // the hero: the chosen portrait's avatar. On the Amiga it's in the
                               // view palette like the companions' (tools/make_res.py): its own
                               // palette would take the figures' colours 16-31.
                def = portrait_avatar(party.members[i].cls, party.members[i].portrait);
                break;
        }
        avatars[i] = SPR_addSpriteEx(def, UI_PANEL_COL * 8, row * 8,
                                     TILE_ATTR_FULL(pal, FALSE, FALSE, FALSE, figures_avatarTile(i)),
                                     SPR_FLAG_AUTO_TILE_UPLOAD);
    }
}

void uiPanel_drawInventory(void)
{
    char text[24];

    text_draw("INVENTAR", UI_PANEL_COL, 13);
    sprintf(text, "GOLD:   %3d", inventory.gold);
    text_draw(text, UI_PANEL_COL, 14);
    sprintf(text, "EDELST.: %2d", inventory.gems);
    text_draw(text, UI_PANEL_COL, 15);
    sprintf(text, "TRÄNKE:  %2d", inventory.healingPotions);
    text_draw(text, UI_PANEL_COL, 16);
    sprintf(text, "RUCKSACK: %2d", inventory.bagCount);
    text_draw(text, UI_PANEL_COL, 17);
    u8 row = 18;                         // key items while carried, packed from row 18
    for (u8 i = 0; i < ITEM_COUNT && row < 18 + INVENTORY_ITEM_ROWS; i++)
        if (inventory_hasItem((ItemId) i))
            text_draw(inventory_itemName((ItemId) i), UI_PANEL_COL, row++);
    for (; row < 18 + INVENTORY_ITEM_ROWS; row++)
        text_draw("            ", UI_PANEL_COL, row);
    uiPanel_drawCountdown();
}

void uiPanel_redrawChrome(void)
{
    for (u8 i = 0; i < PARTY_MAX; i++)
        drawSlotText(i);

    uiPanel_drawInventory();

    text_draw("PFEILE/WASD:", UI_PANEL_COL, 22);
    text_draw("GEHEN/DREHEN", UI_PANEL_COL, 23);
    text_draw("Q: BENUTZEN", UI_PANEL_COL, 24);
    text_draw("E: GRUPPE", UI_PANEL_COL, 25);
    text_draw("M: KARTE", UI_PANEL_COL, 26);
}

void uiPanel_drawStatus(const Player *p)
{
    char text[UI_PANEL_W + 1];
    sprintf(text, "%s (%02d,%02d)", facingLetter[p->facing], p->x, p->y);
    text_draw(text, UI_PANEL_COL, 27);
}

void uiPanel_drawCountdown(void)
{
    char text[UI_PANEL_W + 1];
    if (countdown_active())
        sprintf(text, "ABSTURZ: %2d ", countdown_left());
    else
        strcpy(text, "            ");
    text_draw(text, UI_PANEL_COL, 21);
}

void uiPanel_moveAvatars(s16 x, s16 y, s16 dy)
{
    for (u8 i = 0; i < PARTY_MAX; i++)
    {
        if (!avatars[i]) continue;
        SPR_setPosition(avatars[i], x, y);
        y += dy;
    }
}
