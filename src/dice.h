#ifndef _DICE_H_
#define _DICE_H_

#include <genesis.h>

// A die roll, 1..die. SGDK's random() mixes in the VDP's line counter, so rolls made at the same
// point after a vblank wait (every roll in a fight is) came out correlated -- three W20 in a row
// showed the same number. A xorshift generator gives the sequence; random() only stirs it.
u8 dice_roll(u8 die);

#endif
