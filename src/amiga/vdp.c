/*
 * The Mega Drive's VDP, as far as the game uses it (see genesis.h for the overall mapping).
 *
 * BG_A and BG_B are tile maps of the visible 40x28 cells. Whatever changes marks the cells it
 * touches as dirty, once per screen buffer; vdp_flush (from SYS_doVBlankProcess) composes the
 * dirty cells of the back buffer on the CPU -- BG_B's tile, or the view's pixels in the view area
 * while a view is active, then BG_A's tile over it where it's opaque -- redraws the sprites on
 * top and swaps buffers at the next vertical blank. The other buffer catches up with the same
 * cells on the next flush, so both always end up identical.
 *
 * No copper list: the vertical blank interrupt sets the bitplane pointers, the scroll value and,
 * when they changed, the 32 colour registers.
 */
#include "hw.h"

static u8 *screens[2];
static volatile u8 front;                  // the buffer on screen
static volatile bool swapPending;          // the back buffer is done: show it at the next vblank

static u16 mapA[SCR_ROWS][SCR_COLS], mapB[SCR_ROWS][SCR_COLS];
static const u8 *tilePtr[TILE_MAX];
static u8 dirty[SCR_ROWS][SCR_COLS];       // bit b: buffer b still has to compose the cell
static u8 viewDirty;                       // bit b: buffer b still has to copy the whole view
static bool viewActive;
static u8 *viewPlanes;
static s16 scrollX;

u16 vdp_colors[32];
volatile bool vdp_colorsDirty;
static u16 cram[64];                       // Mega Drive colours as the game set them

// ---------------------------------------------------------------- display

void vdp_init(void)
{
    if (!screens[0])
    {
        screens[0] = hw_allocChip(SCR_PLANE * SCR_PLANES);
        screens[1] = hw_allocChip(SCR_PLANE * SCR_PLANES);
        viewPlanes = hw_allocChip(VIEW_PLANE * 4);
    }
    // 320x224 lowres, 5 bitplanes, centred in the PAL display. Fetch starts a word early (and
    // the modulo is -2) so that BPLCON1 can scroll both ways (VDP_setHorizontalScroll).
    custom->bplcon0 = 0x5200;
    custom->bplcon1 = 0;
    custom->bplcon2 = 0;
    custom->bpl1mod = -2;
    custom->bpl2mod = -2;
    custom->diwstrt = 0x3c81;
    custom->diwstop = 0x1cc1;
    custom->ddfstrt = 0x0030;
    custom->ddfstop = 0x00d0;
    for (int i = 0; i < 8; i++)
    {
        custom->spr[i].ctl = 0;
        custom->spr[i].dataa = 0;
        custom->spr[i].datab = 0;
    }
    vdp_colorsDirty = TRUE;
    vdp_vblank();
}

void vdp_vblank(void)
{
    if (swapPending)
    {
        front ^= 1;
        swapPending = FALSE;
    }
    s16 o = scrollX;
    s32 base = (o >= 0) ? -2 : 0;
    u16 delay = (o >= 0) ? o : 16 + o;
    u8 *scr = screens[front];
    for (int k = 0; k < SCR_PLANES; k++)
        custom->bplpt[k] = (APTR) (scr + k * SCR_PLANE + base);
    custom->bplcon1 = delay | (delay << 4);
    if (vdp_colorsDirty)
    {
        vdp_colorsDirty = FALSE;
        for (int i = 0; i < 32; i++)
            custom->color[i] = vdp_colors[i];
    }
}

// ---------------------------------------------------------------- composing

static void markCell(u16 x, u16 y)
{
    dirty[y][x] = 3;
}

void vdp_markRect(s16 x, s16 y, s16 w, s16 h)
{
    s16 c0 = x >> 3, r0 = y >> 3, c1 = (x + w - 1) >> 3, r1 = (y + h - 1) >> 3;
    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 >= SCR_COLS) c1 = SCR_COLS - 1;
    if (r1 >= SCR_ROWS) r1 = SCR_ROWS - 1;
    for (s16 r = r0; r <= r1; r++)
        for (s16 c = c0; c <= c1; c++)
            dirty[r][c] = 3;
}

static inline bool inView(u16 x, u16 y)
{
    return x < VIEW_COLS && y < VIEW_ROWS;
}

static inline bool bank1(u16 attr)
{
    return ((attr >> 13) & 3) >= PAL2;
}

static void composeCell(u8 *scr, u16 x, u16 y)
{
    u8 *d = scr + y * 8 * SCR_BPR + x;
    const u8 *t;
    u16 a;

    if (viewActive && inView(x, y))
    {
        const u8 *v = viewPlanes + y * 8 * VIEW_PLANE_BPR + x;
        for (int k = 0; k < 4; k++)
            for (int r = 0; r < 8; r++)
                d[k * SCR_PLANE + r * SCR_BPR] = v[k * VIEW_PLANE + r * VIEW_PLANE_BPR];
        for (int r = 0; r < 8; r++)
            d[4 * SCR_PLANE + r * SCR_BPR] = 0;
    }
    else
    {
        a = mapB[y][x];
        t = (a & TILE_INDEX_MASK) ? tilePtr[a & TILE_INDEX_MASK] : NULL;
        if (t)
        {
            for (int k = 0; k < 4; k++)
                for (int r = 0; r < 8; r++)
                    d[k * SCR_PLANE + r * SCR_BPR] = t[k * 8 + r];
            bool b1 = bank1(a);
            for (int r = 0; r < 8; r++)
                d[4 * SCR_PLANE + r * SCR_BPR] = b1 ? t[32 + r] : 0;
        }
        else
        {
            for (int k = 0; k < SCR_PLANES; k++)
                for (int r = 0; r < 8; r++)
                    d[k * SCR_PLANE + r * SCR_BPR] = 0;
        }
    }

    a = mapA[y][x];
    t = (a & TILE_INDEX_MASK) ? tilePtr[a & TILE_INDEX_MASK] : NULL;
    if (t)
    {
        for (int r = 0; r < 8; r++)
        {
            u8 m = t[32 + r];
            if (!m) continue;
            for (int k = 0; k < 4; k++)
            {
                u8 *p = d + k * SCR_PLANE + r * SCR_BPR;
                *p = (*p & ~m) | t[k * 8 + r];
            }
            u8 *p = d + 4 * SCR_PLANE + r * SCR_BPR;
            *p = bank1(a) ? (*p | m) : (*p & ~m);
        }
    }
}

// The whole view area from the view planes, by blitter; bitplane 4 cleared (colours 0-15).
static void blitView(u8 *scr)
{
    for (int k = 0; k < SCR_PLANES; k++)
    {
        hw_waitBlit();
        custom->bltcon0 = (k < 4) ? 0x09f0 : 0x0100;   // D = A, or D = 0
        custom->bltcon1 = 0;
        custom->bltafwm = 0xffff;
        custom->bltalwm = 0xffff;
        if (k < 4) custom->bltapt = viewPlanes + k * VIEW_PLANE;
        custom->bltdpt = scr + k * SCR_PLANE;
        custom->bltamod = 0;
        custom->bltdmod = SCR_BPR - VIEW_PLANE_BPR;
        custom->bltsize = (VIEW_PX_H << 6) | (VIEW_PLANE_BPR / 2);
    }
    hw_waitBlit();
}

void vdp_flush(void)
{
    while (swapPending) ;                  // the back buffer is still on screen until the vblank
    u8 b = front ^ 1, bit = 1 << b;
    u8 *scr = screens[b];
    bool any = FALSE;

    hw_waitBlit();
    if (viewActive && (viewDirty & bit))
    {
        blitView(scr);
        viewDirty &= ~bit;
        for (int y = 0; y < VIEW_ROWS; y++)
            for (int x = 0; x < VIEW_COLS; x++)
                dirty[y][x] &= ~bit;
        any = TRUE;
    }
    for (int y = 0; y < SCR_ROWS; y++)
        for (int x = 0; x < SCR_COLS; x++)
            if (dirty[y][x] & bit)
            {
                composeCell(scr, x, y);
                dirty[y][x] &= ~bit;
                any = TRUE;
            }
    if (!any) return;
    spr_drawAll(scr);
    hw_waitBlit();
    swapPending = TRUE;
}

// ---------------------------------------------------------------- the view

void vdp_setView(bool active)
{
    if (active == viewActive) return;
    viewActive = active;
    for (int y = 0; y < VIEW_ROWS; y++)
        for (int x = 0; x < VIEW_COLS; x++)
            markCell(x, y);
    if (active) viewDirty = 3;
}

u8 *vdp_viewPlanes(void)
{
    return viewPlanes;
}

void vdp_viewChanged(void)
{
    viewDirty = 3;
}

// ---------------------------------------------------------------- SGDK: VDP_*

void VDP_setScreenWidth320(void) {}
void VDP_setScreenHeight224(void) {}
void VDP_setTextPalette(u16 pal) { (void) pal; }
void VDP_setBackgroundColor(u8 index) { (void) index; }

static void setCell(u16 plane, u16 x, u16 y, u16 attr)
{
    if (x >= SCR_COLS || y >= SCR_ROWS) return;
    u16 *m = (plane == BG_A) ? &mapA[y][x] : &mapB[y][x];
    if (*m == attr) return;
    *m = attr;
    markCell(x, y);
}

void VDP_loadFont(const TileSet *font, u16 mode)
{
    (void) mode;
    for (u16 i = 0; i < font->numTile && i < 96; i++)
        tilePtr[TILE_FONT_INDEX + i] = font->tiles + i * TILE_BYTES;
}

void VDP_drawText(const char *str, u16 x, u16 y)
{
    for (; *str; str++, x++)
    {
        u8 c = (u8) *str;
        if (c < 32 || c > 127) c = '?';
        setCell(BG_A, x, y, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_FONT_INDEX + c - 32));
    }
}

void VDP_clearPlane(u16 plane, bool wait)
{
    (void) wait;
    VDP_clearTileMapRect(plane, 0, 0, SCR_COLS, SCR_ROWS);
    if (plane == BG_B) vdp_setView(FALSE);
}

void VDP_clearTileMapRect(u16 plane, u16 x, u16 y, u16 w, u16 h)
{
    VDP_fillTileMapRect(plane, 0, x, y, w, h);
}

void VDP_fillTileMapRect(u16 plane, u16 attr, u16 x, u16 y, u16 w, u16 h)
{
    for (u16 r = y; r < y + h; r++)
        for (u16 c = x; c < x + w; c++)
            setCell(plane, c, r, attr);
}

void VDP_fillTileMapRectInc(u16 plane, u16 attr, u16 x, u16 y, u16 w, u16 h)
{
    for (u16 r = y; r < y + h; r++)
        for (u16 c = x; c < x + w; c++)
            setCell(plane, c, r, attr++);
}

bool VDP_drawImageEx(u16 plane, const Image *image, u16 attr, u16 x, u16 y, bool loadpal, u16 mode)
{
    (void) mode;
    u16 base = attr & TILE_INDEX_MASK;
    const TileSet *ts = image->tileset;
    for (u16 i = 0; i < ts->numTile && base + i < TILE_MAX; i++)
        tilePtr[base + i] = ts->tiles + i * TILE_BYTES;
    const TileMap *tm = image->tilemap;
    for (u16 r = 0; r < tm->h; r++)
        for (u16 c = 0; c < tm->w; c++)
        {
            setCell(plane, x + c, y + r, (attr & ~TILE_INDEX_MASK) | (base + tm->tilemap[r * tm->w + c]));
            markCell(x + c, y + r);        // same attributes may mean new tiles
        }
    if (loadpal) PAL_setPalette((attr >> 13) & 3, image->palette->data, CPU);
    return TRUE;
}

// Mega Drive 4bpp tiles (8 rows of 8 nibbles, left pixel in the high nibble) to the Amiga form.
void VDP_loadTileData(const u32 *data, u16 index, u16 num, u16 mode)
{
    (void) mode;
    u8 *tiles = hw_alloc((u32) num * TILE_BYTES);
    for (u16 i = 0; i < num && index + i < TILE_MAX; i++)
    {
        u8 *t = tiles + i * TILE_BYTES;
        for (int r = 0; r < 8; r++)
        {
            u32 row = data[i * 8 + r];
            for (int px = 0; px < 8; px++)
            {
                u8 c = (row >> (28 - px * 4)) & 15;
                u8 bit = 0x80 >> px;
                for (int k = 0; k < 4; k++)
                    if (c & (1 << k)) t[k * 8 + r] |= bit;
                if (c) t[32 + r] |= bit;
            }
        }
        tilePtr[index + i] = t;
    }
}

void VDP_setHorizontalScroll(u16 plane, s16 value)
{
    (void) plane;                          // the whole screen shakes: BPLCON1 applies per line
    if (value > 15) value = 15;
    if (value < -15) value = -15;
    scrollX = value;
}

// ---------------------------------------------------------------- SGDK: PAL_*

// 0x0BGR with 3 bits per component (bits 1-3) -> 0x0RGB with 4.
static u16 toAmiga(u16 md)
{
    u16 r = (md >> 1) & 7, g = (md >> 5) & 7, b = (md >> 9) & 7;
    r = r * 2 + (r >> 2);
    g = g * 2 + (g >> 2);
    b = b * 2 + (b >> 2);
    return (r << 8) | (g << 4) | b;
}

void PAL_setColor(u16 index, u16 value)
{
    if (index >= 64) return;
    cram[index] = value;
    u16 pal = index >> 4, i = index & 15;
    if (pal == PAL1) return;               // no bank of its own (see genesis.h)
    vdp_colors[(pal == PAL0 ? 0 : 16) + i] = toAmiga(value);
    vdp_colorsDirty = TRUE;
}

void PAL_setColors(u16 index, const u16 *pal, u16 count, u16 mode)
{
    (void) mode;
    for (u16 i = 0; i < count; i++)
        PAL_setColor(index + i, pal[i]);
}

void PAL_setPalette(u16 numPal, const u16 *pal, u16 mode)
{
    PAL_setColors(numPal * 16, pal, 16, mode);
}

void PAL_getColors(u16 index, u16 *dest, u16 count)
{
    for (u16 i = 0; i < count; i++)
        dest[i] = cram[(index + i) & 63];
}
