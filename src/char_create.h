#ifndef _CHAR_CREATE_H_
#define _CHAR_CREATE_H_

#include "party.h"

// Full-screen hero creation (uses the whole screen -- runs before the dungeon view/panel are set
// up): class with up/down, portrait with left/right. Blocks until the player confirms, then
// returns the chosen class and portrait and leaves BG_A cleared behind it.
CharClass charCreate_run(u8 *portrait);

#endif
