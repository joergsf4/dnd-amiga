#include "dice.h"

static u32 state = 0x2545F491;

u8 dice_roll(u8 die)
{
    state ^= random();
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return (state >> 8) % die + 1;
}
