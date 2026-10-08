/*
 * Unpacks ZX0 data (format v2, Einar Saukas' optimal LZ77 compressor; tools/make_res.py packs the
 * entries of dnd.pak with it). Run once per entry at start-up, while the data file loads.
 *
 * The stream interleaves bits (read MSB first from whole bytes) with plain bytes: blocks of
 * literals, copies from the last offset and copies from a new offset, lengths and offsets as
 * interlaced Elias gamma codes. A new offset's low 7 bits come in a byte whose lowest bit is at
 * the same time the first bit of the following length ("backtrack").
 */
#include "unzx0.h"

typedef struct
{
    const u8 *in;
    u8 bits, mask;
    bool backtrack;
    u8 last;
} Reader;

static inline u8 byte(Reader *r)
{
    return r->last = *r->in++;
}

static inline u8 bit(Reader *r)
{
    if (r->backtrack)
    {
        r->backtrack = FALSE;
        return r->last & 1;
    }
    r->mask >>= 1;
    if (!r->mask)
    {
        r->mask = 0x80;
        r->bits = byte(r);
    }
    return (r->bits & r->mask) ? 1 : 0;
}

static u32 eliasGamma(Reader *r, u8 inverted)
{
    u32 value = 1;
    while (!bit(r))
        value = (value << 1) | (bit(r) ^ inverted);
    return value;
}

u32 unzx0(const u8 *src, u8 *dst)
{
    Reader r = { src, 0, 0, FALSE, 0 };
    u8 *out = dst;
    u32 offset = 1, length;

    for (;;)
    {
        // literals
        for (length = eliasGamma(&r, 0); length; length--)
            *out++ = byte(&r);
        if (!bit(&r))
        {
            // copy from the last offset
            for (length = eliasGamma(&r, 0); length; length--, out++)
                *out = out[-(s32) offset];
            if (!bit(&r)) continue;
        }
        // copy from a new offset, maybe several in a row
        for (;;)
        {
            u32 msb = eliasGamma(&r, 1);
            if (msb == 256) return out - dst;
            offset = ((msb - 1) << 7) + 128 - (byte(&r) >> 1);
            r.backtrack = TRUE;
            for (length = eliasGamma(&r, 0) + 1; length; length--, out++)
                *out = out[-(s32) offset];
            if (!bit(&r)) break;
        }
    }
}
