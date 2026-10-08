/*
 * The first-person renderer's inner loops (called from src/dungeon_view.c), from the Mega Drive
 * version's view_draw.s. The buffer is plain chunky now instead of VDP tiles: 112 bytes per row,
 * one byte per 2-pixel column pair, so a column steps down by 112 bytes.
 *
 * void viewCopySpan(u8 *dst, const u8 *src, u32 rows)
 *     copies `rows` baked bytes (a wall column, src/amiga/view_bake.c) down the column at dst
 * void viewMaskSpan(u8 *dst, const u8 *src, u32 rows)
 *     the same for a prop: src holds (mask, data) byte pairs, each byte becomes (dst & mask) | data
 * void c2p(const u8 *chunky, u8 *planes, const u32 *tables, u32 groups)
 *     `groups` groups of 8 pixels (4 chunky bytes) -> the view's 4 bitplanes (VIEW_PLANE = 4480
 *     bytes apart; planes points into bitplane 0)
 *
 * C calling convention: arguments 32-bit on the stack, d0-d1/a0-a1 scratch.
 */

    .text

    .globl  _viewCopySpan
_viewCopySpan:
    move.l  4(%sp),%a0
    move.l  8(%sp),%a1
    move.l  12(%sp),%d1
1:
    cmp.w   #8,%d1
    blt.s   2f
    move.b  (%a1)+,(%a0)
    move.b  (%a1)+,112(%a0)
    move.b  (%a1)+,224(%a0)
    move.b  (%a1)+,336(%a0)
    move.b  (%a1)+,448(%a0)
    move.b  (%a1)+,560(%a0)
    move.b  (%a1)+,672(%a0)
    move.b  (%a1)+,784(%a0)
    lea     896(%a0),%a0
    subq.w  #8,%d1
    bra.s   1b
2:
    subq.w  #1,%d1
    bmi.s   4f
3:
    move.b  (%a1)+,(%a0)
    lea     112(%a0),%a0
    dbra    %d1,3b
4:
    rts

    .globl  _viewMaskSpan
_viewMaskSpan:
    move.l  %d2,-(%sp)
    move.l  8(%sp),%a0
    move.l  12(%sp),%a1
    move.l  16(%sp),%d1
    subq.w  #1,%d1
    bmi.s   2f
1:
    move.b  (%a1)+,%d2
    and.b   %d2,(%a0)
    move.b  (%a1)+,%d2
    or.b    %d2,(%a0)
    lea     112(%a0),%a0
    dbra    %d1,1b
2:
    move.l  (%sp)+,%d2
    rts

/*
 * Chunky to planar, 8 pixels (4 chunky bytes) at a time: each byte looks up a longword in the
 * table for its position (4 tables of 256, built by dungeon_view.c) holding its two pixels' bits
 * for all four planes -- plane 0 in the top byte, plane 3 in the bottom one -- and the four are
 * ORed together. ~32 cycles per pixel.
 */
    .globl  _c2p
_c2p:
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    move.l  48(%sp),%a0             /* chunky */
    move.l  52(%sp),%a1             /* bitplane 0 */
    move.l  56(%sp),%a2             /* tables */
    lea     1024(%a2),%a3
    lea     2048(%a2),%a4
    lea     3072(%a2),%a5
    move.l  60(%sp),%d7             /* groups */
    subq.w  #1,%d7
1:
    moveq   #0,%d1
    move.b  (%a0)+,%d1
    add.w   %d1,%d1
    add.w   %d1,%d1
    move.l  0(%a2,%d1.w),%d0
    moveq   #0,%d1
    move.b  (%a0)+,%d1
    add.w   %d1,%d1
    add.w   %d1,%d1
    or.l    0(%a3,%d1.w),%d0
    moveq   #0,%d1
    move.b  (%a0)+,%d1
    add.w   %d1,%d1
    add.w   %d1,%d1
    or.l    0(%a4,%d1.w),%d0
    moveq   #0,%d1
    move.b  (%a0)+,%d1
    add.w   %d1,%d1
    add.w   %d1,%d1
    or.l    0(%a5,%d1.w),%d0
    move.b  %d0,13440(%a1)          /* plane 3 */
    lsr.w   #8,%d0
    move.b  %d0,8960(%a1)           /* plane 2 */
    swap    %d0
    move.b  %d0,4480(%a1)           /* plane 1 */
    lsr.w   #8,%d0
    move.b  %d0,(%a1)+              /* plane 0 */
    dbra    %d7,1b
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts
