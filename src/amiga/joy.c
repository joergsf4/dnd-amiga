/*
 * SGDK's joypad, from a joystick in port 2 and the keyboard:
 *
 *   Mega Drive   joystick                      keyboard
 *   D-pad        directions                    cursor keys, W A S D
 *   A (use)      fire                          Q
 *   B (party)    second fire button (pin 9)    E
 *   C (map)      -                             M
 *   Start        fire                          Q
 *
 * One key and the fire button are both A and Start: every place that waits for Start also takes
 * A, except the end screen, which a one-button joystick couldn't leave otherwise.
 */
#include "hw.h"

static u8 keys[128];
static volatile u8 tapped[128];            // went down since the last read: reported once even if
                                           // it's already up again (a press during a long redraw)

void joy_init(void)
{
    memset(keys, 0, sizeof(keys));
    custom->potgo = 0xff00;                    // the fire buttons' pot pins as inputs, pulled up
}

// The keyboard sends each key code rotated left by one and inverted; bit 7 set: released.
void joy_key(u8 sdr)
{
    u8 code = (u8) ~sdr;
    code = (code >> 1) | (code << 7);
    u8 k = code & 0x7f;
    keys[k] = !(code & 0x80);
    if (keys[k]) tapped[k] = TRUE;
#ifdef KEY_TRACE
    kprintf("key %02x %s\n", code & 0x7f, (code & 0x80) ? "up" : "down");
#endif
}

// The joystick has no interrupt: it's read every vertical blank, and a press that comes and goes
// between two game reads is kept like a key tap.
static u16 joyState(void)
{
    u16 state = 0;
    u16 dat = custom->joy1dat;
    if (dat & 0x0002) state |= BUTTON_RIGHT;
    if (dat & 0x0200) state |= BUTTON_LEFT;
    if (((dat >> 1) ^ dat) & 0x0001) state |= BUTTON_DOWN;
    if (((dat >> 1) ^ dat) & 0x0100) state |= BUTTON_UP;
    if (!(ciaa->ciapra & CIAF_GAMEPORT1)) state |= BUTTON_A | BUTTON_START;
    if (!(custom->potinp & 0x4000)) state |= BUTTON_B;
    return state;
}

static volatile u16 joyTapped;

void joy_vblank(void)
{
    joyTapped |= joyState();
}

void JOY_update(void)
{
}

static inline bool key(u8 k)
{
    bool down = keys[k] || tapped[k];
    tapped[k] = FALSE;
    return down;
}

#define KEY(k) key(k)

u16 JOY_readJoypad(u16 joy)
{
    (void) joy;
    u16 state = joyState() | joyTapped;
    joyTapped = 0;

    if (KEY(0x4c) || KEY(0x11)) state |= BUTTON_UP;                    // cursor up, W
    if (KEY(0x4d) || KEY(0x21)) state |= BUTTON_DOWN;                  // cursor down, S
    if (KEY(0x4f) || KEY(0x20)) state |= BUTTON_LEFT;                  // cursor left, A
    if (KEY(0x4e) || KEY(0x22)) state |= BUTTON_RIGHT;                 // cursor right, D
    if (KEY(0x10)) state |= BUTTON_A | BUTTON_START;                   // Q
    if (KEY(0x12)) state |= BUTTON_B;                                  // E
    if (KEY(0x37)) state |= BUTTON_C;                                  // M
    return state;
}
