#include "textbox.h"
#include "text.h"
#include "sfx.h"

static void (*cursorHook)(u8 cursor);

void textbox_setCursorHook(void (*hook)(u8 cursor))
{
    cursorHook = hook;
}

static void clearBox(void)
{
    char blank[TEXTBOX_COLS + 1];
    memset(blank, ' ', TEXTBOX_COLS);
    blank[TEXTBOX_COLS] = 0;
    for (u8 r = 0; r < TEXTBOX_H; r++)
        text_draw(blank, 0, TEXTBOX_ROW + r);
}

static void drawLines(const char *const lines[], u8 lineCount)
{
    if (lineCount > TEXTBOX_BODY_LINES) lineCount = TEXTBOX_BODY_LINES;
    for (u8 i = 0; i < lineCount; i++)
        if (lines[i]) text_draw(lines[i], 1, TEXTBOX_ROW + i);
}

static void drawOptions(const char *const options[], u8 optionCount, u8 cursor)
{
    char buf[48];   // UTF-8: umlauts take 2 bytes each
    for (u8 i = 0; i < optionCount; i++)
    {
        sprintf(buf, "%s %s", i == cursor ? ">" : " ", options[i]);
        text_draw(buf, 2, TEXTBOX_ROW + 4 + i);
    }
}

u8 textbox_show(const char *const lines[], u8 lineCount,
                 const char *const options[], u8 optionCount)
{
    clearBox();
    drawLines(lines, lineCount);

    // Seed with the CURRENT joypad state, not 0: this is invoked mid-gameplay in direct response
    // to a button press (e.g. A to interact), so the physical button may still be held down on
    // entry. Seeding with 0 would make that held button register as a fresh "pressed" edge on
    // the very first iteration below and instantly select/confirm before the player releases it.
    u16 prevJoy = JOY_readJoypad(JOY_1);

    if (optionCount == 0)
    {
        text_draw("WEITER MIT Q", 2, TEXTBOX_ROW + TEXTBOX_H - 1);
        while (TRUE)
        {
            u16 joy = JOY_readJoypad(JOY_1);
            u16 pressed = joy & ~prevJoy;
            if (pressed & BUTTON_A) break;
            prevJoy = joy;
            SPR_update();
            SYS_doVBlankProcess();
        }
        clearBox();
        return 0;
    }

    if (optionCount > TEXTBOX_MAX_OPTIONS) optionCount = TEXTBOX_MAX_OPTIONS;
    u8 cursor = 0;
    drawOptions(options, optionCount, cursor);
    if (cursorHook) cursorHook(cursor);

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        u16 pressed = joy & ~prevJoy;

        if (pressed & BUTTON_UP) cursor = (cursor + optionCount - 1) % optionCount;
        if (pressed & BUTTON_DOWN) cursor = (cursor + 1) % optionCount;
        if (pressed & (BUTTON_UP | BUTTON_DOWN))
        {
            sfx_play(SFX_MENU);
            drawOptions(options, optionCount, cursor);
            if (cursorHook) cursorHook(cursor);
        }

        if (pressed & (BUTTON_A | BUTTON_START)) break;

        prevJoy = joy;
        SPR_update();
        SYS_doVBlankProcess();
    }

    clearBox();
    return cursor;
}

void textbox_print(const char *const lines[], u8 lineCount)
{
    clearBox();
    drawLines(lines, lineCount);
}

void textbox_flash(const char *const lines[], u8 lineCount, u16 frames)
{
    textbox_print(lines, lineCount);
    u16 prevJoy = JOY_readJoypad(JOY_1);
    for (u16 i = 0; i < frames; i++)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        if (joy & ~prevJoy & BUTTON_A) break;
        prevJoy = joy;
        SPR_update();
        SYS_doVBlankProcess();
    }
    clearBox();
}
