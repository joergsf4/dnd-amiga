#include "automap.h"
#include "dungeon_view.h"
#include "textbox.h"
#include "text.h"
#include "sfx.h"
#include "view_gen.h"
#include "input.h"

// The map is drawn into the view's pixel buffer (dungeonView_buffer) in the view palette (PAL0)
// and shown through the view's double buffer.

// View palette indices (tools/make_view.py)
#define C_BLACK 0
#define C_CH0   1
#define C_CH1   2
#define C_CH2   3
#define C_CH3   4
#define C_FL0   5
#define C_FL1   6
#define C_MEM1  9
#define C_MEM2  10
#define C_TEAL  11
#define C_BONE  12
#define C_BLUE  13
#define C_GLINT 15

static u16 seen[ROOM_COUNT][AUTOMAP_MAX_H];   // a bit per cell: uncovered
static u16 sight[AUTOMAP_MAX_H];              // the current room's cells in sight right now

static const char *const roomName[ROOM_COUNT] = {
    "KLONKAMMER", "OPERATIONSSAAL", "AUSSENDECK", "KAPSELSAAL", "LABOR", "BRÜCKE",
};

void automap_beginSight(void)
{
    memset(sight, 0, sizeof(sight));
}

void automap_see(s16 x, s16 y)
{
    const RoomDef *room = map_currentRoom();
    if (!room || x < 0 || y < 0 || x >= room->w || y >= room->h || x >= AUTOMAP_MAX_W || y >= AUTOMAP_MAX_H)
        return;
    seen[room->roomId][y] |= 1 << x;
    sight[y] |= 1 << x;
}

static bool known(RoomId r, s16 x, s16 y)
{
    return (seen[r][y] >> x) & 1;
}

static bool anySeen(RoomId r)
{
    for (u8 y = 0; y < AUTOMAP_MAX_H; y++)
        if (seen[r][y]) return TRUE;
    return FALSE;
}

// ---------------------------------------------------------------- drawing into the view buffer

static u8 *buf;

static void pixel(s16 x, s16 y, u8 col)
{
    if (x < 0 || y < 0 || x >= VIEW_W || y >= VIEW_H) return;
    u8 *b = buf + y * VIEW_BPR + (x >> 1);
    *b = (x & 1) ? (*b & 0xF0) | col : (*b & 0x0F) | (col << 4);
}

// Whole bytes (two pixels) where it can: drawing a room pixel by pixel took longer than half a
// second.
static void rect(s16 x0, s16 y0, s16 w, s16 h, u8 col)
{
    u8 pair = col | (col << 4);
    s16 x1 = x0 + w;
    if (y0 < 0) { h += y0; y0 = 0; }
    if (y0 + h > VIEW_H) h = VIEW_H - y0;
    for (s16 y = y0; y < y0 + h; y++)
    {
        s16 x = x0;
        if (x & 1) pixel(x++, y, col);
        u8 *row = buf + y * VIEW_BPR;
        for (; x + 1 < x1; x += 2)
            row[x >> 1] = pair;
        if (x < x1) pixel(x, y, col);
    }
}

// A mark in the middle of a cell: a square `inset` pixels from the cell's edges, with a rim.
static void mark(s16 x, s16 y, s16 cs, s16 inset, u8 rim, u8 fill)
{
    rect(x + inset, y + inset, cs - 2 * inset, cs - 2 * inset, rim);
    if (cs - 2 * inset > 2)
        rect(x + inset + 1, y + inset + 1, cs - 2 * inset - 2, cs - 2 * inset - 2, fill);
}

// The party: an arrow pointing the way it faces.
static void arrow(s16 x, s16 y, s16 cs, Facing f)
{
    s16 n = cs - 2;
    for (s16 b = 0; b < n; b++)          // b: from the tip back
        for (s16 a = 0; a < n; a++)      // a: across
        {
            s16 off = 2 * a - (n - 1);
            if (off < 0) off = -off;
            if (off > b + 1) continue;
            s16 px, py;
            switch (f)
            {
                case FACE_NORTH: px = a;         py = b;         break;
                case FACE_SOUTH: px = a;         py = n - 1 - b; break;
                case FACE_EAST:  px = n - 1 - b; py = a;         break;
                default:         px = b;         py = a;         break;
            }
            pixel(x + 1 + px, y + 1 + py, C_GLINT);
        }
}

static void drawWall(s16 x, s16 y, s16 cs)
{
    rect(x, y, cs, cs, C_CH2);
    rect(x, y, cs, 1, C_CH3);            // lit from the top left
    rect(x, y, 1, cs, C_CH3);
    rect(x, y + cs - 1, cs, 1, C_CH0);
    rect(x + cs - 1, y, 1, cs, C_CH0);
}

static void drawObject(const RoomObject *o, s16 x, s16 y, s16 cs, bool current, s16 cx, s16 cy)
{
    s16 q = cs / 4;
    switch (o->kind)
    {
        case OBJ_DOOR_EXIT:
            rect(x, y, cs, cs, C_CH0);
            rect(x + 2, y + 2, cs - 4, cs - 4, C_BLUE);
            break;
        case OBJ_BREACH:
            rect(x, y, cs, cs, C_MEM1);
            rect(x + q, y + q, cs - 2 * q, cs - 2 * q, C_MEM2);
            break;
        case OBJ_TABLET:
            mark(x, y, cs, q, C_CH0, C_BONE);
            break;
        case OBJ_ENEMY_GROUP:   // they roam: only where they are right now, if that's in sight
            if (current && ((sight[cy] >> cx) & 1)) mark(x, y, cs, q - 1, C_BLACK, C_MEM2);
            break;
        case OBJ_FIRE:
            mark(x, y, cs, q + 1, C_MEM1, C_MEM2);
            break;
        case OBJ_LARVA_TANK: case OBJ_POD_OPEN: case OBJ_POD_BROKEN: case OBJ_POD_SEALED:
        case OBJ_SHADOWHEART_POD: case OBJ_WOMAN_POD: case OBJ_ACID_TANK:
            mark(x, y, cs, q, C_BLACK, C_TEAL);
            break;
        default:                // things to use or look at
            mark(x, y, cs, q, C_BLACK, C_BONE);
            break;
    }
}

// The dungeon as one map: where each room lies (the cell of its top left corner). Neighbouring
// rooms share the wall cell their doors sit in; only Room 1's west door and Room 2's south door
// don't line up, so a short passage joins them.
static const s8 roomOrigin[ROOM_COUNT][2] = {
    { 6, 39 },   // Klonkammer
    { 0, 30 },   // Operationssaal
    { 2, 21 },   // Außendeck
    { 0, 13 },   // Kapselsaal
    { 8, 15 },   // Labor
    { 0, 0 },    // Brücke
};
#define WORLD_W 15
#define WORLD_H 46
static const s8 passage[][2] = { { 5, 40 }, { 4, 40 }, { 4, 39 }, { 4, 38 } };   // Room 1 <-> 2

#define CELL 8                    // pixels per cell
#define ORIGIN_X ((VIEW_W - WORLD_W * CELL) / 2)

static s16 scrollY;               // pixels of the map above the top of the view

static void drawFloor(s16 x, s16 y, s16 cs)
{
    rect(x, y, cs, cs, C_FL0);
    rect(x + 1, y + 1, cs - 1, cs - 1, C_FL1);
}

static void drawRoom(const RoomDef *room, const Player *p)
{
    RoomId r = room->roomId;
    bool current = room == map_currentRoom();
    s16 ox = ORIGIN_X + roomOrigin[r][0] * CELL;
    s16 oy = roomOrigin[r][1] * CELL - scrollY;
    if (oy + room->h * CELL <= 0 || oy >= VIEW_H) return;

    for (s16 cy = 0; cy < room->h; cy++)
        for (s16 cx = 0; cx < room->w; cx++)
        {
            if (!known(r, cx, cy)) continue;
            s16 x = ox + cx * CELL, y = oy + cy * CELL;
            if (room->grid[cy][cx] == '1')
                drawWall(x, y, CELL);
            else
                drawFloor(x, y, CELL);
        }

    u8 count;
    const RoomObject *objects = map_roomStateOf(r, &count);
    for (u8 i = 0; i < count; i++)
    {
        const RoomObject *o = &objects[i];
        if (o->kind == OBJ_NONE || (o->flags & OBJFLAG_HIDDEN) || !known(r, o->x, o->y)) continue;
        drawObject(o, ox + o->x * CELL, oy + o->y * CELL, CELL, current, o->x, o->y);
    }

    if (current) arrow(ox + p->x * CELL, oy + p->y * CELL, CELL, p->facing);
}

static void drawDungeon(const Player *p)
{
    buf = dungeonView_buffer();
    memset(buf, 0, VIEW_BYTES);
    if (anySeen(ROOM_1) && anySeen(ROOM_2))           // walked through
        for (u8 i = 0; i < sizeof(passage) / sizeof(passage[0]); i++)
            drawFloor(ORIGIN_X + passage[i][0] * CELL, passage[i][1] * CELL - scrollY, CELL);
    for (u8 r = 0; r < ROOM_COUNT; r++)
    {
        const RoomDef *room = map_findRoom((RoomId) r);
        if (room && anySeen((RoomId) r) && room != map_currentRoom()) drawRoom(room, p);
    }
    drawRoom(map_currentRoom(), p);                    // last: its doors and the party on top
    dungeonView_present();
}

// ---------------------------------------------------------------- the screen

static void clearBox(void)
{
    for (u8 r = 0; r < TEXTBOX_H; r++)
        text_draw("                            ", 0, TEXTBOX_ROW + r);
}

static void drawText(void)
{
    char s[48];
    clearBox();
    sprintf(s, "KARTE - HIER: %s", roomName[map_currentRoom()->roomId]);
    text_draw(s, 1, TEXTBOX_ROW);
    text_draw("PFEIL: IHR   ROT: GEGNER", 1, TEXTBOX_ROW + 2);
    text_draw("BLAU: TÜR    HELL: DINGE", 1, TEXTBOX_ROW + 3);
    text_draw("HOCH/RUNTER: BLÄTTERN", 1, TEXTBOX_ROW + 6);
    text_draw("E/M: ZURÜCK", 1, TEXTBOX_ROW + 7);
}

#define SCROLL_MAX (WORLD_H * CELL - VIEW_H)
#define SCROLL_STEP (6 * CELL)

void automap_screen(const Player *p)
{
    const RoomDef *room = map_currentRoom();
    scrollY = (roomOrigin[room->roomId][1] + p->y) * CELL + CELL / 2 - VIEW_H / 2;   // the party in the middle
    if (scrollY < 0) scrollY = 0;
    if (scrollY > SCROLL_MAX) scrollY = SCROLL_MAX;
    sfx_play(SFX_MENU);
    drawDungeon(p);
    drawText();

    input_takePresses();   // latched: a press made while the map is being drawn still counts
    while (TRUE)
    {
        u16 pressed = input_takePresses();
        if (pressed & (BUTTON_B | BUTTON_C)) break;
        if (pressed & (BUTTON_UP | BUTTON_DOWN))
        {
            s16 y = scrollY + ((pressed & BUTTON_UP) ? -SCROLL_STEP : SCROLL_STEP);
            if (y < 0) y = 0;
            if (y > SCROLL_MAX) y = SCROLL_MAX;
            if (y != scrollY)
            {
                scrollY = y;
                sfx_play(SFX_MENU);
                drawDungeon(p);
            }
        }
        SPR_update();
        SYS_doVBlankProcess();
    }
    clearBox();
}
