#include "dungeon_view.h"
#include "game.h"
#include "view_gen.h"
#include "amiga/hw.h"
#include "amiga/view_bake.h"
#include "encounter.h"
#include "input.h"
#include "automap.h"

// First-person view, Eye of the Beholder / Dungeon Master style.
//
// The player always stands in a cell centre facing a cardinal direction, so the ray through
// each 2-pixel screen column pair crosses a fixed sequence of cells relative to the player.
// tools/make_view.py (via tools/make_res.py) precomputes that sequence per column (view_gen.h);
// since a given wall face at a given screen position always looks the same, every such wall
// column is baked once per texture when the texture first comes into sight
// (src/amiga/view_bake.c -- on the Mega Drive these 394 KB were in the ROM). Rendering is then:
//   1. copy the static floor/ceiling backdrop into a chunky buffer (2 pixels per byte; mirrored
//      on alternate steps, the classic trick that makes stepping forward read as movement),
//   2. per column pair, copy the baked bytes of the first wall its ray hits (src/amiga/view_draw.s),
//   3. convert the buffer to the view's 4 bitplanes (c2p), which the display copies into the
//      back screen buffer at the next flush (src/amiga/vdp.c, double buffered).
//
// Doors, tablets and the hull breach are wall cells and render as that wall's texture. Other objects (pool, corpse, chest,
// shrine, pods) are props: they stand free in the middle of a floor cell, drawn after the walls
// as pre-scaled billboards (one image per distance, like the originals), far to near, and per
// column only where the wall behind is farther away than the prop. Some have an image per
// state (pool burst, chest opened), see objectProp.

void viewCopySpan(u8 *dst, const u8 *src, u32 rows);          // src/amiga/view_draw.s
void viewMaskSpan(u8 *dst, const u8 *src, u32 rows);          // src/amiga/view_draw.s
void c2p(const u8 *chunky, u8 *planes, const u32 *tables, u32 groups);   // src/amiga/view_draw.s

static u8 *viewBuf;                    // chunky: VIEW_BPR bytes per row, left pixel in the high nibble
static u8 *backdrops[2];               // the backdrop and its mirror image
static u8 *planarBackdrops[2];         // the same as bitplanes (chip RAM, for the blitter)
static const ViewEvent *colWall[VIEW_PAIRS];   // per column pair: the wall its ray hits first
static u8 colTex[VIEW_PAIRS];
static u32 *c2pTables;                 // 4 x 256: a byte's two pixels in all four bitplanes
#ifdef VIEW_TIMING
static u32 renderFrames;
// Time in raster lines (313 per PAL frame), for the stages of a render.
static u32 lines(void)
{
    u32 v;
    u16 pos;
    do { v = hw_vblanks; pos = ((custom->vposr & 1) << 8) | (custom->vhposr >> 8); } while (v != hw_vblanks);
    return v * 313 + pos;
}
static u32 stage[6];
#define STAGE(i) stage[i] = lines()
#else
#define STAGE(i)
#endif
static u8 cellTex[VIEW_DMAX + 1][2 * VIEW_LMAX + 1];   // 0 = open, else texture + 1
static u8 cellProp[VIEW_DMAX + 1][2 * VIEW_LMAX + 1];  // 0 = none, else prop + 1
static u8 cellSeen[VIEW_DMAX + 1][2 * VIEW_LMAX + 1];  // a ray reached it: on screen (automap)
static u8 wallH[VIEW_PAIRS];   // per column pair: rows above the horizon of the wall drawn, 0 = none
static bool mirror;
static s16 lastX = -1, lastY = -1;
static Facing lastFacing;

// Animation: textures and props with several frames (tools/make_view.py) show the one picked by
// animFrame; dungeonView_animate steps it and redraws, but only while one of them is in sight.
static u8 animFrame;
static bool animInSight;

static const u8 fireFrames[] = { PROP_FIRE, PROP_FIRE_F1, PROP_FIRE_F2 };
static const u8 shrineFrames[] = { PROP_SHRINE, PROP_SHRINE_F1, PROP_SHRINE_SQUEEZE1, PROP_SHRINE_SQUEEZE2 };
#define BREACH_FRAMES (TEX_BREACH_F3 - TEX_BREACH + 1)

static bool animated(const RoomObject *o)
{
    return o->kind == OBJ_BREACH || o->kind == OBJ_FIRE || o->kind == OBJ_RESTORATION_SHRINE
        || o->kind == OBJ_TRANSPONDER || o->kind == OBJ_TENTACLE_CONSOLE;
}

static u8 objectTexture(const RoomObject *o)
{
    switch (o->kind)
    {
        case OBJ_DOOR_EXIT: return TEX_DOOR;
        case OBJ_TABLET:    return TEX_TABLET;
        case OBJ_BREACH:    return TEX_BREACH + animFrame % BREACH_FRAMES;
        default:            return TEX_WALL;
    }
}

static u8 objectProp(const RoomObject *o)
{
    switch (o->kind)
    {
        case OBJ_LARVA_TANK:         return (o->flags & OBJFLAG_BROKEN) ? PROP_POOL_BROKEN : PROP_POOL;
        case OBJ_MINDFLAYER_CORPSE:  return PROP_CORPSE;
        case OBJ_CARTILAGE_CHEST:    return (o->flags & OBJFLAG_TRIGGERED) ? PROP_CHEST_OPEN : PROP_CHEST;
        case OBJ_RESTORATION_SHRINE:                     // param1: squeezing (dungeonObjects_useShrine)
            return shrineFrames[o->param1 ? 1 + o->param1 : animFrame % 2];
        case OBJ_POD_OPEN:           return PROP_POD_OPEN;
        case OBJ_MYRNATH:            return (o->flags & OBJFLAG_TRIGGERED) ? PROP_MYRNATH_DEAD : PROP_MYRNATH;
        case OBJ_OP_TABLE:           return PROP_OP_TABLE;
        case OBJ_LECTERN:            return PROP_LECTERN;
        case OBJ_ENEMY_GROUP:                            // how each encounter looks from afar
            switch (o->param0)
            {
                case ENC_HOUNDS:   return PROP_HOUNDS;
                case ENC_CAMBIONS: return PROP_CAMBIONS;
                case ENC_ZHALK:    return PROP_DUEL;
                default:           return PROP_IMPS;
            }
        case OBJ_TRANSPONDER:        return animFrame % 2 ? PROP_TRANSPONDER_F1 : PROP_TRANSPONDER;
        case OBJ_TENTACLE_CONSOLE:   return animFrame % 2 ? PROP_TENTACLE_CONSOLE_F1 : PROP_TENTACLE_CONSOLE;
        case OBJ_ACID_TANK:          return (o->flags & OBJFLAG_BROKEN) ? PROP_TANK_BROKEN : PROP_TANK;
        case OBJ_FIRE:               return fireFrames[animFrame % 3];
        case OBJ_POD_SEALED:
            return (o->flags & OBJFLAG_TRIGGERED) ? PROP_POD_BROKEN : (o->flags & OBJFLAG_MARKED) ? PROP_POD_DEAD : PROP_POD_SEALED;
        case OBJ_SHADOWHEART_POD:    return (o->flags & OBJFLAG_TRIGGERED) ? PROP_POD_OPEN : PROP_POD_SHADOWHEART;
        case OBJ_POD_CONSOLE:        return (o->flags & OBJFLAG_TRIGGERED) ? PROP_POD_CONSOLE_LIT : PROP_POD_CONSOLE;
        case OBJ_BUTTON_CONSOLE:     return PROP_BUTTON_CONSOLE;
        case OBJ_WOMAN_POD:
            return (o->flags & OBJFLAG_BROKEN) ? PROP_POD_DARK : (o->flags & OBJFLAG_TRIGGERED) ? PROP_POD_FLAYER : PROP_POD_WOMAN;
        case OBJ_SWITCH:             return (o->flags & OBJFLAG_TRIGGERED) ? PROP_SWITCH_USED : PROP_SWITCH;
        case OBJ_CLERIC:             return PROP_CLERIC;
        case OBJ_ORNATE_CHEST:       return (o->flags & OBJFLAG_TRIGGERED) ? PROP_ORNATE_CHEST_OPEN : PROP_ORNATE_CHEST;
        default:                     return PROP_POD_BROKEN;
    }
}

// One prop d cells ahead, l to the right. Record layout: see bakeDistance in src/amiga/view_bake.c.
static void drawProp(u8 prop, s16 d, s16 l)
{
    const u8 *src = viewBake_prop(prop, d);
    if (!src) return;
    u8 count = *src++;
    s16 pc = viewPropCenter[d][l + VIEW_LMAX] + (s8) *src++;
    for (u8 k = 0; k < count; k++, pc++)
    {
        u8 top = *src++;
        u8 rows = *src++;
        if (pc >= 0 && pc < VIEW_PAIRS && rows && wallH[pc] * d < VIEW_HALF_K)
            viewMaskSpan(viewBuf + top * VIEW_BPR + pc, src, rows);
        src += rows * 2;
    }
}

// Chunky byte j (0-3) of a group of 8 pixels -> its two pixels' bits in bitplanes 0-3, plane 0 in
// the top byte of the longword (the order c2p writes them).
static void buildC2pTables(void)
{
    c2pTables = hw_alloc(4 * 256 * sizeof(u32));
    for (u16 j = 0; j < 4; j++)
        for (u16 b = 0; b < 256; b++)
        {
            u32 v = 0;
            for (u16 k = 0; k < 4; k++)
            {
                u32 bits = (((b >> (4 + k)) & 1) << (7 - 2 * j)) | (((b >> k) & 1) << (6 - 2 * j));
                v |= bits << ((3 - k) * 8);
            }
            c2pTables[j * 256 + b] = v;
        }
}

void dungeonView_init(void)
{
    VDP_clearPlane(BG_B, TRUE);
    PAL_setPalette(PAL0, viewPalette, DMA);
    VDP_setBackgroundColor(0);
    if (!viewBuf)
    {
        viewBuf = hw_alloc(VIEW_BYTES);
        backdrops[0] = (u8 *) viewBackdrop;
        backdrops[1] = hw_alloc(VIEW_BYTES);
        for (u16 y = 0; y < VIEW_H; y++)             // mirrored: bytes reversed, nibbles swapped
            for (u16 x = 0; x < VIEW_BPR; x++)
            {
                u8 b = viewBackdrop[y * VIEW_BPR + VIEW_BPR - 1 - x];
                backdrops[1][y * VIEW_BPR + x] = (b << 4) | (b >> 4);
            }
        buildC2pTables();
        for (u8 m = 0; m < 2; m++)
        {
            planarBackdrops[m] = hw_allocChip(VIEW_PLANE * 4);
            c2p(backdrops[m], planarBackdrops[m], c2pTables, VIEW_PLANE);
        }
    }
}

// Shows rows y0..y1-1 of the chunky buffer and the backdrop around them: the static floor and
// ceiling come as finished bitplanes (blitter, in parallel), only the band with walls and props
// goes through c2p -- a third to a half of the view, unless a wall is right in front.
static void present(u16 y0, u16 y1)
{
#ifdef VIEW_TIMING
    u32 t0 = hw_vblanks;
#endif
    u8 *planes = vdp_viewPlanes();
    const u8 *bd = planarBackdrops[mirror];
    hw_waitBlit();
    for (u8 k = 0; k < 4; k++)
    {
        for (u8 part = 0; part < 2; part++)
        {
            u16 a = part ? y1 : 0, b = part ? VIEW_H : y0;
            if (a >= b) continue;
            hw_waitBlit();
            custom->bltcon0 = 0x09f0;
            custom->bltcon1 = 0;
            custom->bltafwm = 0xffff;
            custom->bltalwm = 0xffff;
            custom->bltapt = (APTR) (bd + k * VIEW_PLANE + a * VIEW_PLANE_BPR);
            custom->bltdpt = planes + k * VIEW_PLANE + a * VIEW_PLANE_BPR;
            custom->bltamod = 0;
            custom->bltdmod = 0;
            custom->bltsize = ((b - a) << 6) | (VIEW_PLANE_BPR / 2);
        }
    }
    if (y1 > y0)
        c2p(viewBuf + y0 * VIEW_BPR, planes + y0 * VIEW_PLANE_BPR, c2pTables, (y1 - y0) * VIEW_PLANE_BPR);
    hw_waitBlit();
#ifdef VIEW_TIMING
    u32 t1 = hw_vblanks;
#endif
    input_poll();
    vdp_setView(TRUE);
    vdp_viewChanged();
    SYS_doVBlankProcess();
    input_poll();
#ifdef VIEW_TIMING
    kprintf("view: draw %lu, c2p %lu (rows %u-%u), flush %lu frames (50 Hz)\n", renderFrames, t1 - t0, y0, y1, hw_vblanks - t1);
    renderFrames = 0;
#endif
}

void dungeonView_render(const Player *p)
{
    if (p->x != lastX || p->y != lastY || p->facing != lastFacing)
    {
        mirror = !mirror;
        lastX = p->x;
        lastY = p->y;
        lastFacing = p->facing;
    }

#ifdef VIEW_TIMING
    u32 t0 = hw_vblanks;
#endif
    STAGE(0);
    s16 fx, fy, rx, ry;
    animInSight = FALSE;
    viewBake_frame();
    map_forward(p->facing, &fx, &fy);
    map_right(p->facing, &rx, &ry);

    // The room's objects into the cells in view, relative to the player: one pass over them
    // instead of a search per cell (map_objectAt; the first visible object on a cell wins there too).
    RoomObject *cellObj[VIEW_DMAX + 1][2 * VIEW_LMAX + 1];
    memset(cellObj, 0, sizeof(cellObj));
    u8 count;
    RoomObject *objects = map_roomObjects(&count);
    for (u8 i = 0; i < count; i++)
    {
        RoomObject *o = &objects[i];
        if (o->kind == OBJ_NONE || (o->flags & OBJFLAG_HIDDEN)) continue;
        s16 dx = o->x - p->x, dy = o->y - p->y;
        s16 d = dx * fx + dy * fy, l = dx * rx + dy * ry;
        if (d < 0 || d > VIEW_DMAX || l < -VIEW_LMAX || l > VIEW_LMAX) continue;
        if (!cellObj[d][l + VIEW_LMAX]) cellObj[d][l + VIEW_LMAX] = o;
    }
    for (s16 d = 0; d <= VIEW_DMAX; d++)
    {
        for (s16 l = -VIEW_LMAX; l <= VIEW_LMAX; l++)
        {
            s16 cx = p->x + fx * d + rx * l;
            s16 cy = p->y + fy * d + ry * l;
            u8 t = 0, prop = 0;
            RoomObject *o = cellObj[d][l + VIEW_LMAX];
            if (o && animated(o) && d <= VIEW_DMAX && (d > 0 || l == 0)) animInSight = TRUE;
            if (map_isWall(cx, cy))
                t = 1 + (o ? objectTexture(o) : TEX_WALL);
            else if (o)
                prop = 1 + objectProp(o);
            cellTex[d][l + VIEW_LMAX] = t;
            cellProp[d][l + VIEW_LMAX] = prop;
        }
    }

    STAGE(1);
    memset(cellSeen, 0, sizeof(cellSeen));
    cellSeen[0][VIEW_LMAX] = 1;
    u8 ahead = cellTex[1][VIEW_LMAX];
    if (ahead) cellSeen[1][VIEW_LMAX] = 1;
    // First the wall each column's ray hits, and from those and the props the band of rows that
    // differs from the plain backdrop: only that band is copied, drawn and converted.
    s16 y0 = VIEW_H, y1 = 0;
    for (u16 pc = 0; pc < VIEW_PAIRS; pc++)
    {
        const ViewEvent *e = &viewEvents[viewColStart[pc]];
        const ViewEvent *end = &viewEvents[viewColStart[pc + 1]];
        colWall[pc] = NULL;
        wallH[pc] = 0;
        u8 *seen = &cellSeen[0][0];
        const u8 *tex = &cellTex[0][0];
        for (; e < end; e++)
        {
            seen[e->cell] = 1;
            u8 t = tex[e->cell];
            if (t)
            {
                colWall[pc] = e;
                colTex[pc] = t - 1;
                wallH[pc] = VIEW_CY - e->top;
                if (e->top < y0) y0 = e->top;
                if (e->top + e->rows > y1) y1 = e->top + e->rows;
                break;
            }
        }
    }
    for (s16 d = 1; !ahead && d <= VIEW_DMAX; d++)
        for (s16 l = -VIEW_LMAX; l <= VIEW_LMAX; l++)
            if (cellProp[d][l + VIEW_LMAX])
            {
                s16 half = VIEW_HALF_K / d + 1;
                if (VIEW_CY - half < y0) y0 = VIEW_CY - half;
                if (VIEW_CY + half > y1) y1 = VIEW_CY + half;
            }
#ifdef VIEW_DUMP
    y0 = 0;                                // tools/viewcheck.py compares the whole buffer
    y1 = VIEW_H;
#endif
    if (y0 < 0) y0 = 0;
    if (y1 > VIEW_H) y1 = VIEW_H;
    STAGE(2);
    if (y1 > y0)
        memcpy(viewBuf + y0 * VIEW_BPR, backdrops[mirror] + y0 * VIEW_BPR, (y1 - y0) * VIEW_BPR);
    STAGE(3);

    u8 *blocks[TEX_COUNT], *bakedBits[TEX_COUNT];
    u8 fetched = 0;                        // a bit per texture: blocks[] is valid
    for (u16 pc = 0; pc < VIEW_PAIRS; pc++)
    {
        if ((pc & 15) == 0) input_poll();   // a redraw takes frames: don't miss a short press
        const ViewEvent *e = colWall[pc];
        if (!e) continue;
        u8 t = colTex[pc];
        if (!(fetched & (1 << t)))
        {
            blocks[t] = viewBake_block(t, &bakedBits[t]);
            fetched |= 1 << t;
        }
        u8 *dst = viewBuf + e->top * VIEW_BPR + pc;
        if (!blocks[t])
        {
            viewBake_sampleSpan(dst, t, e);
            continue;
        }
        u16 i = e - viewEvents;
        if (!(bakedBits[t][i >> 3] & (1 << (i & 7)))) viewBake_event(t, e);
        viewCopySpan(dst, blocks[t] + e->bake, e->rows);
    }

    STAGE(4);
    input_poll();
    // Props: far to near, so nearer ones cover farther ones. Only |l| <= d can be on screen.
    for (s16 d = VIEW_DMAX; !ahead && d >= 1; d--)
    {
        s16 lmax = d < VIEW_LMAX ? d : VIEW_LMAX;
        for (s16 l = -lmax; l <= lmax; l++)
        {
            u8 prop = cellProp[d][l + VIEW_LMAX];
            if (prop)
            {
                drawProp(prop - 1, d, l);
                input_poll();
            }
        }
    }

    STAGE(5);
    // The automap: what the rays reached, and the cells right around the party.
    automap_beginSight();
    for (s16 d = 0; d <= VIEW_DMAX; d++)
        for (s16 l = -VIEW_LMAX; l <= VIEW_LMAX; l++)
            if (cellSeen[d][l + VIEW_LMAX])
                automap_see(p->x + fx * d + rx * l, p->y + fy * d + ry * l);
    for (s16 dy = -1; dy <= 1; dy++)
        for (s16 dx = -1; dx <= 1; dx++)
            automap_see(p->x + dx, p->y + dy);

#ifdef VIEW_TIMING
    renderFrames = hw_vblanks - t0;
    kprintf("stages (lines): cells %lu, walls? %lu, backdrop %lu, spans %lu, props %lu, automap %lu\n",
            stage[1] - stage[0], stage[2] - stage[1], stage[3] - stage[2], stage[4] - stage[3],
            stage[5] - stage[4], lines() - stage[5]);
#endif
#ifdef VIEW_DUMP
    // For tools/viewcheck.py: what was drawn and the resulting pixels, on the serial port.
    kprintf("VIEWCELLS %d", mirror);
    for (s16 d = 0; d <= VIEW_DMAX; d++)
        for (s16 l = 0; l < 2 * VIEW_LMAX + 1; l++)
            kprintf(" %d/%d", cellTex[d][l], cellProp[d][l]);
    kprintf("\n");
    for (u16 i = 0; i < VIEW_BYTES; i += 64)
    {
        char line[140];
        for (u16 j = 0; j < 64; j++)
            sprintf(line + 2 * j, "%02x", viewBuf[i + j]);
        kprintf("VIEWDATA %s\n", line);
    }
#endif
    present(y0, y1);
}

u8 *dungeonView_buffer(void)
{
    return viewBuf;
}

void dungeonView_present(void)
{
    present(0, VIEW_H);
}

bool dungeonView_animate(const Player *p)
{
    if (!animInSight) return FALSE;
    animFrame++;
    dungeonView_render(p);
    return TRUE;
}
