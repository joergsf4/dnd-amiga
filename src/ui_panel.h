#ifndef _UI_PANEL_H_
#define _UI_PANEL_H_

#include <genesis.h>
#include "dungeon_map.h"
#include "party.h"

// The right-hand panel occupies the 12 columns to the right of the 28-tile-wide dungeon view
// (28 + 12 = 40, the full screen width), all 28 rows. Every draw call in ui_panel.c stays at
// x >= UI_PANEL_COL: BG_A text composites in front of BG_B, so anything further left would cut
// into the view (rows 0-19) or the message area (rows 20-27, src/textbox.c).
#define UI_PANEL_COL 28
#define UI_PANEL_W   12

// Adds the avatar sprite of each active party member that doesn't have one yet: call after the
// party is set up and again whenever someone joins. Split out from the text chrome so a textbox
// interaction can refresh the text (HP changed, an item was found, ...) without touching
// sprites; see uiPanel_redrawChrome.
void uiPanel_initSprites(void);

// (Re)draws all the panel's text: party name/HP/MP per slot ("---LEER---" for unjoined slots),
// the item block (via uiPanel_drawInventory), and the control hints. Safe to call as often as
// needed -- call it after every textbox interaction closes, since a textbox may have covered the
// panel's status row (see textbox.h).
void uiPanel_redrawChrome(void);

// Redraws just the ITEMS block from the live `inventory` global. Also called by
// uiPanel_redrawChrome; exposed separately so a loot pickup can refresh just this part.
void uiPanel_drawInventory(void);

// Moves the avatars in a column (the end screen): the first at (x, y), one below the other.
void uiPanel_moveAvatars(s16 x, s16 y, s16 dy);

// Redraws the countdown line (row 21): "ABSTURZ: 10" while Room 6's countdown runs, else blank.
void uiPanel_drawCountdown(void);

// Redraws just the facing/coordinates line; call after each player move/turn.
void uiPanel_drawStatus(const Player *p);

#endif
