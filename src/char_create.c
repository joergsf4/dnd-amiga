#include "char_create.h"
#include "portraits.h"
#include "figures.h"
#include "text.h"
#include "game.h"

static const CharClass classOrder[3] = { CLASS_FIGHTER, CLASS_ROGUE, CLASS_MAGE };

// Two lines per class, at most 21 characters each (they sit right of the portrait): the class
// features (src/abilities.h).
static const char *const blurbs[3][2] = {
    { "ERHOLEN, SPALTEN,",    "NIEDERWERFEN." },
    { "HINTERHÄLTIG, FLINK,", "VERSTECKEN, EXPERTISE" },
    { "FEUERPFEIL, SCHLAF,",  "MAGISCHES GESCHOSS." },
};

#define PORTRAIT_X 40      // px: the bust, left half of the screen
#define PORTRAIT_Y 48
#define MENU_COL   19      // tiles: everything else, right of it

static void drawMenu(u8 cursor, u8 portrait)
{
    for (u8 i = 0; i < 3; i++)
    {
        char line[20];
        sprintf(line, "%s %s", (i == cursor) ? ">" : " ", class_name(classOrder[i]));
        text_draw(line, MENU_COL, 7 + i * 2);
    }
    text_draw("                     ", MENU_COL, 14);
    text_draw("                     ", MENU_COL, 15);
    text_draw(blurbs[cursor][0], MENU_COL, 14);
    text_draw(blurbs[cursor][1], MENU_COL, 15);

    char line[20];
    sprintf(line, "< BILD %d/%d >", portrait + 1, PORTRAITS_PER_CLASS);
    text_draw(line, MENU_COL, 18);
}

static Sprite *showPortrait(Sprite *old, u8 cursor, u8 portrait)
{
    if (old) figures_release(old);
    const SpriteDefinition *def = portrait_bust(classOrder[cursor], portrait);
    PAL_setPalette(PAL3, def->palette->data, DMA);   // colour 15 stays white: the text is safe
    return figures_addPlain(def, PORTRAIT_X, PORTRAIT_Y, PAL3);
}

CharClass charCreate_run(u8 *portrait)
{
    text_draw("ERSCHAFFE DEINEN HELDEN", 8, 2);
    text_draw("PFEILE: KLASSE/BILD   Q: OK", 5, 25);

    u8 cursor = 0, pick = 0;
    Sprite *bust = showPortrait(NULL, cursor, pick);
    drawMenu(cursor, pick);

    u16 prevJoy = JOY_readJoypad(JOY_1);   // START from the title screen may still be held
    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        u16 pressed = joy & ~prevJoy;

        if (pressed & BUTTON_UP) cursor = (cursor + 2) % 3;
        if (pressed & BUTTON_DOWN) cursor = (cursor + 1) % 3;
        if (pressed & BUTTON_LEFT) pick = (pick + PORTRAITS_PER_CLASS - 1) % PORTRAITS_PER_CLASS;
        if (pressed & BUTTON_RIGHT) pick = (pick + 1) % PORTRAITS_PER_CLASS;
        if (pressed & (BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT))
        {
            bust = showPortrait(bust, cursor, pick);
            drawMenu(cursor, pick);
        }

        if (pressed & (BUTTON_START | BUTTON_A)) break;

        prevJoy = joy;
        SPR_update();
        SYS_doVBlankProcess();
    }

    figures_release(bust);
    VDP_clearPlane(BG_A, TRUE);
    *portrait = pick;
    return classOrder[cursor];
}
