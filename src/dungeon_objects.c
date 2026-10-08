#include "dungeon_objects.h"
#include "textbox.h"
#include "party.h"
#include "ui_panel.h"
#include "sfx.h"
#include "dungeon_view.h"
#include "figures.h"

RoomObject *dungeonObjects_interactTarget(const Player *p)
{
    s16 dx, dy;
    map_forward(p->facing, &dx, &dy);
    return map_objectAt(p->x + dx, p->y + dy);
}

void dungeonObjects_tryDoor(Player *p, RoomObject *obj)
{
    const RoomDef *target = map_findRoom((RoomId) obj->param0);
    if (!target)
    {
        const char *lines[2] = { "Die Sphinktertür zuckt,", "bleibt aber verschlossen." };
        textbox_show(lines, 2, NULL, 0);
        return;
    }
    sfx_play(SFX_DOOR);
    map_enterRoom(target, p, map_currentRoom()->roomId);
}

void dungeonObjects_useShrine(Player *p, RoomObject *shrine)
{
    sfx_play(SFX_HEAL);
    static const u8 squeeze[] = { 1, 2, 2, 1, 0 };     // the frames of the contraction (param1)
    for (u8 i = 0; i < sizeof(squeeze); i++)
    {
        shrine->param1 = squeeze[i];
        dungeonView_render(p);
        figures_wait(6);
    }
    for (u8 i = 0; i < PARTY_MAX; i++)
    {
        if (party.members[i].active)
        {
            party.members[i].hp = party.members[i].hpMax;
            party.members[i].mp = party.members[i].mpMax;
        }
    }
    uiPanel_redrawChrome();
    const char *lines[3] = { "Die Heilblase zieht sich", "zusammen - glitzernde", "Partikel heilen euch." };
    textbox_show(lines, 3, NULL, 0);
}
