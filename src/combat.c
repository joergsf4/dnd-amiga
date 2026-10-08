#include "combat.h"
#include "figures.h"
#include "textbox.h"
#include "party.h"
#include "inventory.h"
#include "abilities.h"
#include "ui_panel.h"
#include "dungeon_view.h"
#include "sfx.h"
#include "game.h"
#include "equipment.h"
#include "dice.h"

// Every text line must fit the message area: at most 27 characters (an umlaut counts as one),
// menu options at most 24. Names are at most 8 characters ("KOBOLD A", "LAE'ZEL", "SCHATTEN"),
// which the formats below rely on.
//
// Class features (D&D 5e / BG3 level 1, see src/abilities.h for the overview): each party member
// gets a main menu of at most four entries -- Angriff (Magier: Zaubertrick), a submenu with the
// class's features (Fähigkeit / Zauber), Heiltrank, Abwehr.

// Level-1-friendly versions of the Monster Manual (and BG3) values.
const EnemyDef ENEMY_IMP     = { "KOBOLD", 9, 13, 4, 4, 2, 0, 1, 1, &fig_imp_sprite, 124, FALSE };
const EnemyDef ENEMY_HOUND   = { "HUND", 12, 13, 4, 6, 2, 0, 1, 1, &fig_hound_sprite, 140, FALSE };
const EnemyDef ENEMY_CAMBION = { "DÄMON", 16, 15, 5, 8, 3, 0, 1, 2, &fig_cambion_sprite, 142, FALSE };
const EnemyDef ENEMY_ZHALK   = { "ZHALK", 45, 17, 6, 10, 4, 4, 2, 3, &fig_zhalk_sprite, 150, TRUE };

#define VIEW_CENTER_X 112   // the view is 224 px wide (view_gen.h)
#define TANK_AC     5
#define TANK_TARGET 0xFF
#define FOE_SPACING 80      // px between figure centres
#define HIDE_DC     11      // the imps' passive perception
#define SLEEP_DICE  5       // Schlaf: 5W8 hit points of enemies fall asleep

#define SCROLL_DC  13        // Brennende Hände: GES save for half damage
#define COMMAND_DC 13        // Befehl: WEI save
#define ZHALK_UNARMED 1      // fig_zhalk's frame without the Everburn Blade

#define FS_ASLEEP 0x01      // skips its turns until hurt; attacks against it have advantage
#define FS_PRONE  0x02      // attacks against it have advantage; spends its next turn getting up
#define FS_SLOWED 0x04      // Kältestrahl: its next attack has disadvantage
#define FS_DISARMED 0x08    // Befehl "Fallenlassen!": fists only (1W4, no fire)

typedef struct
{
    const EnemyDef *def;
    u8 hp;
    u8 status;
    Sprite *spr;
    s16 cx;
    char name[10];
} Foe;

static Foe foes[COMBAT_MAX_ENEMIES];
static u8 foeCount;
static RoomObject *tankObj;
static const Player *viewer;
static Sprite *arrow;
static u8 targetMap[COMBAT_MAX_ENEMIES + 1];   // target menu option -> foe index or TANK_TARGET

// Per-fight state of the party members.
static bool defending[PARTY_MAX];    // Abwehr: +2 AC until their next turn
static bool hidden[PARTY_MAX];       // Verstecken: can't be targeted, next attack with advantage
static bool shielded[PARTY_MAX];     // Schild: +5 AC until their next turn
static bool usedSecondWind[PARTY_MAX], usedCleave[PARTY_MAX], usedTopple[PARTY_MAX];
static bool usedCommand[PARTY_MAX];  // Befehl: once per fight
static bool fled, fleeAllowed, foeFled;

static void (*roundHook)(void);

bool combat_foeFled(void)
{
    return foeFled;
}

void combat_setRoundHook(void (*hook)(void))
{
    roundHook = hook;
}

static u8 roll(u8 die)
{
    return dice_roll(die);
}

// The Everburn Blade's flames (and every other fire in the figure palette) flicker: the three fire
// colours of the hell palette rotate every 8 frames while a burning enemy is in the fight.
#define FIRE_INDEX (PAL2 * 16 + 8)
static u16 fireColours[3];
static u8 fireTimer, firePhase;

static void cycleFire(void)
{
    if (++fireTimer < 8) return;
    fireTimer = 0;
    firePhase = (firePhase + 1) % 3;
    u16 rotated[3];
    for (u8 i = 0; i < 3; i++) rotated[i] = fireColours[(i + firePhase) % 3];
    PAL_setColors(FIRE_INDEX, rotated, 3, CPU);
}

static void stopFire(void)
{
    SYS_setVBlankCallback(NULL);
    PAL_setColors(FIRE_INDEX, fireColours, 3, CPU);
}

// A d20 with advantage (mode > 0: the better of two) or disadvantage (mode < 0: the worse);
// *label names the roll for the message ("Wurf", "Vort.", "Nacht.").
static u8 d20(s8 mode, const char **label)
{
    u8 a = roll(20), b = roll(20);
    *label = mode > 0 ? "Vort." : mode < 0 ? "Nacht." : "Wurf";
    if (mode > 0) return a > b ? a : b;
    if (mode < 0) return a < b ? a : b;
    return a;
}

// Commentary: lines stay up while the caller pauses (A skips a pause).
static void show(const char *a, const char *b, const char *c)
{
    const char *lines[3] = { a, b, c };
    textbox_print(lines, 3);
}

static void pause(u16 frames)
{
    u16 prevJoy = JOY_readJoypad(JOY_1);
    for (u16 i = 0; i < frames; i++)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        if (joy & ~prevJoy & BUTTON_A) break;
        prevJoy = joy;
        figures_wait(1);
    }
}

static void say(const char *a, const char *b, const char *c)
{
    const char *lines[3] = { a, b, c };
    textbox_show(lines, 3, NULL, 0);   // NULL lines are skipped
}

static bool foesLeft(void)
{
    for (u8 i = 0; i < foeCount; i++)
        if (foes[i].hp) return TRUE;
    return FALSE;
}

static bool partyStanding(void)
{
    for (u8 i = 0; i < PARTY_MAX; i++)
        if (party.members[i].active && party.members[i].hp) return TRUE;
    return FALSE;
}

// Hinterhältiger Angriff needs advantage or an ally in the fight next to the target.
static bool allyFighting(u8 m)
{
    for (u8 i = 0; i < PARTY_MAX; i++)
        if (i != m && party.members[i].active && party.members[i].hp) return TRUE;
    return FALSE;
}

static u8 memberAC(u8 m)
{
    return ab_armorClass(&party.members[m]) + (defending[m] ? 2 : 0) + (shielded[m] ? 5 : 0);
}

// ---------------------------------------------------------------- targets

static void arrowHook(u8 cursor)
{
    u8 t = targetMap[cursor];
    if (t == TANK_TARGET)
    {
        SPR_setVisibility(arrow, HIDDEN);   // the tank is part of the view, not a figure
        return;
    }
    SPR_setPosition(arrow, foes[t].cx - 4, foes[t].def->bottom - foes[t].def->sprite->h - 10);
    SPR_setVisibility(arrow, VISIBLE);
}

static u8 chooseTarget(bool allowTank)
{
    const char *options[COMBAT_MAX_ENEMIES + 1];
    u8 n = 0;
    for (u8 i = 0; i < foeCount; i++)
    {
        if (!foes[i].hp) continue;
        targetMap[n] = i;
        options[n++] = foes[i].name;
    }
    if (allowTank && tankObj && !(tankObj->flags & OBJFLAG_BROKEN))
    {
        targetMap[n] = TANK_TARGET;
        options[n++] = "Säuretank";
    }
    if (n == 1) return targetMap[0];

    const char *lines[1] = { "Welches Ziel?" };
    textbox_setCursorHook(arrowHook);
    u8 choice = textbox_show(lines, 1, options, n);
    textbox_setCursorHook(NULL);
    SPR_setVisibility(arrow, HIDDEN);
    return targetMap[choice];
}

// Damages a foe (blinking, waking it up); returns TRUE if it fell.
static bool damageFoe(u8 f, u8 dmg)
{
    Foe *e = &foes[f];
    sfx_play(SFX_HIT);
    figures_blink(e->spr, 3);
    e->status &= ~FS_ASLEEP;
    e->hp = dmg >= e->hp ? 0 : e->hp - dmg;
    if (e->hp) return FALSE;
    SPR_setVisibility(e->spr, HIDDEN);
    return TRUE;
}

static void announceFall(const char *l0, const char *l1, u8 f)
{
    char l2[32];
    sprintf(l2, "%s fällt!", foes[f].name);
    show(l0, l1, l2);
    pause(60);
}

// The acid tank bursts: 2d6 to every enemy still standing.
static void burstTank(void)
{
    tankObj->flags |= OBJFLAG_BROKEN;
    sfx_play(SFX_EXPLOSION);
    dungeonView_render(viewer);
    figures_shakeView();

    u8 dmg = roll(6) + roll(6);
    u8 standing = 0, fallen = 0;
    char line[32];
    for (u8 i = 0; i < foeCount; i++)
    {
        if (!foes[i].hp) continue;
        standing++;
        if (damageFoe(i, dmg)) fallen++;
    }
    sprintf(line, fallen && fallen == standing ? "%d Schaden - alle fallen!" : "%d Schaden an allen!", dmg);
    show("Der Tank platzt! Säure", "spritzt über die Gegner:", line);
    pause(100);
}

// Any attack on the tank: AC 5, bursts on a hit.
static void attackTank(Character *c, u8 bonus, bool autoHit)
{
    char l0[32];
    sprintf(l0, "%s zielt auf den Tank.", c->name);
    u8 r = roll(20);
    if (!autoHit && (r == 1 || r + bonus < TANK_AC))
    {
        show(l0, "Daneben!", NULL);
        pause(70);
        return;
    }
    show(l0, "Treffer!", NULL);
    pause(40);
    burstTank();
}

// ---------------------------------------------------------------- weapon attacks

// Weapon damage; Lae'zel's fighting style Großwaffen rerolls 1s and 2s once.
static u8 weaponDie(const Character *c)
{
    u8 r = roll(c->dmgDie);
    if (c->cls == CLASS_LAEZEL && r <= 2) r = roll(c->dmgDie);
    return r;
}

typedef enum { STRIKE_NORMAL, STRIKE_TOPPLE } Strike;

// The easter egg: whoever wields Zhalk's Everburn Blade never misses and fells anything in one blow
// -- Spalten burns through every enemy at once.
static bool wieldsEverburn(const Character *c)
{
    return c->equip[SLOT_WEAPON] == EQ_EVERBURN;
}

static void everburnStrike(const char *l0, u8 f)
{
    char l1[32];
    sprintf(l1, "%s: %d Schaden.", foes[f].name, foes[f].hp);
    damageFoe(f, foes[f].hp);
    sfx_play(SFX_SPELL);
    show(l0, "Die Immerbrand lodert auf!", l1);
    pause(70);
    announceFall(l0, "Die Immerbrand lodert auf!", f);
}

static void weaponAttack(u8 m, u8 t, Strike strike)
{
    Character *c = &party.members[m];
    if (t == TANK_TARGET)
    {
        hidden[m] = FALSE;
        attackTank(c, c->atk, FALSE);
        return;
    }
    Foe *e = &foes[t];
    s8 mode = (hidden[m] || (e->status & (FS_ASLEEP | FS_PRONE))) ? 1 : 0;
    hidden[m] = FALSE;                                   // attacking gives the hiding place away

    char l0[32], l1[32], l2[32];
    if (wieldsEverburn(c))
    {
        sprintf(l0, "%s greift %s an.", c->name, e->name);
        everburnStrike(l0, t);
        return;
    }
    const char *label;
    u8 r = d20(mode, &label);
    u8 total = r + c->atk;
    bool hit = r == 20 || (r != 1 && total >= e->def->ac);
    if (strike == STRIKE_TOPPLE)
        sprintf(l0, "%s: Niederwerfen!", c->name);
    else
        sprintf(l0, "%s greift %s an.", c->name, e->name);
    sprintf(l1, "%s %d + %d = %d: %s", label, r, c->atk, total, r == 20 ? "Kritisch!" : hit ? "Treffer!" : "daneben.");
    show(l0, l1, NULL);
    if (!hit) sfx_play(SFX_MISS);
    pause(40);
    if (!hit)
    {
        pause(40);
        return;
    }

    u8 dmg = weaponDie(c) + c->dmgBonus;
    if (r == 20) dmg += weaponDie(c);                    // a natural 20 rolls the damage die twice
    if (c->fireDie) dmg += roll(c->fireDie);             // the Everburn Blade's fire
    bool sneak = c->cls == CLASS_ROGUE && (mode > 0 || allyFighting(m));
    if (sneak) dmg += roll(6);                           // Hinterhältiger Angriff: +1W6
    bool fell = damageFoe(t, dmg);
    if (strike == STRIKE_TOPPLE && !fell)
    {
        e->status |= FS_PRONE;
        sprintf(l2, "%d Schaden - am Boden!", dmg);
    }
    else
        sprintf(l2, sneak ? "%d Schaden (Hinterhalt)." : "%d Schaden.", dmg);
    show(l0, l1, l2);
    pause(80);
    if (fell) announceFall(l0, l1, t);
}

// Spalten: one swing through up to three enemies, half damage each.
static void cleave(u8 m)
{
    Character *c = &party.members[m];
    usedCleave[m] = TRUE;
    char l0[32], l1[32], l2[32];
    sprintf(l0, "%s: Spalten!", c->name);
    for (u8 f = 0; f < foeCount; f++)
    {
        Foe *e = &foes[f];
        if (!e->hp) continue;
        if (wieldsEverburn(c))
        {
            everburnStrike(l0, f);
            continue;
        }
        const char *label;
        u8 r = d20((e->status & (FS_ASLEEP | FS_PRONE)) ? 1 : 0, &label);
        u8 total = r + c->atk;
        bool hit = r == 20 || (r != 1 && total >= e->def->ac);
        sprintf(l1, "%s: %d + %d = %d", e->name, r, c->atk, total);
        if (!hit)
        {
            show(l0, l1, "Daneben.");
            pause(60);
            continue;
        }
        u8 dmg = (weaponDie(c) + c->dmgBonus + 1) / 2;
        sprintf(l2, "Treffer, %d Schaden.", dmg);
        bool fell = damageFoe(f, dmg);
        show(l0, l1, l2);
        pause(70);
        if (fell) announceFall(l0, l1, f);
    }
}

// Erholen (Second Wind): a bonus action, 1W10 + 1 KP, once per fight.
static void secondWind(u8 m)
{
    Character *c = &party.members[m];
    usedSecondWind[m] = TRUE;
    sfx_play(SFX_HEAL);
    u8 amount = roll(10) + 1;
    if (c->hp + amount > c->hpMax) amount = c->hpMax - c->hp;
    c->hp += amount;
    uiPanel_redrawChrome();
    char l0[32], l1[32];
    sprintf(l0, "%s schnauft durch:", c->name);
    sprintf(l1, "+%d KP. (Bonusaktion)", amount);
    show(l0, l1, NULL);
    pause(70);
}

// Verstecken: GES + Expertise against the imps' perception.
static void hide(u8 m)
{
    Character *c = &party.members[m];
    const char *label;
    u8 r = d20(0, &label);
    u8 total = r + c->dex + EXPERTISE_BONUS;
    hidden[m] = total >= HIDE_DC;
    char l0[32], l1[32];
    sprintf(l0, "%s versteckt sich:", c->name);
    sprintf(l1, "%s %d + %d = %d: %s", label, r, c->dex + EXPERTISE_BONUS, total, hidden[m] ? "Erfolg!" : "entdeckt.");
    show(l0, l1, hidden[m] ? "Unsichtbar für die Feinde." : NULL);
    pause(80);
}

// ---------------------------------------------------------------- spells

// Feuerpfeil (1W10) and Kältestrahl (1W8, slows): spell attacks with intl as the bonus.
static void cantrip(u8 m, bool frost)
{
    Character *c = &party.members[m];
    u8 t = chooseTarget(!frost);                         // fire lights the tank, frost doesn't
    if (t == TANK_TARGET)
    {
        attackTank(c, c->intl, FALSE);
        return;
    }
    Foe *e = &foes[t];
    char l0[32], l1[32], l2[32];
    sfx_play(frost ? SFX_SPELL : SFX_FIRE);
    const char *label;
    u8 r = d20((e->status & FS_ASLEEP) ? 1 : 0, &label);
    u8 total = r + c->intl;
    bool hit = r == 20 || (r != 1 && total >= e->def->ac);
    sprintf(l0, frost ? "Kältestrahl auf %s!" : "Feuerpfeil auf %s!", e->name);
    sprintf(l1, "%s %d + %d = %d: %s", label, r, c->intl, total, hit ? "Treffer!" : "daneben.");
    show(l0, l1, NULL);
    pause(40);
    if (!hit)
    {
        pause(40);
        return;
    }
    u8 dmg = frost ? roll(8) : roll(10);
    if (r == 20) dmg += frost ? roll(8) : roll(10);
    bool fell = damageFoe(t, dmg);
    if (frost && !fell)
    {
        e->status |= FS_SLOWED;
        sprintf(l2, "%d Schaden, verlangsamt!", dmg);
    }
    else
        sprintf(l2, frost ? "%d Kälteschaden." : "%d Feuerschaden.", dmg);
    show(l0, l1, l2);
    pause(80);
    if (fell) announceFall(l0, l1, t);
}

// Magisches Geschoss: three darts of 1W4 + 1 that never miss; if the target falls, the rest fly
// on to the next enemy.
static void magicMissile(u8 m)
{
    Character *c = &party.members[m];
    u8 t = chooseTarget(FALSE);
    c->mp--;
    uiPanel_redrawChrome();
    sfx_play(SFX_SPELL);
    char l1[32], l2[32];
    for (u8 dart = 1; dart <= 3; dart++)
    {
        if (!foes[t].hp)
        {
            u8 next = 0xFF;
            for (u8 f = 0; f < foeCount; f++)
                if (foes[f].hp) { next = f; break; }
            if (next == 0xFF) return;
            t = next;
        }
        u8 dmg = roll(4) + 1;
        sprintf(l1, "Geschoss %d: %s", dart, foes[t].name);
        bool fell = damageFoe(t, dmg);
        sprintf(l2, fell ? "%d Schaden - fällt!" : "%d Schaden.", dmg);
        show("Magisches Geschoss!", l1, l2);
        pause(60);
    }
}

// Schlaf: 5W8 hit points' worth of enemies fall asleep, weakest first, no saving throw.
static void sleepSpell(u8 m)
{
    Character *c = &party.members[m];
    c->mp--;
    uiPanel_redrawChrome();
    sfx_play(SFX_SPELL);
    u8 pool = 0;
    for (u8 i = 0; i < SLEEP_DICE; i++) pool += roll(8);

    char l0[32], names[2][32];
    u8 asleep = 0;
    names[0][0] = names[1][0] = 0;
    while (TRUE)
    {
        u8 weakest = 0xFF;
        for (u8 f = 0; f < foeCount; f++)
            if (foes[f].hp && !(foes[f].status & FS_ASLEEP) && (weakest == 0xFF || foes[f].hp < foes[weakest].hp))
                weakest = f;
        if (weakest == 0xFF || foes[weakest].hp > pool) break;
        pool -= foes[weakest].hp;
        foes[weakest].status |= FS_ASLEEP;
        if (asleep < 2) strcpy(names[asleep], foes[weakest].name);
        asleep++;
    }
    if (!asleep)
        show("Schlaf wogt heran...", "Niemand schläft ein.", NULL);
    else if (asleep == 1)
    {
        sprintf(l0, "%s schläft ein.", names[0]);
        show("Schlaf wogt heran...", l0, NULL);
    }
    else
    {
        char l1[32], l2[32];
        sprintf(l1, "%s, %s", names[0], names[1]);
        sprintf(l2, asleep > 2 ? "und alle anderen schlafen!" : "schlafen ein.");
        show("Schlaf wogt heran...", l1, l2);
    }
    pause(100);
}

// Schriftrolle: Brennende Hände, 3W6 fire to every enemy, a GES save halves it. Anyone can read it.
static void burningHands(u8 m)
{
    inventory_takeItem(ITEM_SCROLL);
    uiPanel_drawInventory();
    char l0[32], l1[32];
    sfx_play(SFX_FIRE);
    sprintf(l0, "%s liest die Rolle:", party.members[m].name);
    show(l0, "Brennende Hände!", NULL);
    pause(50);
    u8 dmg = roll(6) + roll(6) + roll(6);
    for (u8 f = 0; f < foeCount; f++)
    {
        if (!foes[f].hp) continue;
        bool saved = roll(20) + 2 >= SCROLL_DC;
        u8 d = saved ? dmg / 2 : dmg;
        bool fell = damageFoe(f, d);
        sprintf(l1, fell ? "%s: %d - fällt!" : saved ? "%s: %d (halbiert)" : "%s: %d Schaden", foes[f].name, d);
        show("Flammen fächern auf!", l1, NULL);
        pause(60);
    }
}

static void mageArmor(u8 m)
{
    Character *c = &party.members[m];
    sfx_play(SFX_SPELL);
    ab_mageArmor(c);
    char l1[32];
    sprintf(l1, "%s: RK %d.", c->name, ab_armorClass(c));
    show("Magierrüstung umhüllt", "den Magier schimmernd.", l1);
    pause(80);
}

// ---------------------------------------------------------------- paladin

static void layOnHands(u8 m)
{
    Character *c = &party.members[m];
    Character *t = ab_pickMember("Wen heilen?");
    sfx_play(SFX_HEAL);
    char l1[32], l2[32];
    sprintf(l1, "%s erhält %d KP.", t->name, ab_layOnHands(c, t));
    sprintf(l2, "(Noch %d im Pool.)", c->mp);
    show("Heilende Hände:", l1, l2);
    pause(90);
}

// Göttlicher Sinn in a fight: the enemies are fiends, and she senses how hurt they are.
static void divineSense(void)
{
    sfx_play(SFX_SPELL);
    char lines[3][32];
    const char *out[3] = { NULL, NULL, NULL };
    u8 n = 0;
    for (u8 f = 0; f < foeCount && n < 3; f++)
    {
        if (!foes[f].hp) continue;
        sprintf(lines[n], "%s: %d/%d KP, Teufel", foes[f].name, foes[f].hp, foes[f].def->hpMax);
        out[n] = lines[n];
        n++;
    }
    show(out[0], out[1], out[2]);
    pause(150);
}

// Befehl (the BG3 trick on the Nautiloid): one word the foe must obey unless it makes a WEI save.
// "Fallenlassen!" on Zhalk drops the Everburn Blade -- and the party grabs it.
typedef enum { WORD_DROP, WORD_GROVEL, WORD_FLEE } Word;

static void command(u8 m, u8 f, Word word)
{
    Character *c = &party.members[m];
    Foe *e = &foes[f];
    usedCommand[m] = TRUE;
    static const char *const shout[3] = { "\"FALLENLASSEN!\"", "\"NIEDER!\"", "\"FLIEH!\"" };
    char l0[32], l1[32], l2[32];
    sprintf(l0, "%s: %s", c->name, shout[word]);
    sfx_play(SFX_SPELL);
    u8 r = roll(20);
    s16 total = r + e->def->wis;
    bool resists = total >= COMMAND_DC;
    sprintf(l1, "%s WEI %d%+d = %d", e->name, r, e->def->wis, total);
    if (resists)
    {
        sprintf(l2, "%s widersteht!", e->name);
        show(l0, l1, l2);
        pause(110);
        return;
    }
    switch (word)
    {
        case WORD_DROP:
            if (e->def->sprite == &fig_hound_sprite)
            {
                sprintf(l2, "%s hat nichts im Maul.", e->name);
                break;
            }
            if (e->status & FS_DISARMED)
            {
                sprintf(l2, "%s hat nichts mehr.", e->name);
                break;
            }
            e->status |= FS_DISARMED;
            if (e->def == &ENEMY_ZHALK)
            {
                SPR_setFrame(e->spr, ZHALK_UNARMED);
                stopFire();
                sprintf(l2, "Die Klinge fällt klirrend!");
                show(l0, l1, l2);
                sfx_play(SFX_ITEM);
                pause(110);
                u8 g = m;                         // a fighter grabs it if there's one
                for (u8 i = 0; i < PARTY_MAX; i++)
                    if (party.members[i].active && party.members[i].hp && ab_isFighter(&party.members[i]))
                    {
                        g = i;
                        break;
                    }
                inventory_addEquip(EQ_EVERBURN);
                uiPanel_redrawChrome();
                sprintf(l0, "%s schnappt sich", party.members[g].name);
                show(l0, "die Klinge! Zhalk tobt:", "\"GIB SIE HER, WURM!\"");
                pause(150);
                return;
            }
            sprintf(l2, "%s lässt die Waffe fallen.", e->name);
            break;
        case WORD_GROVEL:
            e->status |= FS_PRONE;
            sprintf(l2, "%s wirft sich zu Boden.", e->name);
            break;
        default:                                  // it runs off the bridge -- no loot from it
            e->hp = 0;
            foeFled = TRUE;
            SPR_setVisibility(e->spr, HIDDEN);
            sprintf(l2, "%s flieht Hals über Kopf!", e->name);
            break;
    }
    show(l0, l1, l2);
    pause(110);
}

// ---------------------------------------------------------------- the party's turn

typedef enum
{
    ACT_ATTACK, ACT_FEATURES, ACT_CANTRIPS, ACT_SPELLS, ACT_POTION, ACT_DEFEND,
    ACT_CLEAVE, ACT_TOPPLE, ACT_SECOND_WIND, ACT_HIDE, ACT_FIRE_BOLT, ACT_RAY_OF_FROST,
    ACT_MISSILE, ACT_SLEEP, ACT_MAGE_ARMOR, ACT_LAY_ON_HANDS, ACT_DIVINE_SENSE, ACT_ITEMS,
    ACT_SCROLL, ACT_COMMAND, ACT_DROP, ACT_GROVEL, ACT_FLEE_WORD, ACT_TACTICS, ACT_RUN, ACT_BACK
} Action;

typedef struct
{
    const char *options[4];
    Action acts[4];
    u8 n;
} Menu;

static void add(Menu *menu, const char *option, Action act)
{
    if (menu->n >= 4) return;
    menu->options[menu->n] = option;
    menu->acts[menu->n++] = act;
}

static Action ask(const char *prompt, Menu *menu)
{
    const char *lines[1] = { prompt };
    return menu->acts[textbox_show(lines, 1, menu->options, menu->n)];
}

static void featureMenu(u8 m, Menu *menu)
{
    Character *c = &party.members[m];
    menu->n = 0;
    if (ab_isFighter(c))
    {
        if (!usedCleave[m]) add(menu, "Spalten", ACT_CLEAVE);
        if (!usedTopple[m]) add(menu, "Niederwerfen", ACT_TOPPLE);
        if (!usedSecondWind[m]) add(menu, "Erholen (Bonusaktion)", ACT_SECOND_WIND);
    }
    else if (c->cls == CLASS_ROGUE)
        add(menu, "Verstecken", ACT_HIDE);
    else if (c->cls == CLASS_SHADOWHEART)
    {
        if (c->mp) add(menu, "Heilende Hände", ACT_LAY_ON_HANDS);
        add(menu, "Göttlicher Sinn", ACT_DIVINE_SENSE);
        if (!usedCommand[m]) add(menu, "Befehl", ACT_COMMAND);
    }
    add(menu, "Zurück", ACT_BACK);
}

static bool hasFeatures(u8 m)
{
    Character *c = &party.members[m];
    if (ab_isFighter(c)) return !usedCleave[m] || !usedTopple[m] || !usedSecondWind[m];
    return c->cls == CLASS_ROGUE || c->cls == CLASS_SHADOWHEART;
}

// Returns TRUE if the action used up the turn (FALSE: back to the main menu, or a bonus action).
static bool perform(u8 m, Action act)
{
    Character *c = &party.members[m];
    Menu sub;
    char l0[32];
    switch (act)
    {
        case ACT_ATTACK:       weaponAttack(m, chooseTarget(TRUE), STRIKE_NORMAL); return TRUE;
        case ACT_FEATURES:
            featureMenu(m, &sub);
            return perform(m, ask("Welche Fähigkeit?", &sub));
        case ACT_CANTRIPS:
            sub.n = 0;
            add(&sub, "Feuerpfeil (1W10)", ACT_FIRE_BOLT);
            add(&sub, "Kältestrahl (1W8)", ACT_RAY_OF_FROST);
            add(&sub, "Zurück", ACT_BACK);
            return perform(m, ask("Welcher Zaubertrick?", &sub));
        case ACT_SPELLS:
            sub.n = 0;
            add(&sub, "Mag. Geschoss (1 ZP)", ACT_MISSILE);
            add(&sub, "Schlaf (1 ZP)", ACT_SLEEP);
            if (!(c->buffs & BUFF_MAGE_ARMOR)) add(&sub, "Magierrüstung (1 ZP)", ACT_MAGE_ARMOR);
            add(&sub, "Zurück", ACT_BACK);
            return perform(m, ask("Welcher Zauber?", &sub));
        case ACT_ITEMS:
            sub.n = 0;
            if (inventory.healingPotions) add(&sub, "Heiltrank", ACT_POTION);
            if (inventory_hasItem(ITEM_SCROLL)) add(&sub, "Schriftrolle (Feuer)", ACT_SCROLL);
            add(&sub, "Zurück", ACT_BACK);
            return perform(m, ask("Welcher Gegenstand?", &sub));
        case ACT_SCROLL:       burningHands(m); return TRUE;
        case ACT_CLEAVE:       cleave(m); return TRUE;
        case ACT_TOPPLE:       usedTopple[m] = TRUE; weaponAttack(m, chooseTarget(FALSE), STRIKE_TOPPLE); return TRUE;
        case ACT_SECOND_WIND:  secondWind(m); return FALSE;   // bonus action: the turn goes on
        case ACT_HIDE:         hide(m); return TRUE;
        case ACT_FIRE_BOLT:    cantrip(m, FALSE); return TRUE;
        case ACT_RAY_OF_FROST: cantrip(m, TRUE); return TRUE;
        case ACT_MISSILE:      magicMissile(m); return TRUE;
        case ACT_SLEEP:        sleepSpell(m); return TRUE;
        case ACT_MAGE_ARMOR:   mageArmor(m); return TRUE;
        case ACT_LAY_ON_HANDS: layOnHands(m); return TRUE;
        case ACT_DIVINE_SENSE: divineSense(); return TRUE;
        case ACT_COMMAND:
            sub.n = 0;
            add(&sub, "\"Fallenlassen!\"", ACT_DROP);
            add(&sub, "\"Nieder!\"", ACT_GROVEL);
            add(&sub, "\"Flieh!\"", ACT_FLEE_WORD);
            add(&sub, "Zurück", ACT_BACK);
            return perform(m, ask("Welcher Befehl?", &sub));
        case ACT_DROP:         command(m, chooseTarget(FALSE), WORD_DROP); return TRUE;
        case ACT_GROVEL:       command(m, chooseTarget(FALSE), WORD_GROVEL); return TRUE;
        case ACT_FLEE_WORD:    command(m, chooseTarget(FALSE), WORD_FLEE); return TRUE;
        case ACT_TACTICS:
            sub.n = 0;
            add(&sub, "Abwehr", ACT_DEFEND);
            add(&sub, "Fliehen", ACT_RUN);
            add(&sub, "Zurück", ACT_BACK);
            return perform(m, ask("Abwehr oder Flucht?", &sub));
        case ACT_RUN:
            fled = TRUE;
            sprintf(l0, "%s ruft: \"Rückzug!\"", c->name);
            show(l0, "Ihr weicht zurück.", NULL);
            pause(80);
            return TRUE;
        case ACT_POTION:
        {
            Character *t = ab_pickMember("Wer bekommt den Heiltrank?");
            sfx_play(SFX_HEAL);
            char l1[32];
            sprintf(l0, "%s nutzt einen Trank:", c->name);
            sprintf(l1, "%s erhält %d KP.", t->name, ab_potion(t));
            show(l0, l1, NULL);
            pause(90);
            return TRUE;
        }
        case ACT_DEFEND:
            defending[m] = TRUE;                         // +2 AC until this member's next turn
            sprintf(l0, "%s geht in Deckung.", c->name);
            show(l0, NULL, NULL);
            pause(60);
            return TRUE;
        default:
            return FALSE;                                // Zurück
    }
}

static void partyTurn(u8 m)
{
    Character *c = &party.members[m];
    defending[m] = FALSE;
    shielded[m] = FALSE;

    char l0[32];
    sprintf(l0, "%s ist am Zug.", c->name);
    while (foesLeft())
    {
        Menu menu = { .n = 0 };
        if (c->cls == CLASS_MAGE)
        {
            add(&menu, "Zaubertrick", ACT_CANTRIPS);
            if (c->mp) add(&menu, "Zauber", ACT_SPELLS);
        }
        else
        {
            add(&menu, "Angriff", ACT_ATTACK);
            if (hasFeatures(m)) add(&menu, "Fähigkeit", ACT_FEATURES);
        }
        if (inventory.healingPotions || inventory_hasItem(ITEM_SCROLL)) add(&menu, "Gegenstand", ACT_ITEMS);
        if (fleeAllowed)
            add(&menu, "Abwehr/Flucht", ACT_TACTICS);
        else
            add(&menu, "Abwehr", ACT_DEFEND);
        if (perform(m, ask(l0, &menu))) return;
    }
}

// ---------------------------------------------------------------- the enemies' turn

// Schild: when an attack would hit the Magier and +5 AC would stop it, he may spend a slot.
static bool offerShield(u8 m, u8 total)
{
    Character *c = &party.members[m];
    if (c->cls != CLASS_MAGE || !c->mp || total >= memberAC(m) + 5) return FALSE;
    const char *lines[2] = { "Ein Treffer droht!", "Schild wirken? (1 ZP)" };
    const char *options[2] = { "Ja", "Nein" };
    if (textbox_show(lines, 2, options, 2)) return FALSE;
    c->mp--;
    shielded[m] = TRUE;
    sfx_play(SFX_SPELL);
    uiPanel_redrawChrome();
    return TRUE;
}

// One attack of foe f against a random party member it can see.
static void foeAttack(u8 f)
{
    Foe *e = &foes[f];
    char l0[32], l1[32], l2[32];
    u8 targets[PARTY_MAX], n = 0;
    for (u8 i = 0; i < PARTY_MAX; i++)
        if (party.members[i].active && party.members[i].hp && !hidden[i]) targets[n++] = i;
    if (!n)
    {
        sprintf(l0, "%s sucht vergeblich.", e->name);
        show(l0, NULL, NULL);
        pause(60);
        return;
    }
    u8 m = targets[dice_roll(n) - 1];
    Character *c = &party.members[m];

    const char *label;
    u8 r = d20((e->status & FS_SLOWED) ? -1 : 0, &label);
    e->status &= ~FS_SLOWED;
    u8 total = r + e->def->atk;
    bool hit = r == 20 || (r != 1 && total >= memberAC(m));
    sprintf(l0, "%s greift %s an.", e->name, c->name);
    sprintf(l1, "%s %d + %d = %d: %s", label, r, e->def->atk, total, hit ? "Treffer!" : "daneben.");
    show(l0, l1, NULL);
    if (!hit) sfx_play(SFX_MISS);
    pause(40);
    if (hit && r != 20 && offerShield(m, total))
    {
        show(l0, l1, "Schild! Angriff geblockt.");
        pause(80);
        return;
    }
    if (!hit)
    {
        pause(40);
        return;
    }
    u8 die = (e->status & FS_DISARMED) ? 4 : e->def->dmgDie;
    u8 dmg = roll(die) + e->def->dmgBonus;
    if (e->def->fireDie && !(e->status & FS_DISARMED)) dmg += roll(e->def->fireDie);
    if (r == 20) dmg += roll(die);
    c->hp = dmg >= c->hp ? 0 : c->hp - dmg;
    sfx_play(SFX_HURT);
    figures_shakeView();
    uiPanel_redrawChrome();
    sprintf(l2, c->hp ? "%d Schaden." : "%d Schaden. %s fällt!", dmg, c->name);
    show(l0, l1, l2);
    pause(90);
}

static void foeTurn(u8 f)
{
    Foe *e = &foes[f];
    char l0[32];
    if (e->status & FS_ASLEEP)
    {
        sprintf(l0, "%s schläft tief.", e->name);
        show(l0, NULL, NULL);
        pause(50);
        return;
    }
    if (e->status & FS_PRONE)
    {
        e->status &= ~FS_PRONE;
        sprintf(l0, "%s rappelt sich auf.", e->name);
        show(l0, NULL, NULL);
        pause(60);
        return;
    }

    for (u8 k = 0; k < e->def->attacks && partyStanding(); k++)
        foeAttack(f);
}

// ---------------------------------------------------------------- the fight

static void releaseFigures(void)
{
    for (u8 i = 0; i < foeCount; i++)
        figures_release(foes[i].spr);
    figures_release(arrow);
    SPR_update();
}

static void gameOver(void)
{
    releaseFigures();
    stopFire();
    ab_gameOver("Deine Gruppe ist gefallen.", "Der Nautiloid stürzt", "weiter durch Avernus...");
}

// No permanent death outside a total defeat: whoever fell gets up with 1 KP.
static bool reviveFallen(void)
{
    bool revived = FALSE;
    for (u8 i = 0; i < PARTY_MAX; i++)
        if (party.members[i].active && !party.members[i].hp)
        {
            party.members[i].hp = 1;
            revived = TRUE;
        }
    return revived;
}

bool combat_run(const Player *p, const EnemyDef *const enemies[], u8 count, RoomObject *tank, u8 goldReward,
                bool canFlee)
{
    viewer = p;
    fled = foeFled = FALSE;
    fleeAllowed = canFlee;
    memset(usedCommand, 0, sizeof(usedCommand));
    tankObj = tank;
    foeCount = count > COMBAT_MAX_ENEMIES ? COMBAT_MAX_ENEMIES : count;
    memset(defending, 0, sizeof(defending));
    memset(hidden, 0, sizeof(hidden));
    memset(shielded, 0, sizeof(shielded));
    memset(usedSecondWind, 0, sizeof(usedSecondWind));
    memset(usedCleave, 0, sizeof(usedCleave));
    memset(usedTopple, 0, sizeof(usedTopple));

    for (u8 i = 0; i < foeCount; i++)
    {
        Foe *e = &foes[i];
        e->def = enemies[i];
        e->hp = e->def->hpMax;
        e->status = 0;
        e->cx = VIEW_CENTER_X + (2 * i - (foeCount - 1)) * FOE_SPACING / 2;
        e->spr = figures_add(e->def->sprite, e->cx, e->def->bottom);
        if (e->def == &ENEMY_ZHALK && equip_partyHas(EQ_EVERBURN))   // it was taken from him
        {
            e->status = FS_DISARMED;
            SPR_setFrame(e->spr, ZHALK_UNARMED);
        }
        if (foeCount > 1)
            sprintf(e->name, "%s %c", e->def->name, 'A' + i);
        else
            strcpy(e->name, e->def->name);
    }
    // The marker uses colour 15 of whatever figure palette is loaded (its own isn't loaded).
    arrow = figures_addPlain(&fig_arrow_sprite, 0, 0, PAL2);
    SPR_setVisibility(arrow, HIDDEN);

    bool burning = FALSE;
    for (u8 i = 0; i < foeCount; i++) burning |= foes[i].def->burning && !(foes[i].status & FS_DISARMED);
    PAL_getColors(FIRE_INDEX, fireColours, 3);
    if (burning)
    {
        fireTimer = firePhase = 0;
        SYS_setVBlankCallback(cycleFire);
    }

    char l0[32];
    sprintf(l0, "Kampf gegen %d Gegner!", foeCount);
    show(l0, NULL, NULL);
    pause(70);

    // Initiative: d20 + DEX for the party, d20 + 2 for enemies, rolled once per fight.
    u8 order[PARTY_MAX + COMBAT_MAX_ENEMIES], init[PARTY_MAX + COMBAT_MAX_ENEMIES], n = 0;
    for (u8 i = 0; i < PARTY_MAX; i++)
        if (party.members[i].active)
        {
            order[n] = i;
            init[n++] = roll(20) + party.members[i].dex;
        }
    for (u8 i = 0; i < foeCount; i++)
    {
        order[n] = PARTY_MAX + i;
        init[n++] = roll(20) + 2;
    }
    for (u8 i = 1; i < n; i++)          // insertion sort, highest first
        for (u8 j = i; j > 0 && init[j] > init[j - 1]; j--)
        {
            u8 t = init[j]; init[j] = init[j - 1]; init[j - 1] = t;
            t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
        }

    while (TRUE)
    {
        if (roundHook) roundHook();
        for (u8 i = 0; i < n; i++)
        {
            u8 who = order[i];
            if (who < PARTY_MAX)
            {
                if (party.members[who].hp) partyTurn(who);
            }
            else if (foes[who - PARTY_MAX].hp)
                foeTurn(who - PARTY_MAX);

            if (!partyStanding()) gameOver();
            if (!foesLeft()) goto won;
            if (fled) goto away;
        }
    }

away:
    releaseFigures();
    stopFire();
    reviveFallen();
    uiPanel_redrawChrome();
    show(NULL, NULL, NULL);                      // clears the message area
    return FALSE;

won:
    releaseFigures();
    stopFire();
    bool revived = reviveFallen();
    inventory_addGold(goldReward);
    uiPanel_redrawChrome();

    sfx_play(SFX_VICTORY);
    char l1[32];
    sprintf(l1, "Beute: %d Gold.", goldReward);
    say("Sieg!", goldReward ? l1 : NULL, revived ? "Bewusstlose kommen zu sich." : NULL);
    return TRUE;
}
