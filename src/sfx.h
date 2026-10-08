#ifndef _SFX_H_
#define _SFX_H_

#include <genesis.h>
#include "dungeon_map.h"

// Sound effects are 8-bit PCM samples (tools/generate_sfx.py), the music is PSG chiptune
// (tools/generate_music.py); on the Amiga both play on Paula (src/amiga/audio.c): the songs on
// three square-wave channels and a noise channel, the effects over the noise channel.

typedef enum
{
    SFX_HIT,        // a weapon or spell hits an enemy
    SFX_MISS,       // a swing that misses
    SFX_HURT,       // the party takes a hit
    SFX_SPELL,      // Kältestrahl, Geschoss, Schlaf, Magierrüstung, Schild, Göttlicher Sinn
    SFX_FIRE,       // Feuerpfeil, Brennende Hände
    SFX_HEAL,       // potions, Heilende Hände, Erholen, the restoration stations
    SFX_EXPLOSION,  // an acid tank bursts
    SFX_DOOR,       // a sphincter door opens
    SFX_LAND,       // Lae'zel drops in
    SFX_DICE,       // a skill check rolls
    SFX_VICTORY,
    SFX_GAMEOVER,
    SFX_MENU,       // menu cursor
    SFX_QUAKE,      // the ship shudders (ending, Zhalk's arrival)
    SFX_ITEM,       // loot
    SFX_INTRO,      // the club logo sweeps in (from Wanderburg)
    SFX_COUNT
} SfxId;

typedef enum
{
    MUSIC_NONE,
    MUSIC_TITLE,
    MUSIC_DUNGEON,
    MUSIC_COMBAT,
    MUSIC_BRIDGE,
    MUSIC_ENDING
} MusicId;

void sfx_init(void);            // once at start-up
void sfx_play(SfxId id);        // a higher priority can't be cut off by a lower one
void music_play(MusicId id);    // starts a looping song; restarting the running one does nothing
void music_playRoom(RoomId room);   // the song of a room: the bridge has its own

#endif
