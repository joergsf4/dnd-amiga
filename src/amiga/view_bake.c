/*
 * Baking the view on the Amiga. On the Mega Drive, tools/make_view.py pre-rendered every wall
 * column the view can show (394 KB) and every prop at every distance (344 KB) into the ROM. Here
 * only the sources are on the disk -- the 64x64 textures and props -- and this bakes the same
 * bytes in memory as they first come into sight: a wall column when a ray first shows it with
 * that texture (bake() in make_view.py; a texture's block has room for all 866 of them, 56 KB), a
 * prop's record for a distance when it's first seen there (bake_prop()), from the sampling tables
 * in view_gen.h. Baking only what's in sight keeps entering a room quick.
 *
 * Baked blocks live in a cache with a memory budget; when it's full, what was used longest ago
 * goes first. Animated walls (the hull breach, 4 frames) aren't baked at all: they would take
 * 225 KB, so viewBake_sampleSpan samples the texture per row while drawing instead.
 */
#include <exec/memory.h>
#include <proto/exec.h>
#include "hw.h"
#include "view_bake.h"
#include "res/resources.h"

#define CACHE_BUDGET (300 * 1024UL)

typedef struct
{
    u8 *data;
    u32 size;
    u32 used;                              // frame stamp of the last use
} Cached;

#define EVENTS 866                         // == viewColStart[VIEW_PAIRS], checked in viewBake_column
static Cached textures[TEX_COUNT], props[PROP_COUNT][VIEW_DMAX + 1];
static u8 texBaked[TEX_COUNT][(EVENTS + 7) / 8];
static u32 cacheBytes, frame;

void viewBake_frame(void)
{
    frame++;
}

static void drop(Cached *c)
{
    hw_free(c->data);
    cacheBytes -= c->size;
    c->data = NULL;
    c->size = 0;
}

// Makes room for `size` more bytes, dropping the least recently used blocks -- never one used in
// this frame. FALSE if that isn't enough.
static bool makeRoom(u32 size)
{
    while (cacheBytes + size > CACHE_BUDGET || AvailMem(MEMF_LARGEST) < size + 16 * 1024)
    {
        Cached *oldest = NULL;
        for (int i = 0; i < TEX_COUNT; i++)
            if (textures[i].data && textures[i].used != frame && (!oldest || textures[i].used < oldest->used))
                oldest = &textures[i];
        for (int i = 0; i < PROP_COUNT; i++)
            for (int d = 1; d <= VIEW_DMAX; d++)
                if (props[i][d].data && props[i][d].used != frame && (!oldest || props[i][d].used < oldest->used))
                    oldest = &props[i][d];
        if (!oldest) return FALSE;
        drop(oldest);
    }
    return TRUE;
}

// ---------------------------------------------------------------- walls

// One event's rows, sampled straight from the packed texture, down the chunky buffer: for walls
// that aren't baked (the hull breach's frames).
void viewBake_sampleSpan(u8 *dst, u8 t, const ViewEvent *e)
{
    const u8 *tex = viewTextures + (u32) t * (VIEW_TEX * VIEW_TEX / 2);
    const u8 *shade = viewShade[e->band];
    const u8 *c0 = tex + (e->u0 >> 1), *c1 = tex + (e->u1 >> 1);
    u8 s0 = (e->u0 & 1) ? 0 : 4, s1 = (e->u1 & 1) ? 0 : 4;
    u16 v = e->vstart;
    for (u16 r = e->rows; r; r--)
    {
        u16 row = (v >> 3) & 0x07e0;       // (v >> 8) * 32
        *dst = (shade[(c0[row] >> s0) & 15] << 4) | shade[(c1[row] >> s1) & 15];
        dst += VIEW_BPR;
        v += e->vstep;
    }
}

// One event's bytes baked into the texture's block: the same sampling, one byte after the other.
static void bakeEvent(u8 *dst, const u8 *tex, const ViewEvent *e)
{
    const u8 *shade = viewShade[e->band];
    const u8 *c0 = tex + (e->u0 >> 1), *c1 = tex + (e->u1 >> 1);
    u8 s0 = (e->u0 & 1) ? 0 : 4, s1 = (e->u1 & 1) ? 0 : 4;
    u16 v = e->vstart, step = e->vstep;
    for (u16 r = e->rows; r; r--)
    {
        u16 row = (v >> 3) & 0x07e0;
        *dst++ = (shade[(c0[row] >> s0) & 15] << 4) | shade[(c1[row] >> s1) & 15];
        v += step;
    }
}

u8 *viewBake_block(u8 t, u8 **baked)
{
    Cached *c = &textures[t];
    if (!c->data)
    {
        if (t >= TEX_BREACH || viewColStart[VIEW_PAIRS] != EVENTS || !makeRoom(VIEW_TEX_ROWS)) return NULL;
        c->data = hw_alloc(VIEW_TEX_ROWS);
        c->size = VIEW_TEX_ROWS;
        cacheBytes += c->size;
        memset(texBaked[t], 0, sizeof(texBaked[t]));
#ifdef VIEW_TIMING
        kprintf("texture %d: block, cache %lu\n", t, cacheBytes);
#endif
    }
    c->used = frame;
    *baked = texBaked[t];
    return c->data;
}

void viewBake_event(u8 t, const ViewEvent *e)
{
    u16 i = e - viewEvents;
    bakeEvent(textures[t].data + e->bake, viewTextures + (u32) t * (VIEW_TEX * VIEW_TEX / 2), e);
    texBaked[t][i >> 3] |= 1 << (i & 7);
}

// ---------------------------------------------------------------- props

#define T 16                               // a transparent texel
static u8 *texels;                         // the prop being baked, a byte per texel
static u8 vTop[VIEW_TEX], vBot[VIEW_TEX];  // per texel column: its opaque rows (vTop >= vBot: none)
static u8 yFor[VIEW_TEX + 1];              // per texel row: the first view row sampling it or a later one

// The source: per texel row, [run count], then per run [first column, length, nibbles].
static void unpackProp(const u8 *src)
{
    memset(texels, T, VIEW_TEX * VIEW_TEX);
    for (u16 v = 0; v < VIEW_TEX; v++)
    {
        u8 *row = texels + v * VIEW_TEX;
        for (u8 runs = *src++; runs; runs--)
        {
            u8 x = *src++, n = *src++;
            for (u8 i = 0; i < n; i++)
                row[x + i] = (i & 1) ? (src[i >> 1] & 15) : (src[i >> 1] >> 4);
            src += (n + 1) >> 1;
        }
    }
    for (u16 u = 0; u < VIEW_TEX; u++)
    {
        vTop[u] = VIEW_TEX;
        vBot[u] = 0;
        for (u16 v = 0; v < VIEW_TEX; v++)
            if (texels[v * VIEW_TEX + u] != T)
            {
                if (vTop[u] == VIEW_TEX) vTop[u] = v;
                vBot[u] = v + 1;
            }
    }
}

static inline u8 texelAt(u8 u, u8 v)
{
    return u == 255 ? T : texels[v * VIEW_TEX + u];
}

// Per distance: [pair count, left pair offset (s8)], then per pair [top, rows, (mask, data) per
// row] -- the Mega Drive's record layout (bake_prop in tools/make_view.py). A pair's rows are
// found from its texel columns' opaque ranges, then trimmed to the first and last row that
// actually samples an opaque texel. With out == NULL it only counts the bytes.
#define EMIT(x) do { if (out) out[n] = (x); n++; } while (0)
static u32 bakeDistance(u8 *out, u8 d)
{
    const ViewPropGeo *g = &viewPropGeo[d - 1];
    const u8 *shade = viewShade[g->band];
    const u8 *vy = g->v;
    s16 ncols = -2 * g->k0 + 1;
    s16 first = -1, last = -1;
    u8 top[64], bot[64];

    u8 y = 0;                              // yFor: v(y) never decreases (255 above and below)
    for (u16 v = 0; v <= VIEW_TEX; v++)
    {
        while (y < VIEW_H && (vy[y] == 255 ? y < VIEW_H / 2 : vy[y] < v)) y++;
        yFor[v] = y;
    }

    for (s16 k = 0; k < ncols; k++)
    {
        u8 ua = g->u[2 * k], ub = g->u[2 * k + 1];
        u8 v0 = VIEW_TEX, v1 = 0;
        if (ua != 255) { v0 = vTop[ua]; v1 = vBot[ua]; }
        if (ub != 255) { if (vTop[ub] < v0) v0 = vTop[ub]; if (vBot[ub] > v1) v1 = vBot[ub]; }
        top[k] = bot[k] = 0;
        if (v0 >= v1) continue;
        s16 y0 = yFor[v0], y1 = yFor[v1];
        while (y0 < y1 && texelAt(ua, vy[y0]) == T && texelAt(ub, vy[y0]) == T) y0++;
        while (y1 > y0 && texelAt(ua, vy[y1 - 1]) == T && texelAt(ub, vy[y1 - 1]) == T) y1--;
        if (y0 >= y1) continue;
        top[k] = y0;
        bot[k] = y1;
        if (first < 0) first = k;
        last = k;
    }

    u32 n = 0;
    if (first < 0)
    {
        EMIT(0);
        EMIT(0);
        return n;
    }
    EMIT(last - first + 1);
    EMIT((u8) (s8) (g->k0 + first));
    for (s16 k = first; k <= last; k++)
    {
        u8 ua = g->u[2 * k], ub = g->u[2 * k + 1];
        EMIT(top[k]);
        EMIT(bot[k] - top[k]);
        if (!out)
        {
            n += 2 * (bot[k] - top[k]);
            continue;
        }
        for (u8 yy = top[k]; yy < bot[k]; yy++)
        {
            u8 a = texelAt(ua, vy[yy]), b = texelAt(ub, vy[yy]);
            EMIT((a == T ? 0xf0 : 0) | (b == T ? 0x0f : 0));
            EMIT((a == T ? 0 : shade[a] << 4) | (b == T ? 0 : shade[b]));
        }
    }
    return n;
}

const u8 *viewBake_prop(u8 prop, u8 d)
{
    Cached *c = &props[prop][d];
    if (!c->data)
    {
        u32 t0 = hw_vblanks;
        if (!texels) texels = hw_alloc(VIEW_TEX * VIEW_TEX);
        unpackProp(viewPropSources + viewPropSourceOffset[prop]);
        u32 size = bakeDistance(NULL, d);  // first the size, then the record into a block of it
        if (!makeRoom(size)) return NULL;
        c->data = hw_alloc(size);
        bakeDistance(c->data, d);
        c->size = size;
        cacheBytes += size;
#ifdef VIEW_TIMING
        kprintf("baked prop %d at %d: %lu bytes in %lu frames, cache %lu\n", prop, d, size, hw_vblanks - t0, cacheBytes);
#else
        (void) t0;
#endif
    }
    c->used = frame;
    return c->data;
}
