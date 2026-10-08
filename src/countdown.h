#ifndef _COUNTDOWN_H_
#define _COUNTDOWN_H_

#include <genesis.h>

// Room 6's race against the crash: "10 RUNDEN BIS ZUM ABSTURZ". A round is a combat round, or
// three steps outside a fight (D&D: 30 feet of movement per round, about three cells). The panel
// shows what's left (row 21, blinking in the last three rounds); at zero the ship hits the ground:
// game over.

// Starts the countdown; onRound (may be NULL) is called after every round with the rounds left.
void countdown_start(u8 rounds, void (*onRound)(u8 left));
void countdown_stop(void);
bool countdown_active(void);
u8 countdown_left(void);

void countdown_step(void);    // the player took a step (main.c)
void countdown_round(void);   // a combat round began (combat_setRoundHook)
void countdown_update(void);  // every frame: blinking

#endif
