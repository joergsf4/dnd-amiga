#include "room2.h"
#include "textbox.h"
#include "skill_check.h"
#include "party.h"
#include "ui_panel.h"
#include "dungeon_objects.h"

// Every text line must fit the textbox: at most 27 characters on screen (an umlaut counts as one),
// menu options at most 24.

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
    say("Ein Operationssaal.", "Auf Seziertischen liegen", "zerlegte Körper.");
}

// "Wir" is out of the skull: tame it or take it as it is. Either way it joins; lobotomised it's
// weaker but obeys blindly (design doc, Szene 2, Menü 2).
static void recruitWir(void)
{
    const char *lines[2] = { "Das Wesen hockt vor dir", "und wartet auf Befehle." };
    const char *options[2] = { "Lobotomieren [GES]", "Als Begleiter aufnehmen" };
    bool lobotomised = FALSE;
    if (textbox_show(lines, 2, options, 2) == 0)
    {
        lobotomised = skillCheck_run(&party.members[0], ATTR_DEX, 15);
        if (lobotomised)
            say("Ein präziser Schnitt. Das", "Gehirn wird schwächer,", "gehorcht aber blind.");
        else
            say("Du rutschst ab. Das Gehirn", "zuckt, bleibt aber", "unversehrt.");
    }

    Character *wir = party_addMember(CLASS_WIR);
    if (wir && lobotomised)
    {
        wir->hpMax = wir->hp = 7;
        wir->dex -= 1;
        wir->intl = 1;
    }
    uiPanel_initSprites();
    uiPanel_redrawChrome();
    say("\"Wir\" schließt sich", "deiner Gruppe an.", NULL);
    if (lobotomised)
        say("WIR: \"...Befehl?\"", NULL, NULL);
    else
        say("WIR: \"Wir sind frei! Zum", "Steuer, schnell - bevor", "die Meister zurückkehren!\"");
}

static void onMyrnath(RoomObject *obj)
{
    if (obj->flags & OBJFLAG_BROKEN)
    {
        say("Der Elf ist tot, das", "Gehirn zerquetscht.", NULL);
        return;
    }
    if (obj->flags & OBJFLAG_TRIGGERED)
    {
        say("Der Elf ist tot.", "Sein Schädel ist leer.", NULL);
        return;
    }

    // Design doc, Szene 2 (original): investigating the brain first makes the careful way easier.
    static bool investigated, understood;
    const char *lines[3] = { "Ein Elf, der Schädel offen.", "Eine Stimme in deinem Kopf:", "\"Befreie uns! Zum Ruder!\"" };
    enum { INVESTIGATE, FREE, CRUSH, IGNORE, BREAK, LOOSEN };
    const char *options[4];
    u8 acts[4], n = 0;
    if (!investigated) { options[n] = "Gehirn untersuchen [INT]"; acts[n++] = INVESTIGATE; }
    options[n] = "Befreien..."; acts[n++] = FREE;
    options[n] = "Gehirn zerquetschen"; acts[n++] = CRUSH;
    options[n] = "Ignorieren"; acts[n++] = IGNORE;
    u8 act = acts[textbox_show(lines, 3, options, n)];
    if (act == IGNORE) return;
    if (act == INVESTIGATE)
    {
        investigated = TRUE;
        understood = skillCheck_run(&party.members[0], ATTR_INT, 10);
        if (understood)
            say("Die Nervenstränge sind nur", "lose verankert. Behutsam", "gelöst, gleitet es heraus.");
        else
            say("Ein Gewirr aus Nerven und", "Schleim. Du wirst daraus", "nicht schlau.");
        return;
    }
    if (act == FREE)
    {
        const char *how[1] = { "Wie befreien?" };
        const char *ways[3] = { "Schädel aufbrechen [STÄ]", understood ? "Behutsam lösen [GES-4]" : "Behutsam lösen [GES]", "Zurück" };
        u8 way = textbox_show(how, 1, ways, 3);
        if (way == 2) return;
        act = way == 0 ? BREAK : LOOSEN;
    }
    u8 choice = act == CRUSH ? 2 : act == BREAK ? 0 : 1;

    // From here on the elf is lost whatever happens: the brain was all that kept him going.
    obj->flags |= OBJFLAG_TRIGGERED;
    if (choice == 2)
    {
        obj->flags |= OBJFLAG_BROKEN;
        say("Du zerquetschst das Gehirn.", "Die Stimme verstummt.", NULL);
        return;
    }

    // A failed check kills the brain (as in BG3); there's no second attempt.
    bool freed = choice == 0 ? skillCheck_run(&party.members[0], ATTR_STR, 12)
                             : skillCheck_run(&party.members[0], ATTR_DEX, understood ? 8 : 12);
    if (!freed)
    {
        obj->flags |= OBJFLAG_BROKEN;
        say("Das Gehirn zuckt, dann", "erschlafft es. Es ist tot.", NULL);
        return;
    }
    if (choice == 0)
        say("Knirschend gibt der Schädel", "nach. Das Gehirn springt", "heraus - auf vier Beinen!");
    else
        say("Mit ruhiger Hand löst du", "das Gehirn. Es springt", "heraus - auf vier Beinen!");
    recruitWir();
}

static void onOpTable(const RoomObject *obj)
{
    switch (obj->param0)
    {
        case 0:  say("Ein Seziertisch. Darauf", "die Reste eines Githyanki,", "säuberlich zerlegt."); break;
        case 1:  say("Ein halb zerlegter Kobold.", "Er riecht nach Schwefel.", NULL); break;
        default: say("Ein leerer Seziertisch.", "Die Fesseln sind zerrissen.", NULL); break;
    }
}

static void onLectern(void)
{
    say("Ein Pult, aus dem Boden", "gewachsen. Darauf glimmen", "fremde Schriftzeichen:");
    say("\"Die Larven reifen sieben", "Tage im Wirt. Dann beginnt", "die Umwandlung.\"");
}

static void onTablet(const RoomObject *obj)
{
    if (obj->param0 == 0)
        say("Eine Knorpeltafel: Ein", "Schädel, von Tentakeln", "umschlungen. Anleitung?");
    else
        say("Eine Tafel: Der Nautiloid", "rast durch Avernus, die", "erste Höllenebene.");
}

static void room2_onInteract(Player *p, RoomObject *obj)
{
    switch (obj->kind)
    {
        case OBJ_MYRNATH:   onMyrnath(obj); break;
        case OBJ_OP_TABLE:  onOpTable(obj); break;
        case OBJ_LECTERN:   onLectern(); break;
        case OBJ_TABLET:    onTablet(obj); break;
        case OBJ_DOOR_EXIT: dungeonObjects_tryDoor(p, obj); break;
        default: break;
    }
}

// Operationssaal, 7x6 inside, flat (the design doc's upper floor and lift don't fit the grid
// engine). Myrnath sits in the middle, the player comes in from Room 1 through the south door.
//
//      x 0 1 2 3 4 5 6 7 8
//  y 0   1 1 1 1 X 1 1 1 1     X door to Room 3, the outer deck
//    1   1 . . . . . . . 1
//    2   T . V . . . V . 1     T tablets on the walls   V vivisection tables
//    3   1 . . . M . . . 1     M Myrnath on the operating couch
//    4   1 . . . . . . . T
//    5   1 . P . . . V . 1     P lectern with the mind flayers' notes
//    6   1 . . . @ . . . 1     @ arrival from Room 1, facing north
//    7   1 1 1 1 E 1 1 1 1     E door back to Room 1
static const char *const room2Grid[8] = {
    "111111111",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "100000001",
    "111111111",
};

static const RoomObject room2Objects[] = {
    { 4, 3, OBJ_MYRNATH,   0,      0, 0 },
    { 2, 2, OBJ_OP_TABLE,  0,      0, 0 },
    { 6, 2, OBJ_OP_TABLE,  1,      0, 0 },
    { 6, 5, OBJ_OP_TABLE,  2,      0, 0 },
    { 2, 5, OBJ_LECTERN,   0,      0, 0 },
    { 0, 2, OBJ_TABLET,    0,      0, 0 },
    { 8, 4, OBJ_TABLET,    1,      0, 0 },
    { 4, 7, OBJ_DOOR_EXIT, ROOM_1, 0, 0 },
    { 4, 0, OBJ_DOOR_EXIT, ROOM_3, 0, 0 }, // ROOM_3 isn't built yet
};
ROOM_OBJECTS_FIT(room2Objects);

const RoomDef ROOM2 = {
    .roomId = ROOM_2,
    .w = 9, .h = 8,
    .grid = room2Grid,
    .startX = 4, .startY = 6, .startFacing = FACE_NORTH,
    .objectCount = sizeof(room2Objects) / sizeof(room2Objects[0]),
    .objects = room2Objects,
    .onInteract = room2_onInteract,
    .onEnter = onEnter,
};
