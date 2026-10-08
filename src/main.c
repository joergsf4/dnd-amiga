#include "game.h"
#include "dungeon_map.h"
#include "dungeon_view.h"
#include "dungeon_objects.h"
#include "ui_panel.h"
#include "party.h"
#include "inventory.h"
#include "char_create.h"
#include "room1.h"
#include "room2.h"
#include "room3.h"
#include "room4.h"
#include "room5.h"
#include "room6.h"
#include "countdown.h"
#include "title.h"
#include "intro.h"
#include "sfx.h"
#include "figures.h"
#include "encounter.h"
#include "abilities.h"
#include "input.h"
#include "text.h"
#include "automap.h"

// Sprite tiles are reserved just below the font; the default 420 would collide with the view's two
// 560-tile buffers (dungeon_view.c). Avatars take 9 tiles each, figures up to 120 (a bust), a
// fight at most 2 cambions (2 x 72) or Zhalk (112) plus the marker (encounter.c).
#define SPRITE_VRAM_TILES 304   // all there is: tile maps start at 0xC000 (1536 tiles), the font
                                // takes the top 96, the view 16 + 2 x 560 below

#define ENEMY_STEP_FRAMES 45
#define ANIM_FRAMES 8          // a new animation frame (and a redraw, if any is in sight) every 8

static const RoomDef *shownRoom;

// Redraws the view and panel status. When the player has just arrived in a different room (game
// start, or through a door), also runs that room's onEnter hook -- after the new view is on
// screen, so an arrival text shows over the room rather than a blank view.
static bool redrawWorld(Player *p)
{
    dungeonView_render(p);
    uiPanel_drawStatus(p);

    const RoomDef *room = map_currentRoom();
    if (room != shownRoom)
    {
        shownRoom = room;
        music_playRoom(room->roomId);
        if (room->onEnter)
        {
            room->onEnter(p);
            uiPanel_redrawChrome();
            return TRUE;
        }
    }
    return FALSE;
}

// After a menu, text or fight: the presses that belonged to it must not count in the dungeon,
// but a press made during the redraw that follows must.
static void afterModal(Player *p)
{
    input_takePresses();
    if (redrawWorld(p)) input_takePresses();
    uiPanel_redrawChrome();
}

int game_main(bool hardReset)   // called by main() in src/amiga/hw.c
{
    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    SPR_initEx(SPRITE_VRAM_TILES);
    PAL_setPalette(PAL1, avatar_wir_sprite.palette->data, DMA);   // the companions' avatars
    text_init();
    sfx_init();

    intro_run();
    music_play(MUSIC_TITLE);   // also through the creation screen
    title_run();
    res_free_rcd_logo();      // Amiga: memory is tight, what's been shown goes
    res_free_title_image();
    u8 portrait;
    CharClass heroClass = charCreate_run(&portrait);
    party_init();
    party_addMember(heroClass)->portrait = portrait;
    res_free_portrait_fighter_1();   // Amiga: the busts were only for the creation screen
    res_free_portrait_fighter_2();
    res_free_portrait_fighter_3();
    res_free_portrait_rogue_1();
    res_free_portrait_rogue_2();
    res_free_portrait_rogue_3();
    res_free_portrait_mage_1();
    res_free_portrait_mage_2();
    res_free_portrait_mage_3();
    inventory_init();

    dungeonView_init();
    uiPanel_initSprites();
    uiPanel_redrawChrome();
    SPR_update();   // drops the creation screen's avatar sprite before the first view is drawn

    map_registerRoom(&ROOM1);
    map_registerRoom(&ROOM2);
    map_registerRoom(&ROOM3);
    map_registerRoom(&ROOM4);
    map_registerRoom(&ROOM5);
    map_registerRoom(&ROOM6);
    Player player;
    map_loadRoom(&ROOM1, &player);
    redrawWorld(&player);

    input_takePresses();
    u16 enemyTimer = 0, animTimer = 0;
    while (TRUE)
    {
        u16 pressed = input_takePresses();
        bool moved = FALSE;

        if (pressed & (BUTTON_UP | BUTTON_DOWN))
        {
            if (player_step(&player, (pressed & BUTTON_UP) ? 1 : -1)) countdown_step();
            moved = TRUE;
        }

        if (pressed & BUTTON_LEFT) { player_turn(&player, -1); moved = TRUE; }
        else if (pressed & BUTTON_RIGHT) { player_turn(&player, 1); moved = TRUE; }

        if (moved) redrawWorld(&player);

        if (pressed & BUTTON_B)
        {
            ab_partyMenu(&player);
            afterModal(&player);
        }

        if (pressed & BUTTON_C)
        {
            automap_screen(&player);
            afterModal(&player);
        }

        if (pressed & BUTTON_A)
        {
            RoomObject *target = dungeonObjects_interactTarget(&player);
            if (target)
            {
                map_currentRoom()->onInteract(&player, target);
                afterModal(&player);
            }
        }

        // Enemies close in in real time, one cell every ENEMY_STEP_FRAMES; next to the party, the
        // fight starts.
        if (++enemyTimer >= ENEMY_STEP_FRAMES)
        {
            enemyTimer = 0;
            if (encounter_tick(&player)) redrawWorld(&player);
        }
        RoomObject *foe = encounter_adjacent(&player);
        if (foe)
        {
            encounter_fight(&player, foe);
            afterModal(&player);
            enemyTimer = 0;
        }

        if (++animTimer >= ANIM_FRAMES)
        {
            animTimer = 0;
            dungeonView_animate(&player);
        }

        countdown_update();
        SPR_update();
        SYS_doVBlankProcess();
    }

    return 0;
}
