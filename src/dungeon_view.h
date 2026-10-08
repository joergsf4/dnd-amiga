#ifndef _DUNGEON_VIEW_H_
#define _DUNGEON_VIEW_H_

#include <genesis.h>
#include "dungeon_map.h"

// The view occupies the top-left 28x20 tiles (224x160 px) of BG_B; rows 20-27 below it are the
// message area (src/textbox.c), columns 28-39 the party panel (src/ui_panel.c).

// Sets up BG_B, the view palette (PAL0) and the buffers. Call once before the first render.
void dungeonView_init(void);

// Redraws the first-person view for the player's current position/facing. Blocks for the
// render plus one vblank (the buffer swap); call after moves/turns and interactions only.
void dungeonView_render(const Player *p);

// Steps the animations (the hull breach, fire, the restoration station, the bridge's consoles)
// and redraws -- only if one of them was in sight at the last render. Called on a timer.
bool dungeonView_animate(const Player *p);

// For other screens drawn into the view area (the automap): the chunky pixel buffer (VIEW_BPR
// bytes per row, 2 pixels per byte, left one in the high nibble), and showing it.
u8 *dungeonView_buffer(void);
void dungeonView_present(void);

#endif
