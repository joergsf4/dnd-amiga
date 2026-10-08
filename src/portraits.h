#ifndef _PORTRAITS_H_
#define _PORTRAITS_H_

#include <genesis.h>
#include "party.h"

// Hero portraits: three per starting class (tools/make_portraits.py). Bust and avatar of one
// portrait share a palette, which goes into PAL3 (the font only uses colour 15, kept white).
#define PORTRAITS_PER_CLASS 3

const SpriteDefinition *portrait_bust(CharClass cls, u8 n);     // 80x96, creation screen
const SpriteDefinition *portrait_avatar(CharClass cls, u8 n);   // 24x24, party panel

#endif
