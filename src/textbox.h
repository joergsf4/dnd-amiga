#ifndef _TEXTBOX_H_
#define _TEXTBOX_H_

#include <genesis.h>

// The message area below the dungeon view: BG_A rows 20-27, columns 0-27 (the view is rows 0-19,
// the party panel columns 28-39). Text starts at column 1, so a line holds at most 27 characters.
#define TEXTBOX_ROW  20
#define TEXTBOX_H    8
#define TEXTBOX_COLS 28
#define TEXTBOX_BODY_LINES 3    // rows TEXTBOX_ROW   .. TEXTBOX_ROW+2
#define TEXTBOX_MAX_OPTIONS 4   // rows TEXTBOX_ROW+4 .. TEXTBOX_ROW+7

// Shows up to TEXTBOX_BODY_LINES lines of text, then either:
//   - optionCount == 0: a "PRESS A" prompt; blocks until A is pressed, returns 0.
//   - optionCount > 0 (up to TEXTBOX_MAX_OPTIONS): a cursor menu (D-pad + A); blocks until
//     confirmed, returns the chosen option's index.
// Clears the message area before returning either way.
u8 textbox_show(const char *const lines[], u8 lineCount,
                 const char *const options[], u8 optionCount);

// Called with the cursor index whenever a menu opens or its cursor moves (NULL = none), so a
// caller can show what the cursor points at -- combat marks the targeted enemy with it.
void textbox_setCursorHook(void (*hook)(u8 cursor));

// Clears the message area and shows up to TEXTBOX_BODY_LINES lines (NULL entries are skipped),
// without waiting. The caller decides how long they stay up.
void textbox_print(const char *const lines[], u8 lineCount);

// Shows up to TEXTBOX_BODY_LINES lines without a prompt for `frames` frames (A skips ahead), then
// clears. For running commentary that shouldn't need a button press per line (combat).
void textbox_flash(const char *const lines[], u8 lineCount, u16 frames);

#endif
