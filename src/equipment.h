#ifndef _EQUIPMENT_H_
#define _EQUIPMENT_H_

#include <genesis.h>
#include "party.h"

// Equipment, D&D 5e at level 1, kept rudimentary: every party member has three slots -- weapon,
// armour, shield -- which decide their armour class and damage die (equip_recalc). Unequipped
// items sit in the party's backpack (inventory.bag). Proficiencies as in 5e: the Magier only uses
// daggers and quarterstaffs and no armour or shield, the Schurke simple weapons, short sword,
// rapier and longsword and only light armour; the fighters and the paladin use everything;
// "Wir" wears nothing.

typedef enum
{
    EQ_NONE = 0,
    EQ_DAGGER, EQ_QUARTERSTAFF, EQ_SHORTSWORD, EQ_RAPIER, EQ_LONGSWORD, EQ_MACE, EQ_GREATSWORD,
    EQ_EVERBURN,                                   // Zhalk's sword: 1W10 + 1W4 fire
    EQ_LEATHER, EQ_STUDDED, EQ_CHAIN_SHIRT, EQ_HALF_PLATE, EQ_CHAIN_MAIL,
    EQ_SHIELD,
    EQ_COUNT
} EquipId;

typedef enum { SLOT_WEAPON = 0, SLOT_ARMOR, SLOT_SHIELD } Slot;   // EQUIP_SLOTS in party.h

const char *equip_name(u8 id);          // at most 14 characters, "-" for EQ_NONE
u8 equip_slot(u8 id);
bool equip_canUse(const Character *c, u8 id);

// Derives armour class, damage die and fire die from the equipment (and Magierrüstung, and the
// hero Kämpfer's fighting style Verteidigung: +1 AC in armour). Call after every change.
void equip_recalc(Character *c);

// Starting equipment of a class (companions come equipped; the hero finds his in Room 1's chest).
void equip_starting(Character *c);

// The class gear the hero finds in the cartilage chest, into the backpack.
void equip_giveHeroGear(CharClass cls);

// Is this item in the backpack or equipped by anyone?
bool equip_partyHas(u8 id);

// The equipment screen (over the view, the panel stays): pick a member with left/right, a slot
// with up/down and A, then an item from the backpack. B goes back. Starts at party member `m`.
void equip_screen(u8 m);

#endif
