#include "input.h"

static u16 latched, last;

void input_poll(void)
{
    JOY_update();
    u16 state = JOY_readJoypad(JOY_1);
    latched |= state & ~last;
    last = state;
}

u16 input_takePresses(void)
{
    input_poll();
    u16 pressed = latched;
    latched = 0;
    return pressed;
}
