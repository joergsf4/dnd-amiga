# D&D Dungeon Crawler (Amiga)

Amiga port of the Mega Drive dungeon crawler [dnd](https://github.com/joergsf4/dnd)
(grid-based first-person, *Eye of the Beholder* / *Dungeon Master* style, "Nautiloid Prologue"
vertical slice). Target machine: **Amiga 500 with 1 MB** — 68000, OCS, Kickstart 1.3,
512 KB chip + 512 KB slow RAM — booting from one double-density floppy like the games of the era.
It uses about 210 KB of the disk's 880 KB.

The whole prologue plays: intro, title, character creation, all six rooms with their scenes,
fights and the countdown, the end screen, and "new game". The plan, the memory budget and how
the Mega Drive's 1.28 MB fit on the floppy are in [AMIGA_PORT.md](AMIGA_PORT.md), together with
where the port differs from the Mega Drive version.

## Controls

| Mega Drive | Joystick (port 2) | Keyboard |
|---|---|---|
| D-pad (walk/turn) | directions | cursor keys, W A S D |
| A (use) / Start | fire | Q |
| B (party) | second fire button | E |
| C (map) | – | M |

## Toolchain

Same idea as the Mega Drive projects in this workspace: everything that compiles runs in Docker,
the emulator runs on the Mac.

- **Bebbo's amiga-gcc** (m68k-amigaos-gcc 6.5, vasm, NDK 1.3 + 3.x headers, libnix, xdftool)
  via the [sacredbanana/amiga-compiler](https://hub.docker.com/r/sacredbanana/amiga-compiler)
  image, plus Pillow for the asset tools and the [ZX0](https://github.com/einar-saukas/ZX0)
  compressor (`Dockerfile`). Multi-arch, so it runs natively on Apple Silicon.
- **FS-UAE** to run it: `brew install --cask fs-uae-emulator`.
- `./build.sh` — builds in the container (`./build.sh clean` to clean):
  - `out/dnd` — the executable (code only),
  - `out/dnd.pak` — all data: graphics, sounds, music, the view's sources; ZX0-packed, loaded
    once at start-up into memory blocks of their own, in chip RAM where needed,
  - `out/dnd.adf` — the bootable floppy with both,
  - `out/hd/` — the same files as a folder, for booting them as a hard drive (fast tests).
  `tools/make_res.py` makes the pak and the generated sources in `out/gen/` (the counterpart of
  SGDK's rescomp). Debug switches: `./build.sh DEFS="-DVIEW_TIMING"` (see below).
- `./run.sh` — boots `out/dnd.adf` in FS-UAE as an A500 (`amiga/a500.fs-uae`). Extra FS-UAE
  options can be appended, e.g. `./run.sh --amiga_model=A1200`.
- `.vscode/tasks.json` — build / clean / run (Cmd+Shift+B to build).

### Kickstart

Put a Kickstart 1.3 ROM (any name ending in `.rom`, unencrypted) into `roms/`: `run.sh` and the
test tools use it. ROMs are gitignored. Without one, FS-UAE falls back to its free AROS
replacement, which takes 400-500 KB of the A500's memory itself; the machine gets 1 MB of fast
RAM on top then, but test with the real Kickstart 1.3 -- that's the target.

## Testing without looking at a window

- `tools/emutest.sh STEP ...` — plays the game headlessly: FS-UAE on a virtual display in a second
  Docker image (`docker/emutest.Dockerfile`), from `out/hd` (`ADF=1`: from the floppy, at real
  speed). Steps: `@T` / `+T` wait, `key:K` press a key, `wait:TEXT` wait for a line of the game's
  serial output, `shot:NAME` screenshot. Results in `out/emutest/`: the screenshots, the game's
  debug output (`serial.txt`, from `kprintf`) and the sound (`audio.wav`).
- `tools/scenario.py SCENARIO` — the routes of dnd's `tools/emutest.py` on the Amiga: `create`,
  `look`, `room1`, `room3` (to Lae'zel and the first fight), `room6` (to the bridge), `ending`
  (the end screen and a new game), `map`.
- `tools/viewcheck.py` — with `./build.sh DEFS="-DVIEW_DUMP"` the game sends every rendered view
  over the serial port; this renders the same cells with `make_view.py`'s reference renderer and
  compares them pixel by pixel.
- `tools/audiocheck.py [from to]` — loudness and strongest frequencies of `audio.wav` per half
  second: are the music's notes the right ones?
- `DEFS="-DVIEW_TIMING"` logs the frames each view render takes (drawing, c2p, display) and what
  gets baked; `DEFS="-DKEY_TRACE"` logs the keyboard.

## Project structure

```
Dockerfile                 Build image: amiga-gcc + Pillow + zx0
docker/emutest.Dockerfile  Headless test image: FS-UAE + Xvfb + xdotool + ImageMagick
Makefile                   Build rules (runs inside the container)
build.sh / run.sh          Build in Docker / run in FS-UAE
amiga/a500.fs-uae          The emulated machine
amiga/startup-sequence     Copied to s/ on the disk: starts the game
src/                       The game, copied from dnd (see AMIGA_PORT.md for what changed)
src/amiga/                 The Amiga layer:
  genesis.h                  the part of SGDK's API the game uses, for the Amiga
  hw.c, isr.s                start-up, data file, taking the machine over, interrupts, memory
  vdp.c, spr.c               the VDP's tile planes and sprites: composed into a 5-bitplane screen
  joy.c                      joystick and keyboard as the joypad
  audio.c                    Paula: the PSG songs and the sound effects
  view_bake.c, view_draw.s   the view: baking walls and props, drawing, chunky to planar
  unzx0.c                    unpacking the data file
res/                       Asset sources from dnd (PNGs, WAVs)
tools/                     Asset generators (from dnd, plus make_res.py), test tools
roms/                      Kickstart ROMs (local only)
```
