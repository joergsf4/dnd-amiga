/*
 * Start-up and the machine: loads the data file while the OS still runs, takes the hardware
 * over like an A500 game, runs the game, hands the machine back. Plus SGDK's SYS_* functions,
 * memory, the interrupts and debug output.
 */
#include <setjmp.h>
#include <stdarg.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include "hw.h"
#include "unzx0.h"

#define ciab ((volatile struct CIA *) 0xbfd000)

extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;
extern struct DosLibrary *DOSBase;

void hw_level2(void);                      // src/amiga/isr.s
void hw_level3(void);
ULONG hw_getVBR(void);
void hw_coldReboot(void);

volatile u32 hw_vblanks;
static void (*vblankCallback)(void);
static u8 callbackPhase;

// ---------------------------------------------------------------- memory

// Every block carries a header so that everything can be freed on exit, even after a fatal error.
typedef struct Block
{
    struct Block *next;
    u32 size;
} Block;

static Block *blocks;

static void *allocWith(u32 size, u32 flags)
{
    Block *b = AllocMem(size + sizeof(Block), flags | MEMF_CLEAR);
    if (!b)
    {
        kprintf("out of memory: %lu bytes, flags %lx, chip free %lu, any free %lu\n", size, flags,
                (u32) AvailMem(MEMF_CHIP), (u32) AvailMem(MEMF_ANY));
        hw_fatal("Nicht genug Speicher");
    }
    b->size = size + sizeof(Block);
    b->next = blocks;
    blocks = b;
    return b + 1;
}

void *hw_allocChip(u32 size) { return allocWith(size, MEMF_CHIP); }
void *hw_alloc(u32 size)     { return allocWith(size, MEMF_ANY); }

void hw_free(void *p)
{
    if (!p) return;
    Block *b = (Block *) p - 1;
    for (Block **l = &blocks; *l; l = &(*l)->next)
        if (*l == b)
        {
            *l = b->next;
            FreeMem(b, b->size);
            return;
        }
}

static void __attribute__((noinline)) freeAll(void)
{
    while (blocks)
    {
        Block *b = blocks;
        blocks = b->next;
        FreeMem(b, b->size);
    }
}

// ---------------------------------------------------------------- the data file

// dnd.pak (tools/make_res.py): "DNDP", entry count, then per entry offset, stored size, unpacked
// size and flags (bit 0: chip RAM, bit 1: ZX0-packed), then the data. Each entry gets its own
// block, in chip RAM if it has to be (blitter and audio data); packed ones are read into a
// buffer first and unpacked into it.
#define PAK_CHIP 1
#define PAK_ZX0  2

static u8 ** __attribute__((noinline)) loadPak(const char *name)
{
    BPTR f = Open((STRPTR) name, MODE_OLDFILE);
    if (!f) return NULL;
    u32 head[2];
    if (Read(f, head, 8) != 8 || head[0] != 0x444E4450)
    {
        Close(f);
        return NULL;
    }
    u32 count = head[1];
    u32 *table = hw_alloc(count * 16);
    u8 **entries = hw_alloc(count * sizeof(u8 *));
    Read(f, table, count * 16);
    u32 largest = 0;
    for (u32 i = 0; i < count; i++)
        if ((table[i * 4 + 3] & PAK_ZX0) && table[i * 4 + 1] > largest) largest = table[i * 4 + 1];
    u8 *packed = largest ? hw_alloc(largest) : NULL;

    u32 pos = 8 + count * 16;
    bool ok = TRUE;
    for (u32 i = 0; i < count && ok; i++)
    {
        u32 offset = table[i * 4], stored = table[i * 4 + 1], size = table[i * 4 + 2], flags = table[i * 4 + 3];
        entries[i] = (flags & PAK_CHIP) ? hw_allocChip(size) : hw_alloc(size);
        // The entries follow each other (4-byte aligned): read on instead of seeking, which on
        // an OFS floppy walks the file's block list from the start every time.
        u32 pad[1];
        if (offset > pos && Read(f, pad, offset - pos) != (LONG) (offset - pos))
            ok = FALSE;
        u8 *into = (flags & PAK_ZX0) ? packed : entries[i];
        if (ok && (u32) Read(f, into, stored) != stored)
            ok = FALSE;
        if (ok && (flags & PAK_ZX0) && unzx0(packed, entries[i]) != size)
            ok = FALSE;
        pos = offset + stored;
    }
    Close(f);
    hw_free(packed);
    hw_free(table);
    return ok ? entries : NULL;
}

// ---------------------------------------------------------------- taking the machine over

static struct View *oldView;
static UWORD oldDma, oldIntena, oldAdkcon;
static volatile APTR *vectors;
static APTR oldLevel2, oldLevel3;
static bool tookOver;

static void __attribute__((noinline)) takeOver(void)
{
    oldView = GfxBase->ActiView;
    LoadView(NULL);
    WaitTOF();
    WaitTOF();
    OwnBlitter();
    WaitBlit();
    Forbid();

    vectors = (SysBase->AttnFlags & AFF_68010) ? (volatile APTR *) Supervisor((ULONG (*)()) hw_getVBR) : 0;
    oldDma = custom->dmaconr;
    oldIntena = custom->intenar;
    oldAdkcon = custom->adkconr;
    custom->intena = 0x7fff;
    custom->intreq = 0x7fff;
    custom->dmacon = 0x7fff;

    // The floppy motor may still run from loading: switch it off (latched when DF0 is selected).
    ciab->ciaprb |= CIAF_DSKMOTOR | CIAF_DSKSEL3 | CIAF_DSKSEL2 | CIAF_DSKSEL1 | CIAF_DSKSEL0;
    ciab->ciaprb &= ~CIAF_DSKSEL0;
    ciab->ciaprb |= CIAF_DSKSEL0;

    oldLevel2 = vectors[0x68 / 4];
    oldLevel3 = vectors[0x6c / 4];
    vectors[0x68 / 4] = (APTR) hw_level2;
    vectors[0x6c / 4] = (APTR) hw_level3;
    tookOver = TRUE;

    vdp_init();
    joy_init();
    audio_init();
    ciaa->ciaicr = 0x7f;                            // CIA-A: only the keyboard
    ciaa->ciaicr = CIAICRF_SETCLR | CIAICRF_SP;
    custom->dmacon = DMAF_SETCLR | DMAF_MASTER | DMAF_RASTER | DMAF_BLITTER;
    custom->intena = INTF_SETCLR | INTF_INTEN | INTF_VERTB | INTF_PORTS;
}

static void __attribute__((noinline)) giveBack(void)
{
    if (!tookOver) return;
    hw_waitBlit();
    audio_shutdown();
    custom->intena = 0x7fff;
    custom->intreq = 0x7fff;
    custom->dmacon = 0x7fff;
    vectors[0x68 / 4] = oldLevel2;
    vectors[0x6c / 4] = oldLevel3;
    ciaa->ciaicr = CIAICRF_SETCLR | CIAICRF_SP;     // the OS's keyboard handler wants it back
    custom->cop1lc = (ULONG) GfxBase->copinit;
    custom->copjmp1 = 0;
    custom->dmacon = DMAF_SETCLR | oldDma;
    custom->intena = INTF_SETCLR | oldIntena;
    custom->adkcon = ADKF_SETCLR | oldAdkcon;
    tookOver = FALSE;

    Permit();
    DisownBlitter();
    LoadView(oldView);
    WaitTOF();
    WaitTOF();
}

// ---------------------------------------------------------------- interrupts (src/amiga/isr.s)

void hw_vblankIsr(void)
{
    hw_vblanks++;
    vdp_vblank();
    joy_vblank();
    audio_vblank();
    if (vblankCallback)
    {
        vblankCallback();                           // SGDK calls it at 60 Hz: once more every 5th
        if (++callbackPhase == 5)
        {
            callbackPhase = 0;
            vblankCallback();
        }
    }
}

void hw_keyIsr(u8 sdr)
{
    ciaa->ciacra |= CIACRAF_SPMODE;                 // handshake: SP low for at least 85 us
    joy_key(sdr);
    u16 line = custom->vhposr >> 8;
    while ((u16) ((custom->vhposr >> 8) - line) < 3) ;
    ciaa->ciacra &= ~CIACRAF_SPMODE;
}

// ---------------------------------------------------------------- SGDK: SYS_*

static u32 gameFrames;

void SYS_doVBlankProcess(void)
{
    vdp_flush();
    // 6 game frames per 5 display frames, i.e. the Mega Drive's 60 Hz on the 50 Hz display.
    // A game frame that fell behind (a redraw takes several) doesn't make the next ones hurry.
    gameFrames++;
    u32 due = hw_vblanks * 6 / 5;
    if (gameFrames + 1 < due) gameFrames = due - 1;
    while (gameFrames * 5 > hw_vblanks * 6) ;
}

void SYS_setVBlankCallback(void (*callback)(void))
{
    vblankCallback = callback;
}

static jmp_buf exitJmp;
static int exitCode;

// "Start a new game": the machine reboots and the disk starts the game afresh, as on the Mega
// Drive, where this resets the console.
void SYS_hardReset(void)
{
    custom->intena = 0x7fff;
    custom->dmacon = 0x7fff;
    Supervisor((ULONG (*)()) hw_coldReboot);
    for (;;) ;
}

void hw_fatal(const char *msg)
{
    kprintf("FATAL: %s\n", msg);
    exitCode = RETURN_FAIL;
    longjmp(exitJmp, 1);
}

void hw_waitBlit(void)
{
    (void) custom->dmaconr;                         // the first read after a blit start can lie
    while (custom->dmaconr & DMAF_BLTDONE) ;
}

static u16 randbase = 0xD94B;

u16 sgdk_random(void)
{
    randbase ^= (randbase >> 1) ^ custom->vhposr;
    randbase ^= (randbase << 1);
    return randbase;
}

// ---------------------------------------------------------------- debug output

void kprintf(const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    custom->serper = 3546895 / 115200 - 1;          // PAL clock
    for (const char *s = buf; *s; s++)
    {
        if (*s == '\n')
        {
            while (!(custom->serdatr & 0x2000)) ;
            custom->serdat = 0x100 | '\r';
        }
        while (!(custom->serdatr & 0x2000)) ;
        custom->serdat = 0x100 | (u8) *s;
    }
}

// ---------------------------------------------------------------- main

// Everything that runs before the way out; kept out of main() so that setjmp there has no
// locals to clobber.
// What the game needs free at start-up: the data (~160 KB of it in chip RAM, ~130 KB anywhere),
// the screens (~110 KB chip) and room for the view's baked walls and props. An A500 with the
// 512 KB trapdoor expansion has ~660 KB free here under Kickstart 1.3.
#define NEED_CHIP  (380 * 1024UL)
#define NEED_TOTAL (600 * 1024UL)

static void say(const char *text)
{
    Write(Output(), (APTR) text, strlen(text));
}

static void __attribute__((noinline)) run(void)
{
    kprintf("dnd: start, chip free %lu (largest %lu), other free %lu (largest %lu)\n",
            (u32) AvailMem(MEMF_CHIP), (u32) AvailMem(MEMF_CHIP | MEMF_LARGEST),
            (u32) AvailMem(MEMF_FAST), (u32) AvailMem(MEMF_FAST | MEMF_LARGEST));
    u32 chip = AvailMem(MEMF_CHIP), total = AvailMem(MEMF_ANY);
    if (chip < NEED_CHIP || total < NEED_TOTAL)
    {
        say("Dieses Spiel braucht einen Amiga mit 1 MB Speicher\n"
            "(512 KB Chip-RAM + 512 KB Erweiterung).\n");
        exitCode = RETURN_FAIL;
        return;
    }
    u8 **entries = loadPak("dnd.pak");
    if (!entries)
    {
        say("dnd.pak fehlt oder ist beschaedigt.\n");
        exitCode = RETURN_FAIL;
        return;
    }
    res_bind(entries);
    kprintf("dnd: loaded, chip free %lu, any free %lu\n", (u32) AvailMem(MEMF_CHIP), (u32) AvailMem(MEMF_ANY));
    takeOver();
    game_main(TRUE);
}

int main(void)
{
    exitCode = RETURN_OK;
    if (setjmp(exitJmp) == 0) run();
    giveBack();
    freeAll();
    return exitCode;
}
