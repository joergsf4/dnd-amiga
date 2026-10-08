#ifndef _PARTY_H_
#define _PARTY_H_

#include <genesis.h>

#define PARTY_MAX 4
#define EQUIP_SLOTS 3   // weapon, armour, shield (src/equipment.h)

typedef enum
{
    CLASS_FIGHTER = 0,
    CLASS_ROGUE = 1,
    CLASS_MAGE = 2,
    CLASS_WIR = 3,      // "Wir", the intellect devourer companion from Room 2 (not selectable)
    CLASS_LAEZEL = 4,   // Lae'zel, githyanki warrior, joins in Room 3 (not selectable)
    CLASS_SHADOWHEART = 5 // Schattenherz, paladin here (a cleric in BG3), joins in Room 4
} CharClass;

typedef struct
{
    CharClass cls;
    const char *name;   // auto-assigned: just the class name (see TODO.md for real name entry)
    u8 hp, hpMax;
    u8 mp, mpMax;       // class resource: Magier spell slots ("ZP"), paladin's Heilende-Hände
                        // pool ("HH"); 0/0 for the others (src/abilities.c)
    u8 str, dex, intl;  // minimal attribute block for skill_check.c; WIS/CON/CHA not modeled yet
    u8 ac;              // combat (src/combat.c): armour class an attack roll has to reach...
    u8 atk;             // ...d20 + atk...
    u8 dmgDie, dmgBonus; // ...and on a hit 1d(dmgDie) + dmgBonus damage...
    u8 fireDie;         // ...+ 1d(fireDie) fire (the Everburn Blade); ac, dmgDie and fireDie come
                        // from the equipment (equip_recalc)
    u8 equip[EQUIP_SLOTS]; // EquipId per slot, EQ_NONE = empty
    u8 buffs;           // lasting effects, BUFF_* in src/abilities.h
    u8 portrait;        // the hero's portrait, 0..PORTRAITS_PER_CLASS-1 (src/portraits.c)
    bool active;        // FALSE = empty slot, not yet recruited
} Character;

typedef struct
{
    Character members[PARTY_MAX];
    u8 count;
} Party;

extern Party party;

const char *class_name(CharClass cls);

// Clears the party to all-empty slots.
void party_init(void);

// Appends a new character of the given class with that class's default stats.
// Returns it, or NULL if the party is already full.
Character *party_addMember(CharClass cls);

// Is a (conscious or not) member of this class in the party? For companions' remarks.
bool party_has(CharClass cls);

#endif
