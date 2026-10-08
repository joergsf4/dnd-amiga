#include "room3.h"
#include "textbox.h"
#include "skill_check.h"
#include "party.h"
#include "ui_panel.h"
#include "figures.h"
#include "dungeon_view.h"
#include "dungeon_objects.h"
#include "encounter.h"
#include "sfx.h"
#include "game.h"

// Every text line must fit the textbox: at most 27 characters on screen (an umlaut counts as one),
// menu options at most 24.

static void say(const char *l0, const char *l1, const char *l2)
{
    const char *lines[3] = { l0, l1, l2 };
    textbox_show(lines, 3, NULL, 0);   // NULL lines are skipped
}

// Lae'zel drops from above right in front of the player, sword first.
static Sprite *laezelLands(void)
{
    Sprite *s = figures_add(&fig_laezel_sprite, 112, 0);
    for (s16 bottom = 0; bottom < 138; bottom += 12)
    {
        SPR_setPosition(s, 112 - 24, bottom - 96);
        figures_wait(1);
    }
    SPR_setPosition(s, 112 - 24, 138 - 96);
    sfx_play(SFX_LAND);
    figures_shakeView();
    return s;
}

// Arrival (first visit): the view through the breach, Lae'zel's ambush, then the imps. Design doc:
// Szene 3, demake version.
static void onEnter(Player *p)
{
    static bool done;
    if (done) return;
    done = TRUE;

    say("Wind heult durch einen Riss", "in der Hülle. Draußen", "brennt der Himmel: Avernus.");
    say("Da! Über dir bewegt sich", "etwas...", NULL);

    Sprite *laezel = laezelLands();
    figures_wait(30);
    figures_release(laezel);                    // she steps up close: the bust while she speaks
    laezel = figures_addBust(&fig_laezel_bust_sprite);
    say("LAE'ZEL: \"Ein Überlebender!", "Halt still... Dein Kopf", "pulsiert.\"");
    const char *lines[2] = { "\"Du bist infiziert -", "genau wie ich!\"" };
    const char *options[3] = { "\"Gemeinsam kämpfen!\"", "\"Wer bist du überhaupt?\"", "Waffe ziehen [STÄ]" };
    switch (textbox_show(lines, 2, options, 3))
    {
        case 1:
            say("\"Ich bin Lae'zel von den", "Githyanki. Diskutiert wird", "später!\"");
            break;
        case 2:                                   // design doc, Szene 3 (original)
            if (skillCheck_run(&party.members[0], ATTR_STR, 12))
                say("LAE'ZEL grinst: \"Ha! Du", "hast Biss, Istik. Den", "brauchen wir gleich.\"");
            else
                say("LAE'ZEL: \"Steck das weg,", "bevor du dich selbst", "schneidest, Istik.\"");
            break;
        default:
            break;
    }
    say("\"Erst schlagen wir uns zum", "Steuerpult durch!\"", NULL);

    figures_release(laezel);
    SPR_update();
    party_addMember(CLASS_LAEZEL);
    uiPanel_initSprites();
    uiPanel_redrawChrome();
    say("LAE'ZEL tritt der", "Gruppe bei!", NULL);

    // The imps come in through the breach: revealed now, they close in on their own (encounter.c).
    u8 count;
    RoomObject *objects = map_roomObjects(&count);
    for (u8 i = 0; i < count; i++)
        if (objects[i].kind == OBJ_ENEMY_GROUP) objects[i].flags &= ~OBJFLAG_HIDDEN;
    dungeonView_render(p);
    say("Kreischend flattern drei", "Kobolde durch den Riss!", NULL);
}

static void room3_onInteract(Player *p, RoomObject *obj)
{
    switch (obj->kind)
    {
        case OBJ_FIRE:
            say("Brennende Trümmer. Zu heiß,", "um näher heranzugehen.", NULL);
            break;
        case OBJ_ACID_TANK:
            if (obj->flags & OBJFLAG_BROKEN)
                say("Das Säurefass ist geplatzt.", "Lila Säure frisst sich in", "den Boden.");
            else
                say("Ein Fass voll lila Säure.", "Ein Treffer würde es", "platzen lassen.");
            break;
        case OBJ_BREACH:
            say("Durch den Riss: Avernus.", "Felsen treiben im Glühen,", "fern kreist ein Drache.");
            break;
        case OBJ_RESTORATION_SHRINE: dungeonObjects_useShrine(p, obj); break;
        case OBJ_DOOR_EXIT:          dungeonObjects_tryDoor(p, obj); break;
        default: break;
    }
}

// Zerstörtes Außendeck: a wrecked corridor, 3 wide, torn open to the sky of Avernus in three
// places. The player comes in from Room 2 through the south door; the imps are hidden until
// Lae'zel has joined (onEnter), then come at the party from the middle of the corridor. The acid
// tank next to their path is in reach for the fight. A restoration station waits before the
// door to Room 4 (the doc's heal points before the next fight).
//
//      x 0 1 2 3 4
//  y 0   1 1 X 1 1     X door to Room 4
//    1   1 . . . 1
//    2   1 . . S 1     S restoration station
//    3   R . . . 1     R breach in the hull
//    4   1 F . . R     F burning wreckage
//    5   1 . I . 1     I imps (hidden at first)
//    6   R . . T 1     T acid tank
//    7   1 F . . 1
//    8   1 . @ . 1     @ arrival from Room 2, facing north
//    9   1 1 E 1 1     E door back to Room 2
static const char *const room3Grid[10] = {
    "11111",
    "10001",
    "10001",
    "10001",
    "10001",
    "10001",
    "10001",
    "10001",
    "10001",
    "11111",
};

static const RoomObject room3Objects[] = {
    { 2, 5, OBJ_ENEMY_GROUP,        ENC_IMPS3, 0, OBJFLAG_HIDDEN },
    { 3, 6, OBJ_ACID_TANK,          0,         0, 0 },
    { 1, 4, OBJ_FIRE,               0,         0, 0 },
    { 1, 7, OBJ_FIRE,               0,         0, 0 },
    { 3, 2, OBJ_RESTORATION_SHRINE, 0,         0, 0 },
    { 0, 3, OBJ_BREACH,             0,         0, 0 },
    { 4, 4, OBJ_BREACH,             0,         0, 0 },
    { 0, 6, OBJ_BREACH,             0,         0, 0 },
    { 2, 9, OBJ_DOOR_EXIT,          ROOM_2,    0, 0 },
    { 2, 0, OBJ_DOOR_EXIT,          ROOM_4,    0, 0 }, // ROOM_4 isn't built yet
};
ROOM_OBJECTS_FIT(room3Objects);

const RoomDef ROOM3 = {
    .roomId = ROOM_3,
    .w = 5, .h = 10,
    .grid = room3Grid,
    .startX = 2, .startY = 8, .startFacing = FACE_NORTH,
    .objectCount = sizeof(room3Objects) / sizeof(room3Objects[0]),
    .objects = room3Objects,
    .onInteract = room3_onInteract,
    .onEnter = onEnter,
};
