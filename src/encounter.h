#ifndef _ENCOUNTER_H_
#define _ENCOUNTER_H_

#include "dungeon_map.h"
#include "combat.h"

// Enemies in the dungeon: OBJ_ENEMY_GROUP objects stand on floor cells, visible from afar as a
// prop, and close in on the player in real time. Once one is next to the player the fight starts
// (combat.c). param0 of the object picks the encounter below.

typedef enum
{
    ENC_IMPS3 = 0,   // Room 3: three imps through the hull breach
    ENC_IMPS2,       // Room 4: two imps released from the pods (button 2)
    ENC_BRIDGE_IMPS, // Room 6: two imps and a hellhound
    ENC_HOUNDS,      // Room 6: two hellhounds guarding the transponder
    ENC_CAMBIONS,    // Room 6: two cambions, arriving from behind in round 5
    ENC_ZHALK,       // Room 6: Kommandant Zhalk, busy duelling the mind flayer
} EncounterId;


// Moves every visible enemy group within reach one step towards the player (called on a timer
// from main.c). Returns TRUE if any moved, so the view needs a redraw.
bool encounter_tick(const Player *p);

// The visible enemy group next to the player (not diagonal), or NULL.
RoomObject *encounter_adjacent(const Player *p);

// Turns the player towards `group` and fights it; the group is gone afterwards. An intact acid
// tank within 3 cells joins the fight as a target. Returns only on victory (see combat_run).
// Beating Zhalk wins the Immerbrand-Klinge.
void encounter_fight(Player *p, RoomObject *group);

#endif
