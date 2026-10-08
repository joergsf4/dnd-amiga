#ifndef _DUNGEON_OBJECTS_H_
#define _DUNGEON_OBJECTS_H_

#include "dungeon_map.h"

// Interaction plumbing shared by all rooms; drawing objects is dungeon_view.c's job.

// The object directly ahead of the player (one step, in the facing direction), or NULL.
RoomObject *dungeonObjects_interactTarget(const Player *p);

// Shared handler for OBJ_DOOR_EXIT objects (param0 = target RoomId): transitions to the target
// room if it's been registered (map_registerRoom), otherwise shows a "still sealed" stub -- every
// room's exit door needs this same not-built-yet check before it's actually wired up.
void dungeonObjects_tryDoor(Player *p, RoomObject *obj);

// Restoration station: it contracts, puffing out glittering particles, and heals every party
// member's KP and ZP fully (reusable).
void dungeonObjects_useShrine(Player *p, RoomObject *shrine);

#endif
