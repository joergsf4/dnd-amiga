#!/usr/bin/env python3
"""Plays scripted routes through the game with tools/emutest.sh and saves screenshots on the way --
the routes of the Mega Drive version's tools/emutest.py (same moves, same order), on the Amiga
keyboard: A and Start are Q, B is E, C is M, the D-pad the cursor keys.

Usage: tools/scenario.py SCENARIO     (screenshots in out/emutest/, debug output in serial.txt)
    create   the creation screen, all three classes
    look     Room 1: looking around from the start
    room1    Room 1: the chest, putting the gear on, the corpse, to the door
    room3    through Rooms 1 and 2 to Room 3: Lae'zel's scene and the imp fight
    room6    on through Rooms 4 and 5 onto the bridge
    ending   on to the transponder, the end screen, START for a new game
    map      the automap in Room 1
"""
import subprocess
import sys
import os

HERE = os.path.dirname(os.path.abspath(__file__))

ACT_WAIT = 0.9          # seconds after a press: a redraw, maybe baking, then the release


class Route:
    def __init__(self):
        self.steps = []

    def act(self, key, wait=ACT_WAIT):
        self.steps += [f"key:{key}", f"+{wait}"]

    def a(self, wait=ACT_WAIT):
        self.act("q", wait)

    def walk(self, n):
        for _ in range(n):
            self.act("Up")

    def left(self):
        self.act("Left")

    def right(self):
        self.act("Right")

    def shot(self, name):
        self.steps.append(f"shot:{name}")

    def wait(self, seconds):
        self.steps.append(f"+{seconds}")

    # ---------------------------------------------------------------- the MD routes

    def create_hero(self, cls_down=0):
        self.steps += ["wait:music: 1", "+1", "key:q", "+2"]   # the title (MUSIC_TITLE)
        for _ in range(cls_down):
            self.act("Down", 0.6)
        self.act("q")
        self.steps.append("wait:music: 2")                  # the first view (MUSIC_DUNGEON)
        self.wait(1)

    def start_game(self, cls_down=0):
        self.create_hero(cls_down)
        self.a()
        self.a()

    def to_room3(self, cls_down=0):
        self.start_game(cls_down)
        self.left()
        self.walk(2)
        self.left()
        self.walk(2)
        self.right()                                        # the chest
        self.a()
        self.a()                                            # "Gleich anlegen?"
        self.a(1.5)                                         # Ja: the equipment screen
        self.shot("equip")
        for k in range(3):
            if k:
                self.act("Down", 0.6)
            self.a()
            self.a()
        self.act("e", 1.5)
        self.right()
        self.walk(2)
        self.right()
        self.walk(4)
        self.right()
        self.walk(2)
        self.left()                                         # door to Room 2
        self.a(2)
        self.shot("room2")
        self.a()                                            # Room 2 intro
        self.right()
        self.walk(1)
        self.left()
        self.walk(5)
        self.left()
        self.walk(1)
        self.right()                                        # door to Room 3
        self.a(2)

    def room3_fight(self):
        for i in range(9):                                  # Lae'zel's scene, the imps' entrance
            self.a(1.2)
            if i in (1, 4, 8):
                self.shot(f"room3_scene{i}")
        self.wait(3)
        for i in range(80):                                 # the fight
            self.a(1.0)
            if i % 10 == 0:
                self.shot(f"fight{i:02d}")


    def to_room6(self):
        """On from Room 3's fight: Room 4, the rune from Room 5, Schattenherz freed, the bridge."""
        self.walk(7)                                        # (2,1)
        self.a(2)                                           # Room 4
        self.shot("room4")
        self.a()                                            # intro
        self.right(); self.walk(2); self.left(); self.walk(3); self.right(); self.walk(1)
        self.a(2)                                           # passage to Room 5
        self.a(); self.a()                                  # Room 5 intro
        self.shot("room5")
        self.walk(1); self.right(); self.walk(1); self.left()
        self.a(); self.a()                                  # the cleric: rune + key
        self.shot("room5_cleric")
        self.left(); self.walk(1); self.left(); self.walk(1)
        self.a(2)                                           # back to Room 4
        self.right(); self.walk(1); self.left(); self.walk(4)
        self.a()                                            # pod console
        self.a(2)                                           # Rune einsetzen
        for i in range(6):                                  # Puff!, her thanks, joining ...
            self.a(1.2)
            if i in (0, 3):
                self.shot(f"room4_shadowheart{i}")
        self.right(); self.walk(1); self.right(); self.walk(1); self.left(); self.walk(1)
        self.a(3)                                           # onto the bridge
        self.shot("room6")


def scenario(name):
    r = Route()
    if name == "create":
        r.steps += ["wait:music: 1", "+1", "key:q", "+2"]
        r.shot("create_fighter")
        r.act("Down", 0.6)
        r.shot("create_rogue")
        r.act("Down", 0.6)
        r.act("Right", 0.6)
        r.shot("create_mage2")
    elif name == "look":
        r.start_game()
        r.shot("look_s")
        for n in ("e", "n", "w"):
            r.left()
            r.shot(f"look_{n}")
    elif name == "room1":
        r.start_game()
        r.left(); r.walk(2); r.left(); r.walk(2); r.right()
        r.shot("r1_chest")
        r.a(); r.shot("r1_chest_text")
        r.a(); r.shot("r1_chest_ask")
        r.a(1.5); r.shot("r1_equip")
        r.act("e", 1.5); r.shot("r1_after_equip")
    elif name == "room3":
        r.to_room3()
        r.shot("room3_door")
        r.room3_fight()
        r.shot("after_fight")
    elif name == "room6":
        r.to_room3()
        r.room3_fight()
        r.to_room6()
        for i in range(6):
            r.a(1.5)
            r.shot(f"room6_{i}")
    elif name == "ending":
        r.to_room3()
        r.room3_fight()
        r.to_room6()
        for _ in range(7):                                  # the bridge's intro
            r.a(1.2)
        r.right(); r.walk(3); r.left(); r.walk(10)          # the east lane, past both groups
        r.left(); r.walk(1); r.right(); r.walk(1)           # (6,1): the cambions
        r.shot("end_cambions")
        r.a()
        r.left(); r.walk(1)                                 # the transponder ahead
        r.a(1.2)
        r.shot("end_transponder")
        r.a(1.5)                                            # Nervenstränge verbinden!
        r.shot("end1")
        r.a(4)
        r.shot("end2")
        r.a(5)
        r.shot("end3")
        r.a(3)
        r.shot("end_screen")
        r.act("q")                                     # START: a new game (the disk runs it again)
        r.steps.append("wait:music: 1:2")                   # the title the second time
        r.wait(1)
        r.shot("restart")
    elif name == "map":
        r.start_game()
        r.act("m", 2)
        r.shot("map")
        r.act("m", 1.5)
        r.shot("map_back")
    else:
        raise SystemExit(__doc__)
    return r.steps


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    sys.exit(subprocess.call([os.path.join(HERE, "emutest.sh")] + scenario(sys.argv[1])))
