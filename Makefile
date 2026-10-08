# Runs inside the dnd-amiga-devkit container (see build.sh).
# Target: Amiga 500, 68000, OCS, Kickstart 1.3+, 512 KB chip + 512 KB slow RAM.

CC      = m68k-amigaos-gcc
CFLAGS  = -mcrt=nix13 -m68000 -Os -fomit-frame-pointer -fno-delete-null-pointer-checks -std=gnu99 \
          -Wall -Wextra -Wno-unused-parameter \
          -Isrc/amiga -Isrc -Iout/gen $(DEFS)
LDFLAGS = -mcrt=nix13 -s

# Debug switches (./build.sh DEFS="-DVIEW_TIMING ..."): out/defs changes when they do, and
# everything that's compiled depends on it.
$(shell mkdir -p out; echo '$(DEFS)' > out/defs.new; cmp -s out/defs.new out/defs || cp out/defs.new out/defs)

GEN  = out/gen/resources.c out/gen/view_data.c
SRC  = $(wildcard src/*.c) $(wildcard src/amiga/*.c)
ASM  = $(wildcard src/amiga/*.s)
OBJ  = $(patsubst %.c,out/obj/%.o,$(SRC)) $(patsubst %.s,out/obj/%.o,$(ASM)) \
       out/obj/gen/resources.o out/obj/gen/view_data.o

EXE  = out/dnd
PAK  = out/dnd.pak
ADF  = out/dnd.adf

all: $(ADF) out/hd/.stamp

# Assets: tools/make_res.py writes the pak and the generated sources in one go.
ASSETS = tools/make_res.py tools/make_view.py $(wildcard res/gfx/*.png) $(wildcard res/sfx/*.wav)
out/gen/.stamp: $(ASSETS)
	@mkdir -p out/gen
	python3 tools/make_res.py
	@touch $@
$(GEN) $(PAK) out/gen/res/resources.h out/gen/view_gen.h: out/gen/.stamp

out/obj/%.o: %.c out/gen/.stamp Makefile out/defs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

# The hot paths for speed -- the Amiga layer (baking, composing) and the view's per-render
# lookups -- the rest of the game for size.
out/obj/src/amiga/%.o: src/amiga/%.c out/gen/.stamp Makefile out/defs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -O2 -MMD -c $< -o $@

out/obj/src/dungeon_view.o out/obj/src/dungeon_map.o: out/obj/src/%.o: src/%.c out/gen/.stamp Makefile out/defs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -O2 -MMD -c $< -o $@

out/obj/gen/%.o: out/gen/%.c out/gen/.stamp Makefile out/defs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

out/obj/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) -m68000 -c $< -o $@

$(EXE): $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@

# Bootable OFS disk: the standard boot block hands over to AmigaDOS, which runs
# s/startup-sequence, which starts the game.
$(ADF): $(EXE) $(PAK) amiga/startup-sequence
	rm -f $@
	xdftool $@ format "DND" + boot install + makedir s \
		+ write amiga/startup-sequence s/startup-sequence + write $(EXE) dnd + write $(PAK) dnd.pak

# The same files as a folder, for booting them as a hard drive in the emulator (tools/emutest.sh):
# much faster than the floppy, which loads at real speed.
out/hd/.stamp: $(EXE) $(PAK) amiga/startup-sequence
	mkdir -p out/hd/s
	cp $(EXE) $(PAK) out/hd/
	cp amiga/startup-sequence out/hd/s/
	@touch $@

clean:
	rm -rf out

-include $(wildcard out/obj/src/*.d out/obj/src/amiga/*.d)

.PHONY: all clean
