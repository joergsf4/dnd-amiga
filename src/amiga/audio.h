#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <genesis.h>

// A one-shot sound effect (signed 8-bit, chip RAM) on channel 3, over the song's drums. It cuts
// a running effect off unless that one has a higher priority.
void audio_play(const u8 *data, u32 size, u16 rate, u8 priority);

// Starts a song (tools/make_res.py's song() format), looping; NULL stops the music.
void audio_playMusic(const u8 *stream);

#endif
