#include "room6.h"
#include "textbox.h"
#include "figures.h"
#include "countdown.h"
#include "combat.h"
#include "encounter.h"
#include "ending.h"
#include "dungeon_view.h"
#include "dungeon_objects.h"
#include "sfx.h"
#include "game.h"
#include "party.h"

// Every text line must fit the textbox: at most 27 characters on screen (an umlaut counts as one),
// menu options at most 24.

#define COUNTDOWN_ROUNDS 10
#define CAMBIONS_ROUND   5    // "Nach Runde 5: zwei weitere Cambions betreten die Brücke"

static void say(const char *l0, const char *l1, const char *l2)
{
    const char *lines[3] = { l0, l1, l2 };
    textbox_show(lines, 3, NULL, 0);   // NULL lines are skipped
}

// After every round of the countdown: halfway through, the cambions storm in from behind.
static void onRound(u8 left)
{
    if (left != COUNTDOWN_ROUNDS - CAMBIONS_ROUND) return;
    u8 count;
    RoomObject *objects = map_roomObjects(&count);
    for (u8 i = 0; i < count; i++)
        if (objects[i].kind == OBJ_ENEMY_GROUP && objects[i].param0 == ENC_CAMBIONS)
            objects[i].flags &= ~OBJFLAG_HIDDEN;
    figures_shakeView();
    say("Hinter euch stürmen zwei", "Cambions auf die Brücke!", NULL);
}

// Arrival (first visit): the mind flayer's command, Zhalk's threat, the countdown starts.
static void onEnter(Player *p)
{
    static bool done;
    (void) p;
    if (done) return;
    done = TRUE;

    say("Die Brücke! Der Boden", "bebt, draußen gähnt der", "Höllenschlund.");
    Sprite *s = figures_addBust(&fig_mindflayer_bust_sprite);
    say("Eine Stimme in deinem Kopf:", "\"Sklaven! Lauft zum", "Transponder!\"");
    say("\"Verbindet die Nerven!", "Beeilt euch!\"", NULL);
    figures_release(s);

    s = figures_add(&fig_zhalk_sprite, 112, 150);
    sfx_play(SFX_QUAKE);
    figures_shakeView();
    say("ZHALK: \"Niemand verlässt", "diese Ebene lebend!\"", NULL);
    figures_release(s);
    SPR_update();

    countdown_start(COUNTDOWN_ROUNDS, onRound);
    combat_setRoundHook(countdown_round);
    say("Der Nautiloid stürzt!", "10 RUNDEN BIS ZUM ABSTURZ", "(3 Schritte = 1 Runde)");
    say("Im Westen ringt Zhalk mit", "dem Gedankenschinder. Wer", "sich ihm nähert, kämpft.");
    if (party_has(CLASS_WIR))
        say("WIR: \"Das Steuer! Verbinde", "uns mit dem Transponder,", "schnell!\"");
    if (party_has(CLASS_SHADOWHEART))   // a hint at the BG3 trick (Befehl, src/combat.c)
        say("SCHATTEN flüstert: \"Ein", "Befehl von mir, und er", "lässt die Klinge fallen.\"");
}

static void onTransponder(void)
{
    const char *lines[2] = { "Der Transponder pulsiert.", "Nervenstränge hängen herab." };
    const char *options[2] = { "Nervenstränge verbinden!", "Zurückweichen" };
    if (textbox_show(lines, 2, options, 2) == 0)
        ending_run();
}

static void room6_onInteract(Player *p, RoomObject *obj)
{
    switch (obj->kind)
    {
        case OBJ_TRANSPONDER:      onTransponder(); break;
        case OBJ_TENTACLE_CONSOLE: say("Eine Konsole aus Fleisch.", "Rote Knoten pulsieren im", "Takt des Absturzes."); break;
        case OBJ_FIRE:             say("Brennende Trümmer. Zu heiß,", "um näher heranzugehen.", NULL); break;
        case OBJ_BREACH:           say("Durch den Riss: der", "Höllenschlund, ganz nah.", NULL); break;
        case OBJ_ACID_TANK:
            if (obj->flags & OBJFLAG_BROKEN)
                say("Das Säurefass ist geplatzt.", NULL, NULL);
            else
                say("Ein Fass voll lila Säure.", "Ein Treffer würde es", "platzen lassen.");
            break;
        case OBJ_DOOR_EXIT:        dungeonObjects_tryDoor(p, obj); break;
        default: break;
    }
}

// Die Brücke, 7x12 inside: the transponder at the far (north) end, two groups guarding the middle
// line -- they only close in within 2 cells, so the outer lanes (x = 1 or 7) lead past them --
// Zhalk duelling the mind flayer on the west side (walk up to him and you fight him), acid tanks
// next to both groups, and the cambions hidden by the entrance until round 5.
//
//      x 0 1 2 3 4 5 6 7 8
//  y 0   1 1 1 1 1 1 1 1 1
//    1   1 c . . T . . c 1     T transponder   c tentacle consoles
//    2   R . . . . . . . R     R hull breaches onto the hellmouth
//    3   1 . . . H A . . 1     H hellhounds    A acid tank
//    4   1 . F . . . . . 1     F burning wreckage
//    5   R . . . . . . . 1
//    6   1 Z . . . . . . R     Z Zhalk and the mind flayer
//    7   1 . . . . . . . 1
//    8   R . . A I . . . 1     I imps and a hellhound
//    9   1 . . . . . F . 1
//   10   1 . . . . . . . 1
//   11   1 C . . . . . . 1     C cambions (hidden until round 5)
//   12   1 . . . @ . . . 1     @ arrival from Room 4, facing north
//   13   1 1 1 1 E 1 1 1 1     E door back to Room 4
static const char *const room6Grid[14] = {
    "111111111",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "111111111",
};

static const RoomObject room6Objects[] = {
    { 4, 1, OBJ_TRANSPONDER,      0,                0, 0 },
    { 1, 1, OBJ_TENTACLE_CONSOLE, 0,                0, 0 },
    { 7, 1, OBJ_TENTACLE_CONSOLE, 0,                0, 0 },
    { 4, 3, OBJ_ENEMY_GROUP,      ENC_HOUNDS,       0, 0 },
    { 5, 3, OBJ_ACID_TANK,        0,                0, 0 },
    { 2, 4, OBJ_FIRE,             0,                0, 0 },
    { 1, 6, OBJ_ENEMY_GROUP,      ENC_ZHALK,        0, 0 },
    { 4, 8, OBJ_ENEMY_GROUP,      ENC_BRIDGE_IMPS,  0, 0 },
    { 3, 8, OBJ_ACID_TANK,        0,                0, 0 },
    { 6, 9, OBJ_FIRE,             0,                0, 0 },
    { 1, 11, OBJ_ENEMY_GROUP,     ENC_CAMBIONS,     0, OBJFLAG_HIDDEN },
    { 0, 2, OBJ_BREACH,           0,                0, 0 },
    { 8, 2, OBJ_BREACH,           0,                0, 0 },
    { 0, 5, OBJ_BREACH,           0,                0, 0 },
    { 8, 6, OBJ_BREACH,           0,                0, 0 },
    { 0, 8, OBJ_BREACH,           0,                0, 0 },
    { 4, 13, OBJ_DOOR_EXIT,       ROOM_4,           0, 0 },
};
ROOM_OBJECTS_FIT(room6Objects);

const RoomDef ROOM6 = {
    .roomId = ROOM_6,
    .w = 9, .h = 14,
    .grid = room6Grid,
    .startX = 4, .startY = 12, .startFacing = FACE_NORTH,
    .objectCount = sizeof(room6Objects) / sizeof(room6Objects[0]),
    .objects = room6Objects,
    .onInteract = room6_onInteract,
    .onEnter = onEnter,
};
