#!/bin/bash
# Boots the built floppy in FS-UAE as an Amiga 500 (see amiga/a500.fs-uae).
# Kickstart: the first roms/*.rom (Kickstart 1.3, e.g. from Amiga Forever),
# otherwise FS-UAE's built-in AROS replacement ROM.
set -euo pipefail
cd "$(dirname "$0")"
ADF="out/dnd.adf"
if [ ! -f "$ADF" ]; then
    echo "Disk image not found at $ADF — run ./build.sh first." >&2
    exit 1
fi

# AROS takes 400-500 KB of the A500's memory itself (Kickstart 1.3: ~40 KB): with it, the machine
# gets 1 MB of fast RAM on top so that the game fits.
KICK="internal"
EXTRA="--fast_memory=1024"
ROM=$(ls roms/*.rom 2>/dev/null | head -1 || true)
if [ -n "$ROM" ]; then KICK="$PWD/$ROM"; EXTRA=""; fi

FSUAE="/Applications/FS-UAE.app/Contents/MacOS/fs-uae"
command -v fs-uae > /dev/null && FSUAE="fs-uae"

"$FSUAE" amiga/a500.fs-uae \
    --floppy_drive_0="$PWD/$ADF" \
    --kickstart_file="$KICK" $EXTRA \
    "$@"
