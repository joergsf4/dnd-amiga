#include "portraits.h"
#include "game.h"

static const SpriteDefinition *const busts[3][PORTRAITS_PER_CLASS] = {
    { &portrait_fighter_1, &portrait_fighter_2, &portrait_fighter_3 },
    { &portrait_rogue_1,   &portrait_rogue_2,   &portrait_rogue_3 },
    { &portrait_mage_1,    &portrait_mage_2,    &portrait_mage_3 },
};

static const SpriteDefinition *const avatars[3][PORTRAITS_PER_CLASS] = {
    { &hero_fighter_1, &hero_fighter_2, &hero_fighter_3 },
    { &hero_rogue_1,   &hero_rogue_2,   &hero_rogue_3 },
    { &hero_mage_1,    &hero_mage_2,    &hero_mage_3 },
};

const SpriteDefinition *portrait_bust(CharClass cls, u8 n)
{
    return busts[cls][n];
}

const SpriteDefinition *portrait_avatar(CharClass cls, u8 n)
{
    return avatars[cls][n];
}
