#include "title.h"
#include "text.h"
#include "game.h"

void title_run(void)
{
    // The image uses PAL0 and the tiles the view takes over later (it's gone by then). Its palette
    // is loaded by hand, 16 colours: letting VDP_drawImageEx load it wrote the PNG's padded
    // palette over PAL1-PAL3 too, and the text (PAL3) turned black.
    PAL_setPalette(PAL0, title_image.palette->data, CPU);
    VDP_drawImageEx(BG_B, &title_image, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                    0, 0, FALSE, DMA);
    text_draw("EIN D&D-PROLOG", 13, 25);   // in the dark band at the bottom of the image

    u16 prevJoy = JOY_readJoypad(JOY_1);
    u16 frame = 0;
    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        if (joy & ~prevJoy & (BUTTON_START | BUTTON_A)) break;
        prevJoy = joy;
        if (frame % 60 == 0) text_draw("  Q DRÜCKEN", 13, 27);
        if (frame % 60 == 40) text_draw("             ", 13, 27);
        frame++;
        SYS_doVBlankProcess();
    }

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
}
