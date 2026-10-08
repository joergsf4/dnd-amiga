#include "skill_check.h"
#include "dice.h"
#include "textbox.h"
#include "text.h"
#include "abilities.h"
#include "sfx.h"
#include "game.h"

static const char *const attrLabel[3] = { "STÄ", "GES", "INT" };

static u8 attrValue(const Character *c, Attribute attr)
{
    switch (attr)
    {
        case ATTR_STR: return c->str;
        case ATTR_DEX: return c->dex;
        default:       return c->intl; // ATTR_INT
    }
}

static void waitFrames(u16 n)
{
    for (u16 i = 0; i < n; i++)
    {
        SPR_update();
        SYS_doVBlankProcess();
    }
}

bool skillCheck_run(Character *actor, Attribute attr, u8 threshold)
{
    char buf[48];

    sfx_play(SFX_DICE);
    // a short flourish: a few flickering fake rolls before the real one settles
    for (u8 i = 0; i < 16; i++)
    {
        sprintf(buf, "[%s-PROBE] WÜRFELT... %2d", attrLabel[attr], dice_roll(20));
        text_draw(buf, 1, TEXTBOX_ROW);
        waitFrames(4);
    }

    // Expertise: the Schurke adds it to every GES check (Fingerfertigkeit, Heimlichkeit).
    bool expertise = actor->cls == CLASS_ROGUE && attr == ATTR_DEX;
    u8 mod = attrValue(actor, attr) + (expertise ? EXPERTISE_BONUS : 0);
    u8 roll = dice_roll(20);
    u8 total = roll + mod;
    bool success = total >= threshold;

    sprintf(buf, "[%s-PROBE] %2d +%d = %2d     ", attrLabel[attr], roll, mod, total);   // <= 27 chars
    text_draw(buf, 1, TEXTBOX_ROW);
    text_draw(success ? "ERFOLG!" : "FEHLSCHLAG.", 1, TEXTBOX_ROW + 1);
    if (expertise) text_draw("(MIT EXPERTISE +2)", 1, TEXTBOX_ROW + 2);

    waitFrames(60); // let the result sit before the caller's own textbox_show() replaces it

    return success;
}
