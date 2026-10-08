#ifndef _COMBAT_H_
#define _COMBAT_H_

#include <genesis.h>
#include "dungeon_map.h"

// Turn-based menu combat in the first-person view, in the vein of Shining in the Darkness: the
// enemies stand in front of the party as figures (src/figures.c), everyone acts in initiative
// order, the player picks every party member's action from a menu in the message area.

#define COMBAT_MAX_ENEMIES 3

typedef struct
{
    const char *name;                 // at most 6 characters: a letter is appended ("KOBOLD A")
    u8 hpMax, ac, atk, dmgDie, dmgBonus;
    u8 fireDie;                       // extra fire damage per hit (0: none)
    u8 attacks;                       // attacks per turn
    s8 wis;                           // Weisheitsrettungswurf bonus (Schattenherz's Befehl)
    const SpriteDefinition *sprite;
    s16 bottom;                       // screen y of the figure's feet (hovering ones are higher)
    bool burning;                     // cycle the fire colours while it's in the fight (Zhalk)
} EnemyDef;

extern const EnemyDef ENEMY_IMP, ENEMY_HOUND, ENEMY_CAMBION, ENEMY_ZHALK;

// Called at the start of every combat round (NULL: none) -- Room 6's countdown counts them.
void combat_setRoundHook(void (*hook)(void));

// Fights `count` enemies (up to COMBAT_MAX_ENEMIES). `tank`, if not NULL, is an acid tank within
// reach: it's offered as a target and, once hit, bursts and hurts every enemy (the design doc's
// explosive Nautiloid tanks). Blocking; returns TRUE when the party has won, FALSE when it got
// away (only offered with `canFlee`: Zhalk, who stays put) -- on defeat it shows the game-over
// screen and restarts the game.
// `p` is only needed to redraw the view (a bursting tank).
bool combat_run(const Player *p, const EnemyDef *const enemies[], u8 count, RoomObject *tank, u8 goldReward,
                bool canFlee);

// After a won fight: did any enemy run off (Befehl "Flieh!") instead of falling?
bool combat_foeFled(void);

#endif
