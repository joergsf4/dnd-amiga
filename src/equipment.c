#include "equipment.h"
#include "inventory.h"
#include "abilities.h"
#include "ui_panel.h"
#include "text.h"
#include "sfx.h"
#include "game.h"

// Every line here is at most 28 characters: the screen covers the view and the message area
// (columns 0-27), the party panel stays on the right.

typedef enum { K_SIMPLE, K_MARTIAL, K_LIGHT, K_MEDIUM, K_HEAVY, K_SHIELD } Kind;

typedef struct
{
    const char *name;
    u8 slot, kind;
    u8 value;      // weapon: damage die; armour: base AC; shield: AC bonus
    u8 fire;       // weapon: extra fire die
    u8 dexMax;     // armour: most of the GES bonus that counts
} EquipDef;

static const EquipDef defs[EQ_COUNT] = {
    [EQ_NONE]        = { "-",             SLOT_WEAPON, K_SIMPLE,  1,  0, 0 },
    [EQ_DAGGER]      = { "DOLCH",         SLOT_WEAPON, K_SIMPLE,  4,  0, 0 },
    [EQ_QUARTERSTAFF]= { "KAMPFSTAB",     SLOT_WEAPON, K_SIMPLE,  6,  0, 0 },
    [EQ_SHORTSWORD]  = { "KURZSCHWERT",   SLOT_WEAPON, K_MARTIAL, 6,  0, 0 },
    [EQ_RAPIER]      = { "RAPIER",        SLOT_WEAPON, K_MARTIAL, 8,  0, 0 },
    [EQ_LONGSWORD]   = { "LANGSCHWERT",   SLOT_WEAPON, K_MARTIAL, 8,  0, 0 },
    [EQ_MACE]        = { "STREITKOLBEN",  SLOT_WEAPON, K_SIMPLE,  6,  0, 0 },
    [EQ_GREATSWORD]  = { "GROSSSCHWERT",  SLOT_WEAPON, K_MARTIAL, 12, 0, 0 },
    [EQ_EVERBURN]    = { "IMMERBRAND",    SLOT_WEAPON, K_MARTIAL, 10, 4, 0 },
    [EQ_LEATHER]     = { "LEDERRÜSTUNG",  SLOT_ARMOR,  K_LIGHT,   11, 0, 9 },
    [EQ_STUDDED]     = { "BESCHL. LEDER", SLOT_ARMOR,  K_LIGHT,   12, 0, 9 },
    [EQ_CHAIN_SHIRT] = { "KETTENHEMD",    SLOT_ARMOR,  K_MEDIUM,  13, 0, 2 },
    [EQ_HALF_PLATE]  = { "HALBPLATTE",    SLOT_ARMOR,  K_MEDIUM,  15, 0, 2 },
    [EQ_CHAIN_MAIL]  = { "KETTENPANZER",  SLOT_ARMOR,  K_HEAVY,   16, 0, 0 },
    [EQ_SHIELD]      = { "SCHILD",        SLOT_SHIELD, K_SHIELD,  2,  0, 0 },
};

const char *equip_name(u8 id)
{
    return defs[id].name;
}

u8 equip_slot(u8 id)
{
    return defs[id].slot;
}

bool equip_canUse(const Character *c, u8 id)
{
    const EquipDef *d = &defs[id];
    switch (c->cls)
    {
        case CLASS_WIR:
            return FALSE;
        case CLASS_MAGE:
            return id == EQ_DAGGER || id == EQ_QUARTERSTAFF;
        case CLASS_ROGUE:
            if (d->slot == SLOT_WEAPON)
                return d->kind == K_SIMPLE || id == EQ_SHORTSWORD || id == EQ_RAPIER || id == EQ_LONGSWORD;
            return d->kind == K_LIGHT;
        default:
            return TRUE;                                  // fighters, the paladin
    }
}

void equip_recalc(Character *c)
{
    if (c->cls == CLASS_WIR)                              // claws and a hard skull
    {
        c->ac = 13;
        c->dmgDie = 6;
        c->fireDie = 0;
        return;
    }
    u8 dexBonus = (c->dex + 1) / 2;                       // the check modifiers are larger than 5e's
    u8 armor = c->equip[SLOT_ARMOR];
    if (armor)
        c->ac = defs[armor].value + (dexBonus < defs[armor].dexMax ? dexBonus : defs[armor].dexMax);
    else
        c->ac = ((c->buffs & BUFF_MAGE_ARMOR) ? 13 : 10) + dexBonus;
    if (c->equip[SLOT_SHIELD]) c->ac += defs[c->equip[SLOT_SHIELD]].value;
    if (c->cls == CLASS_FIGHTER && armor) c->ac += 1;     // fighting style Verteidigung
    c->dmgDie = defs[c->equip[SLOT_WEAPON]].value;        // EQ_NONE: a fist, 1 + bonus
    c->fireDie = defs[c->equip[SLOT_WEAPON]].fire;
}

void equip_starting(Character *c)
{
    memset(c->equip, 0, sizeof(c->equip));
    if (c->cls == CLASS_LAEZEL)
    {
        c->equip[SLOT_WEAPON] = EQ_GREATSWORD;
        c->equip[SLOT_ARMOR] = EQ_HALF_PLATE;
    }
    else if (c->cls == CLASS_SHADOWHEART)
    {
        c->equip[SLOT_WEAPON] = EQ_MACE;
        c->equip[SLOT_ARMOR] = EQ_CHAIN_MAIL;
        c->equip[SLOT_SHIELD] = EQ_SHIELD;
    }
    equip_recalc(c);
}

void equip_giveHeroGear(CharClass cls)
{
    switch (cls)
    {
        case CLASS_FIGHTER:
            inventory_addEquip(EQ_LONGSWORD);
            inventory_addEquip(EQ_CHAIN_SHIRT);
            inventory_addEquip(EQ_SHIELD);
            break;
        case CLASS_ROGUE:
            inventory_addEquip(EQ_RAPIER);
            inventory_addEquip(EQ_LEATHER);
            inventory_addEquip(EQ_DAGGER);
            break;
        default:
            inventory_addEquip(EQ_QUARTERSTAFF);
            inventory_addEquip(EQ_DAGGER);
            break;
    }
}

bool equip_partyHas(u8 id)
{
    for (u8 i = 0; i < inventory.bagCount; i++)
        if (inventory.bag[i] == id) return TRUE;
    for (u8 i = 0; i < PARTY_MAX; i++)
        for (u8 s = 0; s < EQUIP_SLOTS; s++)
            if (party.members[i].active && party.members[i].equip[s] == id) return TRUE;
    return FALSE;
}

// ---------------------------------------------------------------- the screen

#define ROWS 28
#define COLS 28

static void clearLeft(void)
{
    for (u16 r = 0; r < ROWS; r++)
        text_draw("                            ", 0, r);
}

static void line(const char *s, u16 row)
{
    text_draw("                            ", 0, row);
    text_draw(s, 1, row);
}

static const char *const slotLabel[EQUIP_SLOTS] = { "WAFFE:  ", "RÜSTUNG:", "SCHILD: " };

// The candidates for a slot: indices into the backpack of items this member can use there.
static u8 candidates(const Character *c, u8 slot, u8 *out)
{
    u8 n = 0;
    for (u8 i = 0; i < inventory.bagCount; i++)
        if (equip_slot(inventory.bag[i]) == slot && equip_canUse(c, inventory.bag[i])) out[n++] = i;
    return n;
}

static void draw(u8 m, u8 cursor, bool picking, u8 pick, const u8 *cand, u8 nCand, const char *note)
{
    const Character *c = &party.members[m];
    char s[40];
    sprintf(s, "AUSRÜSTUNG: %s", c->name);
    line(s, 1);
    sprintf(s, "RK %2d   ANGRIFF +%d", c->ac, c->atk);
    line(s, 3);
    if (c->fireDie)
        sprintf(s, "SCHADEN 1W%d+%d +1W%d FEUER", c->dmgDie, c->dmgBonus, c->fireDie);
    else
        sprintf(s, "SCHADEN 1W%d+%d", c->dmgDie, c->dmgBonus);
    line(s, 4);
    for (u8 k = 0; k < EQUIP_SLOTS; k++)
    {
        sprintf(s, "%s %s %s", !picking && cursor == k ? ">" : (picking && cursor == k ? "*" : " "),
                slotLabel[k], equip_name(c->equip[k]));
        line(s, 6 + k);
    }
    line(picking ? "WAS ANLEGEN?" : "RUCKSACK:", 11);
    for (u8 r = 0; r < 12; r++)
    {
        s[0] = 0;
        if (picking)
        {
            if (r < nCand)
                sprintf(s, "%s %s", pick == r ? ">" : " ", equip_name(inventory.bag[cand[r]]));
            else if (r == nCand && c->equip[cursor])
                sprintf(s, "%s (ABLEGEN)", pick == r ? ">" : " ");
        }
        else if (r < inventory.bagCount)
            sprintf(s, "  %s%s", equip_name(inventory.bag[r]), equip_canUse(c, inventory.bag[r]) ? "" : " (-)");
        line(s, 12 + r);
    }
    line(note ? note : "", 24);
    line(picking ? "Q: ANLEGEN   E: ZURÜCK" : "Q: PLATZ WÄHLEN  E: FERTIG", 26);
    line(picking ? "" : "LINKS/RECHTS: FIGUR", 27);
}

static u8 nextMember(u8 m, s8 dir)
{
    for (u8 k = 0; k < PARTY_MAX; k++)
    {
        m = (m + PARTY_MAX + dir) % PARTY_MAX;
        if (party.members[m].active) return m;
    }
    return m;
}

void equip_screen(u8 m)
{
    VDP_clearPlane(BG_B, TRUE);   // the caller redraws the view afterwards
    clearLeft();

    u8 cursor = 0, pick = 0, nCand = 0, cand[INVENTORY_BAG_MAX] = { 0 };
    bool picking = FALSE;
    const char *note = NULL;
    draw(m, cursor, picking, pick, cand, nCand, note);

    u16 prevJoy = JOY_readJoypad(JOY_1);
    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        u16 pressed = joy & ~prevJoy;
        prevJoy = joy;
        bool redraw = FALSE;
        Character *c = &party.members[m];

        if (!picking)
        {
            if (pressed & BUTTON_B) break;
            if (pressed & (BUTTON_LEFT | BUTTON_RIGHT))
            {
                m = nextMember(m, (pressed & BUTTON_LEFT) ? -1 : 1);
                note = NULL;
                redraw = TRUE;
            }
            if (pressed & (BUTTON_UP | BUTTON_DOWN))
            {
                cursor = (cursor + ((pressed & BUTTON_UP) ? EQUIP_SLOTS - 1 : 1)) % EQUIP_SLOTS;
                sfx_play(SFX_MENU);
                redraw = TRUE;
            }
            if (pressed & BUTTON_A)
            {
                nCand = candidates(c, cursor, cand);
                if (c->cls == CLASS_WIR)
                    note = "WIR KANN NICHTS TRAGEN.";
                else if (!nCand && !c->equip[cursor])
                    note = "NICHTS PASSENDES DABEI.";
                else
                {
                    picking = TRUE;
                    pick = 0;
                    note = NULL;
                }
                redraw = TRUE;
            }
        }
        else
        {
            u8 options = nCand + (c->equip[cursor] ? 1 : 0);
            if (pressed & BUTTON_B)
            {
                picking = FALSE;
                redraw = TRUE;
            }
            if (pressed & (BUTTON_UP | BUTTON_DOWN))
            {
                pick = (pick + ((pressed & BUTTON_UP) ? options - 1 : 1)) % options;
                sfx_play(SFX_MENU);
                redraw = TRUE;
            }
            if (pressed & BUTTON_A)
            {
                u8 old = c->equip[cursor];
                if (pick < nCand)                         // equip: the old item goes into the pack
                {
                    u8 bagIndex = cand[pick];
                    c->equip[cursor] = inventory.bag[bagIndex];
                    inventory_removeEquipAt(bagIndex);
                    if (old) inventory_addEquipQuiet(old);
                }
                else if (inventory.bagCount < INVENTORY_BAG_MAX)
                {
                    c->equip[cursor] = EQ_NONE;           // take off
                    inventory_addEquipQuiet(old);
                }
                else
                    note = "DER RUCKSACK IST VOLL.";
                equip_recalc(c);
                sfx_play(SFX_ITEM);
                uiPanel_redrawChrome();
                picking = FALSE;
                redraw = TRUE;
            }
        }
        if (redraw) draw(m, cursor, picking, pick, cand, nCand, note);
        SPR_update();
        SYS_doVBlankProcess();
    }
    clearLeft();
}
