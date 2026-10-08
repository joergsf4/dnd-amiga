#include "sfx.h"
#include "game.h"
#include "amiga/audio.h"

// On the Amiga the samples and the songs play on Paula (src/amiga/audio.c).
typedef struct
{
    const Sample *sample;
    u8 priority;        // 0..15: a sound with a lower priority cannot cut a higher one off
} Sfx;

static const Sfx sounds[SFX_COUNT] =
{
    { &sfx_hit,       6 },
    { &sfx_miss,      4 },
    { &sfx_hurt,      7 },
    { &sfx_spell,     6 },
    { &sfx_fire,      6 },
    { &sfx_heal,      6 },
    { &sfx_explosion, 9 },
    { &sfx_door,      5 },
    { &sfx_land,      10 },
    { &sfx_dice,      5 },
    { &sfx_victory,   12 },
    { &sfx_gameover,  12 },
    { &sfx_menu,      2 },
    { &sfx_quake,     11 },
    { &sfx_item,      5 },
    { &sfx_intro,     12 },
};

static MusicId currentMusic = MUSIC_NONE;

void sfx_init(void)
{
    currentMusic = MUSIC_NONE;
}

void sfx_play(SfxId id)
{
    const Sfx *s = &sounds[id];
    audio_play(s->sample->data, s->sample->size, s->sample->rate, s->priority);
}

void music_play(MusicId id)
{
    if (id == currentMusic) return;
    currentMusic = id;
    kprintf("music: %d\n", id);   // tools/emutest.sh's wait: steps sync to these
    switch (id)
    {
        case MUSIC_TITLE:   audio_playMusic(music_title); break;
        case MUSIC_DUNGEON: audio_playMusic(music_dungeon); break;
        case MUSIC_COMBAT:  audio_playMusic(music_combat); break;
        case MUSIC_BRIDGE:  audio_playMusic(music_bridge); break;
        case MUSIC_ENDING:  audio_playMusic(music_ending); break;
        default:            audio_playMusic(NULL); break;
    }
}

void music_playRoom(RoomId room)
{
    music_play(room == ROOM_6 ? MUSIC_BRIDGE : MUSIC_DUNGEON);
}
