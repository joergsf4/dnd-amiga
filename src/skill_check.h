#ifndef _SKILL_CHECK_H_
#define _SKILL_CHECK_H_

#include "party.h"

typedef enum { ATTR_STR = 0, ATTR_DEX = 1, ATTR_INT = 2 } Attribute;

// d20 + actor's attribute vs. threshold, flat-additive (no D&D modifier table -- the simplest
// thing that works at this scale). Animates a short "die rolls" flourish in the textbox's
// reserved rows (see textbox.h's TEXTBOX_ROW), then blocks briefly so the result is readable.
// Takes a specific Character rather than assuming party.members[0] so a later room can let any
// party member attempt a check without a signature change.
bool skillCheck_run(Character *actor, Attribute attr, u8 threshold);

#endif
