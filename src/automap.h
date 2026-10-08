#ifndef _AUTOMAP_H_
#define _AUTOMAP_H_

#include <genesis.h>
#include "dungeon_map.h"

// The automap: every room starts dark and uncovers itself as the party explores it. The renderer
// reports each cell a view ray crosses up to (and including) the first wall it hits -- exactly what
// is on screen -- plus the 8 cells around the party (automap_see). C in the dungeon opens the map
// screen (automap_screen): every room as far as it is known, the party as an arrow, objects as marks,
// enemies only while they're in sight.

#define AUTOMAP_MAX_W 16   // a room may be at most 16 x 16 cells, walls included
#define AUTOMAP_MAX_H 16

// A new view is being drawn: forget what was in sight (enemies are only shown while they are).
void automap_beginSight(void);

// The cell (x, y) of the current room is seen: uncovered for good, and in sight for now.
void automap_see(s16 x, s16 y);

// The map screen over the view and the message area: the whole dungeon as far as it's been seen,
// centred on the party; up/down scrolls, B or C closes. The caller redraws the view afterwards.
void automap_screen(const Player *p);

#endif
