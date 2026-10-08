/*
 * Paula: the music and the sound effects.
 *
 * The songs (tools/generate_music.py, converted by tools/make_res.py) were written for the Mega
 * Drive's PSG -- three square-wave voices and a noise voice -- and play the same way here: channels
 * 0-2 loop a 16-byte square wave, channel 3 a noise sample, and a song is a stream of per-tick
 * period and volume changes (the format is in make_res.py's song()), stepped at 60 Hz from the
 * vertical blank interrupt.
 *
 * Sound effects are one-shot samples on channel 3, over the drums: the Mega Drive played them on
 * PCM channels of their own, four Paula channels don't leave room for that. While an effect runs
 * the song's noise voice is muted; when it's over, the noise loop comes back. A new effect cuts
 * the running one off unless that one has a higher priority.
 */
#include "hw.h"
#include "audio.h"

#define PAULA_CLOCK 3546895                // PAL
#define SQUARE_LEN  16
#define NOISE_LEN   2048
#define SFX_CH      3

static s8 *square, *noise;
static u16 *silence;

static const u8 *song, *songPos;
static u16 songWait;
static u8 musicPhase;
static u16 noisePeriod = 124;
static u8 noiseVolume;

static volatile u32 sfxEndsAt;             // hw_vblanks
static u8 sfxPriority;
static volatile bool sfxActive;

static void waitLines(u16 n)
{
    for (u16 i = 0; i < n; i++)
    {
        u16 line = custom->vhposr >> 8;
        while ((custom->vhposr >> 8) == line) ;
    }
}

// Starts a looping sample on a channel (volume 0 until the song sets one).
static void loop(u8 c, const void *data, u16 bytes, u16 period)
{
    volatile struct AudChannel *a = &custom->aud[c];
    custom->dmacon = DMAF_AUD0 << c;
    a->ac_ptr = (UWORD *) data;
    a->ac_len = bytes / 2;
    a->ac_per = period;
    custom->dmacon = DMAF_SETCLR | (DMAF_AUD0 << c);
}

void audio_init(void)
{
    if (!square)
    {
        square = hw_allocChip(SQUARE_LEN);
        noise = hw_allocChip(NOISE_LEN);
        silence = hw_allocChip(4);
        for (int i = 0; i < SQUARE_LEN; i++)
            square[i] = i < SQUARE_LEN / 2 ? 48 : -48;
        u16 r = 0xace1;
        for (int i = 0; i < NOISE_LEN; i++)
        {
            r = (r >> 1) ^ (-(r & 1) & 0xb400);
            noise[i] = (r & 1) ? 48 : -48;
        }
    }
    custom->dmacon = DMAF_AUDIO;
    custom->adkcon = 0x00ff;               // no modulation
    for (int c = 0; c < 4; c++)
        custom->aud[c].ac_vol = 0;
    for (int c = 0; c < 3; c++)
        loop(c, square, SQUARE_LEN, 428);
    loop(3, noise, NOISE_LEN, noisePeriod);
    sfxActive = FALSE;
}

void audio_shutdown(void)
{
    song = NULL;
    custom->dmacon = DMAF_AUDIO;
    for (int c = 0; c < 4; c++)
        custom->aud[c].ac_vol = 0;
}

// ---------------------------------------------------------------- music

void audio_playMusic(const u8 *stream)
{
    song = NULL;                           // the interrupt leaves it alone meanwhile
    for (int c = 0; c < 3; c++)
        custom->aud[c].ac_vol = 0;
    if (!sfxActive) custom->aud[SFX_CH].ac_vol = 0;
    noiseVolume = 0;
    songPos = stream;
    songWait = 0;
    song = stream;
}

static void tick(void)
{
    if (!song) return;
    if (songWait)
    {
        songWait--;
        return;
    }
    for (;;)
    {
        u8 cmd = *songPos++;
        switch (cmd >> 4)
        {
            case 1:
                custom->aud[cmd & 3].ac_per = (songPos[0] << 8) | songPos[1];
                songPos += 2;
                break;
            case 2:
                custom->aud[cmd & 3].ac_vol = *songPos++;
                break;
            case 3:
                if (cmd == 0x30)
                {
                    noisePeriod = (songPos[0] << 8) | songPos[1];
                    songPos += 2;
                    if (!sfxActive) custom->aud[SFX_CH].ac_per = noisePeriod;
                }
                else
                {
                    noiseVolume = *songPos++;
                    if (!sfxActive) custom->aud[SFX_CH].ac_vol = noiseVolume;
                }
                break;
            case 4:
                songWait = *songPos++ - 1;
                return;
            default:                       // 0xFF: from the top
                songPos = song;
                return;
        }
    }
}

// From the vertical blank interrupt: 6 song ticks per 5 frames, the Mega Drive's 60 Hz.
void audio_vblank(void)
{
    if (sfxActive && hw_vblanks >= sfxEndsAt)
    {
        sfxActive = FALSE;                 // the drums get their channel back
        loop(SFX_CH, noise, NOISE_LEN, noisePeriod);
        custom->aud[SFX_CH].ac_vol = noiseVolume;
    }
    tick();
    if (++musicPhase == 5)
    {
        musicPhase = 0;
        tick();
    }
}

// ---------------------------------------------------------------- sound effects

void audio_play(const u8 *data, u32 size, u16 rate, u8 priority)
{
    if (sfxActive && hw_vblanks < sfxEndsAt && sfxPriority > priority) return;

    volatile struct AudChannel *a = &custom->aud[SFX_CH];
    sfxActive = TRUE;                      // the song leaves the channel alone from now on
    custom->dmacon = DMAF_AUD0 << SFX_CH;  // stop it; the sample starts from the top
    waitLines(2);                          // DMA needs a moment to notice
    a->ac_ptr = (UWORD *) data;
    a->ac_len = size / 2;
    a->ac_per = PAULA_CLOCK / rate;
    a->ac_vol = 64;
    custom->dmacon = DMAF_SETCLR | (DMAF_AUD0 << SFX_CH);
    waitLines(2);
    a->ac_ptr = silence;                   // after the sample: 2 bytes of silence, looped
    a->ac_len = 1;
    sfxEndsAt = hw_vblanks + (size * 50) / rate + 1;
    sfxPriority = priority;
}
