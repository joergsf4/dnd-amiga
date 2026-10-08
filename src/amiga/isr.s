/*
 * Interrupt entry points for the level 2 (CIA-A: keyboard) and level 3 (vertical blank)
 * autovectors, installed by src/amiga/hw.c while the game owns the machine. They save the
 * registers the C calling convention may clobber, acknowledge the request and call C.
 */

    .text

    .globl  _hw_level3
_hw_level3:
    movem.l %d0-%d1/%a0-%a1,-(%sp)
    move.w  0xdff01e,%d0            /* INTREQR */
    and.w   #0x0070,%d0             /* COPER, VERTB, BLIT: acknowledge all three */
    move.w  %d0,0xdff09c
    move.w  %d0,0xdff09c            /* twice: fast CPUs can outrun the chipset */
    btst    #5,%d0
    beq.s   1f
    jsr     _hw_vblankIsr
1:
    movem.l (%sp)+,%d0-%d1/%a0-%a1
    rte

    .globl  _hw_level2
_hw_level2:
    movem.l %d0-%d1/%a0-%a1,-(%sp)
    move.w  0xdff01e,%d0
    btst    #3,%d0                  /* PORTS */
    beq.s   2f
    move.b  0xbfed01,%d0            /* CIA-A ICR; reading it clears it */
    btst    #3,%d0                  /* serial port: a key */
    beq.s   1f
    moveq   #0,%d0
    move.b  0xbfec01,%d0            /* SDR */
    move.l  %d0,-(%sp)
    jsr     _hw_keyIsr
    addq.l  #4,%sp
1:
    move.w  #0x0008,0xdff09c
    move.w  #0x0008,0xdff09c
2:
    movem.l (%sp)+,%d0-%d1/%a0-%a1
    rte

/* ULONG hw_getVBR(void), run through exec's Supervisor() on a 68010 or later */
    .globl  _hw_getVBR
_hw_getVBR:
    .word   0x4e7a,0x0801           /* movec vbr,d0 */
    rte

/*
 * Reboots the machine: the reset code from the Amiga Hardware Reference Manual. RESET resets the
 * chips and maps the Kickstart ROM to address 0; the prefetched jmp then runs the ROM's cold
 * start, which boots the disk again. Longword aligned, run through Supervisor().
 */
    .balign 4
    .globl  _hw_coldReboot
_hw_coldReboot:
    lea     2,%a0
    reset
    jmp     (%a0)
