#ifndef _ABILITIES_H_
#define _ABILITIES_H_

#include "party.h"
#include "dungeon_map.h"

// D&D 5e / BG3 level-1 class features that matter both in and out of combat, and the party menu
// (B in the dungeon). Combat-only features (Spalten, Hinterhältiger Angriff, Schlaf, ...) live in
// src/combat.c.
//
//   Kämpfer (hero, Lae'zel): Erholen, Spalten, Niederwerfen; Kampfstil Verteidigung (hero, in
//                            the AC) / Großwaffen (Lae'zel)
//   Schurke: Hinterhältiger Angriff, Verstecken, Expertise (+2 on GES checks, src/skill_check.c)
//   Magier:  Feuerpfeil, Kältestrahl (cantrips); Magisches Geschoss, Schlaf, Magierrüstung,
//            Schild (2 slots); Magierhand (Room 1's larva pool)
//   Paladin (Schattenherz): Heilende Hände (pool of 5), Göttlicher Sinn, heavy armour

#define BUFF_MAGE_ARMOR 0x01   // AC 13 + GES, lasts the whole game once cast (8 hours in 5e)

#define EXPERTISE_BONUS 2
#define LAY_ON_HANDS_POOL 5

bool ab_isFighter(const Character *c);   // the hero as Kämpfer, and Lae'zel
u8 ab_armorClass(const Character *c);    // from the equipment, including Magierrüstung

// Party member picker (menu over the message area), NULL-free: returns the chosen member.
Character *ab_pickMember(const char *question);

// These apply the effect and return what the caller needs for its message.
u8 ab_potion(Character *target);                        // 2W4+2 KP, revives the fallen
u8 ab_layOnHands(Character *paladin, Character *target); // up to the pool, revives the fallen
void ab_mageArmor(Character *mage);                     // one slot

// "Deine Gruppe ist gefallen" and friends: a game-over box, then a restart. Never returns.
void ab_gameOver(const char *l0, const char *l1, const char *l2);

// The party menu on B in the dungeon: Ausrüstung, potions, and the class features that work
// outside a fight (Magierrüstung, Heilende Hände, Göttlicher Sinn).
void ab_partyMenu(const Player *p);

#endif
