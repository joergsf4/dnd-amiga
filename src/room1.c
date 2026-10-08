#include "room1.h"
#include "textbox.h"
#include "skill_check.h"
#include "inventory.h"
#include "party.h"
#include "ui_panel.h"
#include "dungeon_objects.h"
#include "equipment.h"

// Every text line must fit the textbox: at most 27 characters on screen (an umlaut counts as one).

static void say(const char *l0, const char *l1, const char *l2)
{
    const char *lines[3] = { l0, l1, l2 };
    textbox_show(lines, l2 ? 3 : l1 ? 2 : 1, NULL, 0);
}

static void onEnter(Player *p)
{
    static bool introShown;
    (void) p;
    if (introShown) return;
    introShown = TRUE;
    say("Du erwachst in einer", "Kapsel voller Schleim.", "Hinter den Augen brennt es.");
    say("Die Kapsel hinter dir steht", "offen. Wo bist du?", "(Q: Dinge untersuchen)");
}

static void onLarvaPool(RoomObject *obj)
{
    if (obj->flags & OBJFLAG_BROKEN)
    {
        say("Das Becken ist geplatzt.", "Nur noch Säure und", "tote Kaulquappen.");
        return;
    }

    // Once the danger is recognised, the pool is marked and investigating again is pointless. A
    // Magier hero can set it off from a safe distance with Magierhand.
    bool marked = obj->flags & OBJFLAG_MARKED;
    const char *fresh[3] = { "Ein Becken voller zuckender", "Kaulquappen. Die Hülle", "scheint brüchig." };
    const char *known[2] = { "Das Becken ist markiert:", "Die Hülle ist instabil." };
    enum { REACH, INVESTIGATE, MAGE_HAND, LEAVE };
    const char *options[4];
    u8 acts[4], n = 0;
    options[n] = "Hineinfassen"; acts[n++] = REACH;
    if (!marked) { options[n] = "Untersuchen [INT]"; acts[n++] = INVESTIGATE; }
    if (party.members[0].cls == CLASS_MAGE) { options[n] = "Magierhand [Zauber]"; acts[n++] = MAGE_HAND; }
    options[n] = "Weggehen"; acts[n++] = LEAVE;

    u8 choice = acts[marked ? textbox_show(known, 2, options, n) : textbox_show(fresh, 3, options, n)];

    if (choice == MAGE_HAND)
    {
        obj->flags |= OBJFLAG_BROKEN;
        say("Eine Geisterhand stupst das", "Becken an - BOOM! Die Säure", "spritzt ins Leere.");
    }
    else if (choice == REACH)
    {
        Character *hero = &party.members[0];
        hero->hp = (hero->hp > 4) ? (hero->hp - 3) : 1;   // no death yet: never below 1 KP
        obj->flags |= OBJFLAG_BROKEN;
        uiPanel_redrawChrome();
        say("BOOM! Das Becken platzt,", "Säure spritzt dich an.", "-3 KP Schaden.");
    }
    else if (choice == INVESTIGATE)
    {
        if (skillCheck_run(&party.members[0], ATTR_INT, 12))
        {
            obj->flags |= OBJFLAG_MARKED;
            say("Gefahr erkannt: Die Hülle", "ist instabil und würde", "bei Berührung platzen.");
        }
        else
            say("Du erkennst nichts", "Besonderes.", NULL);
    }
    // LEAVE: nothing happens
}

static void onCorpse(RoomObject *obj)
{
    if (obj->flags & OBJFLAG_TRIGGERED)
    {
        say("Der Gedankenschinder ist", "bereits durchsucht.", NULL);
        return;
    }
    obj->flags |= OBJFLAG_TRIGGERED;
    inventory_addGold(15);
    inventory_addGem(1);
    inventory_addEquip(EQ_DAGGER);
    uiPanel_drawInventory();
    say("Ein toter Gedankenschinder.", "Bei ihm: 15 Gold, ein", "Edelstein und ein Dolch.");
}

static void onChest(RoomObject *obj)
{
    if (obj->flags & OBJFLAG_TRIGGERED)
    {
        say("Die Knorpelkiste ist leer.", NULL, NULL);
        return;
    }
    obj->flags |= OBJFLAG_TRIGGERED;
    equip_giveHeroGear(party.members[0].cls);   // the hero woke up with nothing
    inventory_addHealingPotion(1);
    uiPanel_drawInventory();
    say("Eine Knorpelkiste. Darin:", "deine Ausrüstung und", "ein Heiltrank.");
    const char *lines[1] = { "Gleich anlegen?" };
    const char *options[2] = { "Ja", "Später (E: Gruppe)" };
    if (textbox_show(lines, 1, options, 2) == 0) equip_screen(0);
}

static void room1_onInteract(Player *p, RoomObject *obj)
{
    switch (obj->kind)
    {
        case OBJ_LARVA_TANK:         onLarvaPool(obj); break;
        case OBJ_MINDFLAYER_CORPSE:  onCorpse(obj); break;
        case OBJ_CARTILAGE_CHEST:    onChest(obj); break;
        case OBJ_RESTORATION_SHRINE: dungeonObjects_useShrine(p, obj); break;
        case OBJ_POD_OPEN:
            say("Deine Kapsel. Glibbrige", "Reste kleben noch am Glas.", "Hier kamst du heraus.");
            break;
        case OBJ_POD_BROKEN:
            say("Eine zerbrochene", "Klonkapsel. Scherben und", "kalter Schleim. Leer.");
            break;
        case OBJ_DOOR_EXIT:          dungeonObjects_tryDoor(p, obj); break;
        default: break;
    }
}

// Klonkammer, 6x5 inside. Everything except the door stands free on the floor (props, see
// dungeon_view.c) and blocks that cell; the player uses it from a neighbouring cell. The open pod
// the player woke up in stands right behind the start.
//
//      x 0 1 2 3 4 5 6 7
//  y 0   1 1 1 1 1 1 1 1
//    1   D . P . . . C 1     D sphincter door (north-west exit, on the wall)
//    2   1 . . O . . . 1     P broken pod   C chest   O open pod
//    3   1 . . @ . . S 1     @ start, facing south    S restoration station
//    4   1 P . . . . . 1
//    5   1 . . L . P G 1     L larva pool   G mind flayer corpse
//    6   1 1 1 1 1 1 1 1
static const char *const room1Grid[7] = {
    "11111111",
    "10000001",
    "10000001",
    "10000001",
    "10000001",
    "10000001",
    "11111111",
};

// Objects on a wall cell ('1') are drawn as that wall's texture (only the door); objects on a
// floor cell are free-standing props.
static const RoomObject room1Objects[] = {
    { 3, 2, OBJ_POD_OPEN,           0,      0, 0 },
    { 3, 5, OBJ_LARVA_TANK,         0,      0, 0 },
    { 2, 1, OBJ_POD_BROKEN,         0,      0, 0 },
    { 1, 4, OBJ_POD_BROKEN,         0,      0, 0 },
    { 5, 5, OBJ_POD_BROKEN,         0,      0, 0 },
    { 6, 1, OBJ_CARTILAGE_CHEST,    0,      0, 0 },
    { 6, 3, OBJ_RESTORATION_SHRINE, 0,      0, 0 },
    { 6, 5, OBJ_MINDFLAYER_CORPSE,  0,      0, 0 },
    { 0, 1, OBJ_DOOR_EXIT,          ROOM_2, 0, 0 }, // ROOM_2 isn't built yet
};
ROOM_OBJECTS_FIT(room1Objects);

const RoomDef ROOM1 = {
    .roomId = ROOM_1,
    .w = 8, .h = 7,
    .grid = room1Grid,
    .startX = 3, .startY = 3, .startFacing = FACE_SOUTH,
    .objectCount = sizeof(room1Objects) / sizeof(room1Objects[0]),
    .objects = room1Objects,
    .onInteract = room1_onInteract,
    .onEnter = onEnter,
};
