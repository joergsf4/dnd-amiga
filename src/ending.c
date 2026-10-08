#include "ending.h"
#include "countdown.h"
#include "textbox.h"
#include "figures.h"
#include "party.h"
#include "inventory.h"
#include "ui_panel.h"
#include "text.h"
#include "sfx.h"
#include "equipment.h"
#include "game.h"

static void say(const char *l0, const char *l1, const char *l2)
{
    const char *lines[3] = { l0, l1, l2 };
    textbox_show(lines, 3, NULL, 0);   // NULL lines are skipped
}

// The whole screen flashes (backdrop colour) while the view shakes.
static void quake(u8 times)
{
    static const u16 flashes[] = { 0x000E, 0x0EEE, 0x0006, 0x0000 };   // red, white, dark red, black
    sfx_play(SFX_QUAKE);
    for (u8 i = 0; i < times; i++)
    {
        PAL_setColor(0, flashes[i % 4]);
        figures_shakeView();
    }
    PAL_setColor(0, 0);
}

void ending_run(void)
{
    countdown_stop();
    say("Die Nervenstränge zucken -", "der Transponder glüht auf!", NULL);
    quake(4);
    say("Der Nautiloid reißt sich", "los und stürzt durch ein", "Portal ...");
    quake(8);
    say("... und zerschellt an der", "Schwertküste.", "Ihr lebt.");

    // The end screen: the view goes dark, the party lines up with its loot.
    music_play(MUSIC_ENDING);
    VDP_clearPlane(BG_B, TRUE);
    VDP_clearPlane(BG_A, TRUE);
    text_draw("ENDE DES PROLOGS", 12, 2);
    text_draw("DIE GRUPPE HAT ÜBERLEBT:", 8, 5);
    u8 row = 8;
    for (u8 i = 0; i < PARTY_MAX; i++)
    {
        const Character *c = &party.members[i];
        if (!c->active) continue;
        char line[24];
        sprintf(line, "%-8s  KP %2d/%2d", c->name, c->hp, c->hpMax);
        text_draw(line, 13, row + 1);
        row += 3;
    }
    uiPanel_moveAvatars(9 * 8, 8 * 8, 3 * 8);
    char line[40];
    sprintf(line, "GOLD: %d   EDELSTEINE: %d", inventory.gold, inventory.gems);
    text_draw(line, 8, 21);
    if (equip_partyHas(EQ_EVERBURN)) text_draw("IMMERBRAND-KLINGE ERBEUTET!", 7, 23);
    text_draw("DANKE FÜRS SPIELEN!", 11, 25);
    text_draw("Q: NEUES SPIEL", 11, 26);

    u16 prevJoy = JOY_readJoypad(JOY_1);
    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        if (joy & ~prevJoy & BUTTON_START) SYS_hardReset();
        prevJoy = joy;
        SPR_update();
        SYS_doVBlankProcess();
    }
}
