#include "room4.h"
#include "textbox.h"
#include "party.h"
#include "skill_check.h"
#include "inventory.h"
#include "ui_panel.h"
#include "figures.h"
#include "dungeon_view.h"
#include "dungeon_objects.h"
#include "encounter.h"
#include "game.h"

// Every text line must fit the textbox: at most 27 characters on screen (an umlaut counts as one),
// menu options at most 24.

static void say(const char *l0, const char *l1, const char *l2)
{
    const char *lines[3] = { l0, l1, l2 };
    textbox_show(lines, 3, NULL, 0);   // NULL lines are skipped
}

static RoomObject *findObject(ObjectKind kind)
{
    u8 count;
    RoomObject *objects = map_roomObjects(&count);
    for (u8 i = 0; i < count; i++)
        if (objects[i].kind == kind) return &objects[i];
    return NULL;
}

static void onEnter(Player *p)
{
    static bool introShown;
    (void) p;
    if (introShown) return;
    introShown = TRUE;
    say("Eine Halle voller", "versiegelter Kapseln. Eine", "Konsole pulsiert im Takt.");
    if (party_has(CLASS_WIR))
        say("WIR: \"Die Frau in der", "Kapsel. Nützlich. Befreie", "sie.\"");
}

static void onShadowheartPod(RoomObject *obj)
{
    if (obj->flags & OBJFLAG_TRIGGERED)
    {
        say("Die Kapsel ist leer.", NULL, NULL);
        return;
    }
    figures_shakeView();
    say("Eine Frau hämmert gegen die", "Scheibe: \"Hol mich hier", "raus!\"");

    // Design doc, Szene 4 (original): ask her name, study the runes, or leave her be.
    static bool asked, studied;
    const char *lines[2] = { "\"Die Konsole daneben", "braucht einen Schlüssel!\"" };
    enum { ASK, STUDY, NO_TIME };
    const char *options[3];
    u8 acts[3], n = 0;
    if (!asked) { options[n] = "\"Wer bist du?\""; acts[n++] = ASK; }
    if (!studied) { options[n] = "Runen untersuchen [INT]"; acts[n++] = STUDY; }
    options[n] = "\"Keine Zeit!\""; acts[n++] = NO_TIME;
    switch (acts[textbox_show(lines, 2, options, n)])
    {
        case ASK:
            asked = TRUE;
            say("\"Schattenherz. Und jetzt", "hol mich raus, bevor die", "Kaulquappe mich frisst!\"");
            break;
        case STUDY:
            studied = TRUE;
            if (skillCheck_run(&party.members[0], ATTR_INT, 10))
                say("Die Siegel gehorchen einer", "Eldritch-Rune - oder dem", "Willen eines Illithiden.");
            else
                say("Verschlungene Zeichen.", "Du wirst nicht schlau", "daraus.");
            break;
        default:
            say("SCHATTEN: \"Warte! Lass", "mich nicht hier drin!\"", NULL);
            break;
    }
}

// Schattenherz is free: she steps out in front of the player, thanks them and joins.
static void freeShadowheart(Player *p, RoomObject *console)
{
    console->flags |= OBJFLAG_TRIGGERED;
    findObject(OBJ_SHADOWHEART_POD)->flags |= OBJFLAG_TRIGGERED;
    uiPanel_drawInventory();
    dungeonView_render(p);
    say("Puff! Die Kapsel öffnet", "sich zischend.", NULL);

    Sprite *s = figures_add(&fig_shadowheart_sprite, 112, 138);
    figures_wait(40);
    figures_release(s);                         // she steps up close: the bust while she speaks
    s = figures_addBust(&fig_shadowheart_bust_sprite);
    say("SCHATTEN: \"Danke. Ich", "dachte schon, das wäre", "mein Ende.\"");
    say("\"Lass uns diesen", "Höllenort verlassen!\"", NULL);
    figures_release(s);
    SPR_update();

    party_addMember(CLASS_SHADOWHEART);
    uiPanel_initSprites();
    uiPanel_redrawChrome();
    say("SCHATTENHERZ tritt der", "Gruppe bei!", NULL);
    if (party_has(CLASS_LAEZEL))                  // BG3: they can't stand each other
    {
        say("LAE'ZEL: \"Eine Dienerin", "der Shar. Ich behalte sie", "im Auge.\"");
        say("SCHATTEN: \"Und ich dich.", "Charmant wie immer.\"", NULL);
    }
}

static void onPodConsole(Player *p, RoomObject *obj)
{
    if (obj->flags & OBJFLAG_TRIGGERED)
    {
        say("Die Rune glüht im Sockel.", NULL, NULL);
        return;
    }
    // Design doc, Szene 4 (original): the rune, the tadpole's power (BG3: an illithid WEI check
    // against 2 -- here it simply works, and it's unsettling), or force.
    bool rune = inventory_hasItem(ITEM_RUNE);
    const char *withRune[2] = { "Ein runder Sockel - genau", "die Form der Rune." };
    const char *noRune[2] = { "Ein runder Sockel fehlt.", "Vielleicht im Nebenraum?" };
    enum { RUNE, TADPOLE, FORCE, LEAVE };
    const char *options[4];
    u8 acts[4], n = 0;
    if (rune) { options[n] = "Rune einsetzen"; acts[n++] = RUNE; }
    options[n] = "Kaulquappe nutzen"; acts[n++] = TADPOLE;
    options[n] = "Gewalt anwenden [STÄ]"; acts[n++] = FORCE;
    options[n] = "Weggehen"; acts[n++] = LEAVE;
    switch (acts[textbox_show(rune ? withRune : noRune, 2, options, n)])
    {
        case RUNE:
            inventory_takeItem(ITEM_RUNE);
            freeShadowheart(p, obj);
            break;
        case TADPOLE:
            figures_shakeView();
            say("Du legst die Hand auf. In", "deinem Kopf windet sich", "etwas - und greift zu.");
            say("Die Konsole gehorcht dir", "wie einem Meister...", NULL);
            if (party_has(CLASS_WIR))
                say("WIR: \"Ja... Du spürst es", "auch. Die Kraft der", "Meister.\"");
            freeShadowheart(p, obj);
            break;
        case FORCE:
            say("Die Konsole funkelt,", "nichts passiert.", NULL);
            break;
        default:
            break;
    }
}

static void setPods(u8 flag)
{
    u8 count;
    RoomObject *objects = map_roomObjects(&count);
    for (u8 i = 0; i < count; i++)
        if (objects[i].kind == OBJ_POD_SEALED) objects[i].flags |= flag;
}

// The three buttons (design doc, Raum 4): 1 does nothing, 2 releases the inmates (they attack),
// 3 kills them. 2 and 3 only work once.
static void onButtonConsole(Player *p, RoomObject *obj)
{
    const char *lines[3] = { "Ein Pult mit drei Tasten,", "pulsierend im Takt der", "Kapseln." };
    const char *options[4] = { "Taste 1", "Taste 2", "Taste 3", "Weggehen" };
    u8 choice = textbox_show(lines, 3, options, 4);
    if (choice == 3) return;
    if (choice == 0 || (obj->flags & OBJFLAG_TRIGGERED))
    {
        say("Nichts passiert.", NULL, NULL);
        return;
    }
    obj->flags |= OBJFLAG_TRIGGERED;
    if (choice == 1)
    {
        setPods(OBJFLAG_TRIGGERED);
        findObject(OBJ_ENEMY_GROUP)->flags &= ~OBJFLAG_HIDDEN;
        dungeonView_render(p);
        figures_shakeView();
        say("Die Kapseln zischen auf!", "Zwei Kobolde brechen", "heraus!");
    }
    else
    {
        setPods(OBJFLAG_MARKED);
        dungeonView_render(p);
        say("Ein Zucken geht durch die", "Kapseln. Die Insassen", "sterben lautlos.");
    }
}

static void onPod(const RoomObject *obj)
{
    if (obj->flags & OBJFLAG_TRIGGERED)
        say("Die Kapsel ist aufgeplatzt", "und leer.", NULL);
    else if (obj->flags & OBJFLAG_MARKED)
        say("Die Gestalt in der Kapsel", "regt sich nicht mehr.", NULL);
    else
        say("In der Kapsel treibt eine", "dunkle Gestalt.", NULL);
}

static void room4_onInteract(Player *p, RoomObject *obj)
{
    switch (obj->kind)
    {
        case OBJ_SHADOWHEART_POD: onShadowheartPod(obj); break;
        case OBJ_POD_CONSOLE:     onPodConsole(p, obj); break;
        case OBJ_BUTTON_CONSOLE:  onButtonConsole(p, obj); break;
        case OBJ_POD_SEALED:      onPod(obj); break;
        case OBJ_DOOR_EXIT:
            // The gate to the bridge opens "once ready" (design doc): when Schattenherz is free.
            if (obj->param0 == ROOM_6 && !(findObject(OBJ_SHADOWHEART_POD)->flags & OBJFLAG_TRIGGERED))
                say("Das Tor zur Brücke ist", "noch versiegelt.", NULL);
            else
                dungeonObjects_tryDoor(p, obj);
            break;
        default: break;
    }
}

// Kapselsaal, 7x7 inside: sealed pods along the walls, Schattenherz's pod on the west side with
// its console, the console with three buttons in the middle. North: the gate to the bridge
// (Room 6), east: the passage to the lab (Room 5), south: back to Room 3. The two imps (button 2)
// are hidden until released.
//
//      x 0 1 2 3 4 5 6 7 8
//  y 0   1 1 1 1 N 1 1 1 1     N gate to Room 6
//    1   1 P . P . P . P 1     P sealed pods
//    2   1 . . . . . I . 1     I imps (hidden until button 2)
//    3   1 H c . . . . . 1     H Schattenherz's pod   c its console
//    4   1 . . . K . . . E     K console with three buttons   E passage to Room 5
//    5   1 . . . . . . . 1
//    6   1 P . . . . . P 1
//    7   1 . . . @ . . . 1     @ arrival from Room 3, facing north
//    8   1 1 1 1 S 1 1 1 1     S door back to Room 3
static const char *const room4Grid[9] = {
    "111111111",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "111111111",
};

static const RoomObject room4Objects[] = {
    { 1, 3, OBJ_SHADOWHEART_POD, 0,         0, 0 },
    { 2, 3, OBJ_POD_CONSOLE,     0,         0, 0 },
    { 4, 4, OBJ_BUTTON_CONSOLE,  0,         0, 0 },
    { 1, 1, OBJ_POD_SEALED,      0,         0, 0 },
    { 3, 1, OBJ_POD_SEALED,      0,         0, 0 },
    { 5, 1, OBJ_POD_SEALED,      0,         0, 0 },
    { 7, 1, OBJ_POD_SEALED,      0,         0, 0 },
    { 1, 6, OBJ_POD_SEALED,      0,         0, 0 },
    { 7, 6, OBJ_POD_SEALED,      0,         0, 0 },
    { 6, 2, OBJ_ENEMY_GROUP,     ENC_IMPS2, 0, OBJFLAG_HIDDEN },
    { 4, 8, OBJ_DOOR_EXIT,       ROOM_3,    0, 0 },
    { 8, 4, OBJ_DOOR_EXIT,       ROOM_5,    0, 0 },
    { 4, 0, OBJ_DOOR_EXIT,       ROOM_6,    0, 0 }, // opens once Schattenherz is free
};
ROOM_OBJECTS_FIT(room4Objects);

const RoomDef ROOM4 = {
    .roomId = ROOM_4,
    .w = 9, .h = 9,
    .grid = room4Grid,
    .startX = 4, .startY = 7, .startFacing = FACE_NORTH,
    .objectCount = sizeof(room4Objects) / sizeof(room4Objects[0]),
    .objects = room4Objects,
    .onInteract = room4_onInteract,
    .onEnter = onEnter,
};
