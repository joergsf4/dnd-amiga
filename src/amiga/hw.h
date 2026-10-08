/*
 * Internals shared by the Amiga layer (src/amiga/); the game only sees genesis.h.
 */
#ifndef _HW_H_
#define _HW_H_

#include <genesis.h>
#include <hardware/adkbits.h>
#include <hardware/custom.h>
#include <hardware/cia.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>

#define custom ((volatile struct Custom *) 0xdff000)
#define ciaa   ((volatile struct CIA *) 0xbfe001)

// ---------------------------------------------------------------- screen geometry

#define SCR_W       320
#define SCR_H       224
#define SCR_BPR     (SCR_W / 8)            // bytes per row and plane
#define SCR_PLANES  5
#define SCR_PLANE   (SCR_BPR * SCR_H)      // bytes per plane
#define SCR_COLS    40                     // tiles
#define SCR_ROWS    28

#define VIEW_COLS   28                     // the first-person view: top-left 28x20 tiles
#define VIEW_ROWS   20
#define VIEW_PX_W   (VIEW_COLS * 8)
#define VIEW_PX_H   (VIEW_ROWS * 8)
#define VIEW_PLANE_BPR    (VIEW_PX_W / 8)
#define VIEW_PLANE  (VIEW_PLANE_BPR * VIEW_PX_H)

// ---------------------------------------------------------------- hw.c

void *hw_allocChip(u32 size);              // cleared; fatal if out of memory
void *hw_alloc(u32 size);                  // any memory, cleared; fatal if out of memory
void hw_free(void *p);
void hw_fatal(const char *msg) __attribute__((noreturn));
void hw_waitBlit(void);
extern volatile u32 hw_vblanks;            // counts real (50 Hz) vertical blanks

// ---------------------------------------------------------------- vdp.c

void vdp_init(void);
void vdp_vblank(void);                     // from the vertical blank interrupt
void vdp_flush(void);                      // composes the back buffer, swap at the next vblank
void vdp_markRect(s16 x, s16 y, s16 w, s16 h);    // pixels: recompose in both buffers
void vdp_setView(bool active);             // BG_B's view area shows the view buffer
u8 *vdp_viewPlanes(void);                  // the view's 4 bitplanes (VIEW_PLANE bytes each)
void vdp_viewChanged(void);                // the view planes were redrawn
extern u16 vdp_colors[32];                 // Amiga colour registers
extern volatile bool vdp_colorsDirty;

// ---------------------------------------------------------------- spr.c

void spr_drawAll(u8 *screen);              // all visible sprites into a screen buffer
bool spr_anyVisible(void);

// ---------------------------------------------------------------- joy.c

void joy_init(void);
void joy_key(u8 raw);                      // from the keyboard interrupt
void joy_vblank(void);                     // from the vertical blank interrupt

// ---------------------------------------------------------------- audio.c

void audio_init(void);
void audio_shutdown(void);
void audio_vblank(void);                   // from the vertical blank interrupt: the music

// ---------------------------------------------------------------- pak (hw.c, generated resources.c)

void res_bind(u8 *const *entries);

#endif
