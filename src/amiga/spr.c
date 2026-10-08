/*
 * SGDK's sprite engine as blitter objects. A sprite is only a position, a frame and a
 * visibility; every change marks the cells under its old and new rectangle dirty (vdp.c), and
 * the flush recomposes them and draws all visible sprites on top, cookie-cut through their
 * mask. As on the Mega Drive, the sprite added first is on top.
 *
 * Colours: a sprite on PAL2/PAL3 uses colours 16-31 (bitplane 4 set where it's opaque), one on
 * PAL0/PAL1 colours 0-15 (the avatars, converted to the view palette by tools/make_res.py).
 */
#include "hw.h"

#define SPR_MAX 24

struct Sprite
{
    const SpriteDefinition *def;
    s16 x, y;
    u8 used, visible, bank1, frame;
    u16 timer;
    Sprite *next;                          // in the order they were added: first = on top
};

static Sprite pool[SPR_MAX];
static Sprite *first;

void SPR_initEx(u16 vramSize)
{
    (void) vramSize;
    memset(pool, 0, sizeof(pool));
    first = NULL;
}

static void mark(Sprite *s)
{
    if (s->visible) vdp_markRect(s->x, s->y, s->def->w, s->def->h);
}

Sprite *SPR_addSpriteEx(const SpriteDefinition *def, s16 x, s16 y, u16 attribut, u16 flag)
{
    (void) flag;
    Sprite *s = NULL;
    for (int i = 0; i < SPR_MAX; i++)
        if (!pool[i].used)
        {
            s = &pool[i];
            break;
        }
    if (!s) hw_fatal("too many sprites");
    memset(s, 0, sizeof(*s));
    s->def = def;
    s->x = x;
    s->y = y;
    s->used = TRUE;
    s->visible = TRUE;
    s->bank1 = ((attribut >> 13) & 3) >= PAL2;
    s->timer = def->animations[0].timer;
    Sprite **l = &first;
    while (*l) l = &(*l)->next;
    *l = s;
    mark(s);
    return s;
}

void SPR_releaseSprite(Sprite *sprite)
{
    if (!sprite || !sprite->used) return;
    mark(sprite);
    for (Sprite **l = &first; *l; l = &(*l)->next)
        if (*l == sprite)
        {
            *l = sprite->next;
            break;
        }
    sprite->used = FALSE;
}

void SPR_setPosition(Sprite *sprite, s16 x, s16 y)
{
    if (sprite->x == x && sprite->y == y) return;
    mark(sprite);
    sprite->x = x;
    sprite->y = y;
    mark(sprite);
}

void SPR_setVisibility(Sprite *sprite, u16 value)
{
    bool v = (value != HIDDEN);
    if (sprite->visible == v) return;
    sprite->visible = TRUE;                // marks the rectangle either way
    mark(sprite);
    sprite->visible = v;
}

void SPR_setFrame(Sprite *sprite, s16 frame)
{
    if (sprite->frame == frame || frame >= (s16) sprite->def->animations[0].numFrame) return;
    sprite->frame = frame;
    sprite->timer = sprite->def->animations[0].timer;
    mark(sprite);
}

// Steps the automatic animations (a frame time in the sprite definition, like the imps' wings).
void SPR_update(void)
{
    for (Sprite *s = first; s; s = s->next)
    {
        const Animation *a = &s->def->animations[0];
        if (!a->timer || a->numFrame < 2) continue;
        if (--s->timer == 0)
        {
            s->timer = a->timer;
            s->frame = (s->frame + 1) % a->numFrame;
            mark(s);
        }
    }
}

bool spr_anyVisible(void)
{
    for (Sprite *s = first; s; s = s->next)
        if (s->visible) return TRUE;
    return FALSE;
}

// Cookie-cut blit of one frame at (x, y): D = mask ? bob : screen for bitplanes 0-3, and
// bitplane 4 set (colours 16-31) or cleared (colours 0-15) under the mask. One word wider than
// the frame for the shift, with the source modulo at -2 and the mask's last word masked off.
static void blitBob(u8 *scr, const BobFrame *f, s16 x, s16 y, bool b1)
{
    s16 h = f->h, skip = 0;
    if (y < 0) { skip = -y; h += y; y = 0; }
    if (y + h > SCR_H) h = SCR_H - y;
    if (h <= 0 || x < 0 || x >= SCR_W) return;

    u16 shift = x & 15;
    u16 words = f->wWords + 1;
    u32 rowBytes = f->wWords * 2;
    u32 planeBytes = rowBytes * f->h;
    const u8 *mask = f->data + 4 * planeBytes + skip * rowBytes;
    u8 *dst = scr + y * SCR_BPR + (x >> 4) * 2;

    for (int k = 0; k < SCR_PLANES; k++)
    {
        hw_waitBlit();
        if (k < 4)
        {
            custom->bltcon0 = (shift << 12) | 0x0fca;
            custom->bltbpt = (APTR) (f->data + k * planeBytes + skip * rowBytes);
            custom->bltbmod = -2;
        }
        else
            custom->bltcon0 = (shift << 12) | 0x0b00 | (b1 ? 0xfa : 0x0a);
        custom->bltcon1 = shift << 12;
        custom->bltafwm = 0xffff;
        custom->bltalwm = 0x0000;
        custom->bltapt = (APTR) mask;
        custom->bltcpt = dst + k * SCR_PLANE;
        custom->bltdpt = dst + k * SCR_PLANE;
        custom->bltamod = -2;
        custom->bltcmod = SCR_BPR - words * 2;
        custom->bltdmod = SCR_BPR - words * 2;
        custom->bltsize = (h << 6) | words;
    }
}

// Last added first, so that the first added ends up on top.
static void drawFrom(Sprite *s, u8 *scr)
{
    if (!s) return;
    drawFrom(s->next, scr);
    if (s->visible)
        blitBob(scr, &s->def->animations[0].frames[s->frame], s->x, s->y, s->bank1);
}

void spr_drawAll(u8 *scr)
{
    drawFrom(first, scr);
}
