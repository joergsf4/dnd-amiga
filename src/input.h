#ifndef _INPUT_H_
#define _INPUT_H_

#include <genesis.h>

// Button presses for the dungeon loop, latched so none gets lost while the view is being redrawn
// (3-5 frames; the animations redraw every 8): SGDK only reads the pads in SYS_doVBlankProcess, so
// a press shorter than a redraw was never seen. input_poll reads the pad itself and remembers new
// presses; the renderer calls it between column batches, the dungeon loop once per frame.

void input_poll(void);
u16 input_takePresses(void);   // the buttons pressed since the last call, then forgets them

#endif
