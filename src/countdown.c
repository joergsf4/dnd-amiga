#include "countdown.h"
#include "abilities.h"
#include "ui_panel.h"
#include "text.h"

#define STEPS_PER_ROUND 3

static bool active;
static u8 left, steps, blink;
static void (*roundCallback)(u8 left);

void countdown_start(u8 rounds, void (*onRound)(u8 left))
{
    active = TRUE;
    left = rounds;
    steps = 0;
    roundCallback = onRound;
    uiPanel_drawCountdown();
}

void countdown_stop(void)
{
    active = FALSE;
    uiPanel_drawCountdown();
}

bool countdown_active(void)
{
    return active;
}

u8 countdown_left(void)
{
    return left;
}

void countdown_round(void)
{
    if (!active) return;
    if (left) left--;
    uiPanel_drawCountdown();
    if (roundCallback) roundCallback(left);
    if (!left)
    {
        active = FALSE;
        ab_gameOver("Zu spät! Der Nautiloid", "schlägt auf und zerschellt", "in einem Feuerball.");
    }
}

void countdown_step(void)
{
    if (!active) return;
    if (++steps < STEPS_PER_ROUND) return;
    steps = 0;
    countdown_round();
}

void countdown_update(void)
{
    if (!active || left > 3) return;
    if (++blink == 20) uiPanel_drawCountdown();
    if (blink == 40)
    {
        text_draw("            ", UI_PANEL_COL, 21);
        blink = 0;
    }
}
