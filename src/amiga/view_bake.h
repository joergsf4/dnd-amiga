#ifndef _VIEW_BAKE_H_
#define _VIEW_BAKE_H_

#include <genesis.h>
#include "view_gen.h"

// Wall columns and scaled props, baked on first use (src/amiga/view_bake.c).

void viewBake_frame(void);                 // a new render starts: what it uses stays cached

// Texture t's block of baked wall columns (at ViewEvent.bake) and, in *baked, a bit per event
// (index e - viewEvents) that's set once that column is in it -- viewBake_event bakes it. NULL:
// the texture isn't baked (the hull breach, or no memory); draw it with viewBake_sampleSpan.
u8 *viewBake_block(u8 t, u8 **baked);
void viewBake_event(u8 t, const ViewEvent *e);
void viewBake_sampleSpan(u8 *dst, u8 t, const ViewEvent *e);

// A prop's record for distance d (1..VIEW_DMAX); NULL if there's no memory left for it.
const u8 *viewBake_prop(u8 prop, u8 d);

#endif
