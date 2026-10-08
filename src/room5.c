#include "room5.h"
#include "textbox.h"
#include "inventory.h"
#include "ui_panel.h"
#include "figures.h"
#include "dungeon_view.h"
#include "dungeon_objects.h"
#include "skill_check.h"
#include "equipment.h"
#include "party.h"
#include "game.h"

// Every text line must fit the textbox: at most 27 characters on screen (an umlaut counts as one),
// menu options at most 24.

static void say(const char *l0, const char *l1, const char *l2)
{
    const char *lines[3] = { l0, l1, l2 };
    textbox_show(lines, 3, NULL, 0);   // NULL lines are skipped
}

static void onEnter(Player *p)
{
    static bool introShown;
    (void) p;
    if (introShown) return;
    introShown = TRUE;
    say("Ein unheimlicher Brutsaal.", "Am Boden liegt eine tote", "Klerikerin.");
    say("In einer Kapsel treibt", "eine Frau.", NULL);
}

static RoomObject *womanPod(void)
{
    u8 count;
    RoomObject *objects = map_roomObjects(&count);
    for (u8 i = 0; i < count; i++)
        if (objects[i].kind == OBJ_WOMAN_POD) return &objects[i];
    return NULL;
}

// Ceremorphosis in a few seconds: purple mist (the backdrop colour flashes) while the pod
// flickers between the woman and what she becomes.
static void transform(Player *p, RoomObject *pod)
{
    for (u8 i = 0; i < 8; i++)
    {
        PAL_setColor(0, i & 1 ? 0 : 0x0A28);   // purple
        if (i & 1) pod->flags ^= OBJFLAG_TRIGGERED;
        dungeonView_render(p);
        figures_wait(4 + i);
    }
    pod->flags |= OBJFLAG_TRIGGERED;
    PAL_setColor(0, 0);                        // back to the view's black
    dungeonView_render(p);
}

// The transformation switch (design doc, Szene 5, demake version).
static void onSwitch(Player *p, RoomObject *obj)
{
    if (obj->flags & OBJFLAG_TRIGGERED)
    {
        say("Der Knorpelschalter ist", "erschlafft.", NULL);
        return;
    }
    const char *lines[2] = { "Ein Schalter aus Knorpel,", "verbunden mit der Kapsel." };
    const char *options[3] = { "Taste 1: Auslösen", "Taste 2: Vernichten", "Nichts tun" };
    u8 choice = textbox_show(lines, 2, options, 3);
    if (choice == 2) return;

    obj->flags |= OBJFLAG_TRIGGERED;
    RoomObject *pod = womanPod();
    if (choice == 0)
    {
        say("Lila Nebel füllt die", "Kapsel...", NULL);
        transform(p, pod);
        say("Die Frau verwandelt sich", "in einen Gedankenschinder!", NULL);
    }
    else
    {
        pod->flags |= OBJFLAG_BROKEN;
        figures_shakeView();
        dungeonView_render(p);
        say("Ein Blitz zuckt durch den", "Raum. Die Kapsel erlischt.", NULL);
    }
}

static void onWomanPod(const RoomObject *obj)
{
    if (obj->flags & OBJFLAG_BROKEN)
        say("Die Kapsel ist dunkel.", NULL, NULL);
    else if (obj->flags & OBJFLAG_TRIGGERED)
        say("Ein Gedankenschinder", "starrt dich durch das", "Glas an.");
    else
        say("Eine Frau in zerrissener", "Kleidung, reglos im", "Schleim.");
}

static void onCleric(RoomObject *obj)
{
    if (obj->flags & OBJFLAG_TRIGGERED)
    {
        say("Die Klerikerin ist", "bereits durchsucht.", NULL);
        return;
    }
    obj->flags |= OBJFLAG_TRIGGERED;
    inventory_giveItem(ITEM_RUNE);
    inventory_giveItem(ITEM_GOLD_KEY);
    uiPanel_drawInventory();
    say("Eine tote Klerikerin. Bei", "ihr: eine Eldritch-Rune und", "ein verzierter Schlüssel.");
}

static void onOrnateChest(RoomObject *obj)
{
    if (obj->flags & OBJFLAG_TRIGGERED)
    {
        say("Die verzierte Truhe", "ist leer.", NULL);
        return;
    }
    if (!inventory_hasItem(ITEM_GOLD_KEY))
    {
        // A Schurke picks the lock (GES, with Expertise); anyone else needs the key.
        if (party.members[0].cls != CLASS_ROGUE)
        {
            say("Eine verzierte Truhe.", "Sie ist verschlossen.", NULL);
            return;
        }
        const char *lines[2] = { "Eine verzierte Truhe.", "Sie ist verschlossen." };
        const char *options[2] = { "Schloss knacken [GES]", "Weggehen" };
        if (textbox_show(lines, 2, options, 2)) return;
        if (!skillCheck_run(&party.members[0], ATTR_DEX, 15))
        {
            say("Der Dietrich rutscht ab.", NULL, NULL);
            return;
        }
        obj->flags |= OBJFLAG_TRIGGERED;
        inventory_giveItem(ITEM_SCROLL);
        inventory_addEquip(EQ_STUDDED);
        inventory_addGold(25);
        inventory_addGem(1);
        uiPanel_drawInventory();
        say("Klick! Darin: 25 Gold,", "Schriftrolle, Onyx und", "beschlagenes Leder.");
        return;
    }
    obj->flags |= OBJFLAG_TRIGGERED;
    inventory_takeItem(ITEM_GOLD_KEY);
    inventory_giveItem(ITEM_SCROLL);
    inventory_addEquip(EQ_STUDDED);
    inventory_addGold(25);
    inventory_addGem(1);
    uiPanel_drawInventory();
    say("Der Schlüssel passt! Darin:", "25 Gold, Schriftrolle, Onyx", "und beschlagenes Leder.");
}

static void room5_onInteract(Player *p, RoomObject *obj)
{
    switch (obj->kind)
    {
        case OBJ_SWITCH:        onSwitch(p, obj); break;
        case OBJ_WOMAN_POD:     onWomanPod(obj); break;
        case OBJ_CLERIC:        onCleric(obj); break;
        case OBJ_ORNATE_CHEST:  onOrnateChest(obj); break;
        case OBJ_DOOR_EXIT:     dungeonObjects_tryDoor(p, obj); break;
        default: break;
    }
}

// Transformations-Labor, 5x4 inside, off Room 4's east side.
//
//      x 0 1 2 3 4 5 6
//  y 0   1 1 1 1 1 1 1
//    1   1 . . W S . 1     W pod with the woman   S transformation switch
//    2   D @ . . . T 1     D door back to Room 4   @ arrival, facing east   T ornate chest
//    3   1 . . C . . 1     C the dead cleric (rune and key)
//    4   1 . . . . . 1
//    5   1 1 1 1 1 1 1
static const char *const room5Grid[6] = {
    "1111111",
    "1000001",
    "1000001",
    "1000001",
    "1000001",
    "1111111",
};

static const RoomObject room5Objects[] = {
    { 3, 1, OBJ_WOMAN_POD,    0,      0, 0 },
    { 4, 1, OBJ_SWITCH,       0,      0, 0 },
    { 5, 2, OBJ_ORNATE_CHEST, 0,      0, 0 },
    { 3, 3, OBJ_CLERIC,       0,      0, 0 },
    { 0, 2, OBJ_DOOR_EXIT,    ROOM_4, 0, 0 },
};
ROOM_OBJECTS_FIT(room5Objects);

const RoomDef ROOM5 = {
    .roomId = ROOM_5,
    .w = 7, .h = 6,
    .grid = room5Grid,
    .startX = 1, .startY = 2, .startFacing = FACE_EAST,
    .objectCount = sizeof(room5Objects) / sizeof(room5Objects[0]),
    .objects = room5Objects,
    .onInteract = room5_onInteract,
    .onEnter = onEnter,
};
